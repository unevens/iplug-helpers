// iplug-helpers — controls/SplineEditorControl.hpp
//
// LCD-style 2-channel spline editor used by both Curvessor and Overdraw —
// a custom IControl that
//   - draws the spline curve(s) over a teal grid + identity diagonal,
//   - lets the user drag knot bodies and per-knot tangent handles,
//   - supports double-click to toggle a knot's enabled / linked state,
//   - drives the host-side automation via Begin/Send/End-InformHostOfParamChange,
//   - hosts scroll-wheel zoom + click-and-drag pan,
//   - paints a live "current input" dot per channel from the audio-thread
//     ISender packets fed through OnMsgFromDelegate.
//
// The two plugins differ only in:
//   - the data range (Curvessor: -96..+6 dB; Overdraw: -2..+2 linear samples);
//   - the knot count (8 vs 15);
//   - the maximum knots the GuiSpline buffer holds (DSP-side constant);
//   - the fixed anchor coordinates at slot 0 (origin pin vs noise-floor pin);
//   - the grid lines + axis label format;
//   - whether channel symmetry exists at all + how it maps to a param (only
//     Overdraw cares — the abs() fold means negative-X knots have no effect,
//     so we neuter them to anchor coords matching the DSP-side helper);
//   - the AmpToX conversion of the live-level packet (Curvessor logs amp→dB,
//     Overdraw uses amp directly as a linear sample value).
//
// Each plugin provides a small `Traits` struct that captures those choices,
// then aliases the templated control:
//
//   struct CurvessorSplineTraits {
//     static constexpr int    kNumKnots = 8;
//     static constexpr int    kFirstKnotParamBase = kKnot1_enabled;
//     static constexpr int    kKnotParamStride    = 10;
//     static constexpr int    kMaxKnotsInSpline   = curvessor::maxNumKnots;
//     static constexpr double kKnotMin = -96.0;
//     static constexpr double kKnotMax = 6.0;
//     static constexpr double kAnchorX = -96.0;
//     static constexpr double kAnchorY = -96.0;
//     static constexpr std::array<double, 5> kMajorGrid =
//       { -96.0, -72.0, -48.0, -24.0, 0.0 };
//     static constexpr std::array<double, 4> kMinorGrid =
//       { -84.0, -60.0, -36.0, -12.0 };
//     static double AmpToX(float amp) {
//       return 20.0 * std::log10(static_cast<double>(amp));
//     }
//     static void FormatAxisLabel(double v, char* buf, size_t n) {
//       std::snprintf(buf, n, "%d", static_cast<int>(v));
//     }
//     static bool IsChannelSymmetric(const iplug::IEditorDelegate*, int) {
//       return false;  // Curvessor has no symmetry param.
//     }
//   };
//   using CurvessorSplineControl =
//     iplug_helpers::SplineEditorControl<Curvessor, CurvessorSplineTraits>;
//
// Requirements on Plugin:
//   - public `int mSelectedKnot` and `int mSelectedChannel` members;
//   - public `void SetSelectedKnot(int knotIdx, int channel)`.
//
// Requirements on the including TU:
//   - `juicy::GuiSpline` must be visible (i.e. `#include "SplineEditorDsp.hpp"`
//     before this header). We instantiate it directly rather than templating
//     the spline type because both plugins already share the juicy submodule.
//
// All palette constants live in `iplug-helpers/controls/Palette.hpp`.

#pragma once

#include "iplug-helpers/controls/Palette.hpp"

#include "IControl.h"
#include "IGraphicsStructs.h"
#include "IPlugStructs.h"   // ISender / ISenderData / IByteStream
#include "IPlugParameter.h" // IParam

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>

namespace iplug_helpers {

using iplug::igraphics::IControl;
using iplug::igraphics::IGraphics;
using iplug::igraphics::IRECT;
using iplug::igraphics::IColor;
using iplug::igraphics::IMouseMod;
using iplug::igraphics::IText;
using iplug::igraphics::EAlign;
using iplug::IParam;
using iplug::IByteStream;
using iplug::ISender;
using iplug::ISenderData;

template <class Plugin, class Traits>
class SplineEditorControl : public IControl
{
public:
  SplineEditorControl(const IRECT& bounds)
  : IControl(bounds)
  {
  }

  void Draw(IGraphics& g) override
  {
    auto* del = GetDelegate();

    // LCD chassis.
    g.FillRect(kLcdBg, mRECT);

    DrawGrid(g);

    // Refresh scalar spline from the live params and count active knots.
    const int numActive = RefreshSplineFromParams();

    // Detect whether any knot is unlinked — if all are linked, ch0 and ch1
    // curves overlap so we draw a single curve.
    bool anyUnlinked = false;
    for (int i = 0; i < Traits::kNumKnots; ++i) {
      const int base = Traits::kFirstKnotParamBase + i * Traits::kKnotParamStride;
      if (!del->GetParam(base + 0)->Bool()) continue;
      if (!del->GetParam(base + 1)->Bool()) { anyUnlinked = true; break; }
    }

    // Curves — ch0 first, then ch1 on top if any knot is split.
    DrawCurve(g, 0, kLcdCurveCh0, numActive);
    if (anyUnlinked) DrawCurve(g, 1, kLcdCurveCh1, numActive);

    // Knots.
    auto* plug = static_cast<Plugin*>(del);
    const int selKnot = plug->mSelectedKnot;
    const int selCh   = plug->mSelectedChannel;

    for (int i = 0; i < Traits::kNumKnots; ++i) {
      const int base = Traits::kFirstKnotParamBase + i * Traits::kKnotParamStride;
      const bool enabled = del->GetParam(base + 0)->Bool();
      const bool linked  = del->GetParam(base + 1)->Bool();
      for (int c = 0; c < 2; ++c) {
        if (linked && c > 0) break;
        const int chBase = base + 2 + c * 4;
        const float kx = DataToScreenX(del->GetParam(chBase + 0)->Value());
        const float ky = DataToScreenY(del->GetParam(chBase + 1)->Value());

        const bool hot = enabled
                      && ((i == mDraggedKnot && c == mDraggedChannel)
                       || (i == mHoverKnot   && c == mHoverChannel));
        const bool sel = enabled && (i == selKnot && c == selCh);

        if (enabled) {
          const IColor& fill = hot ? kLcdSelectedHalo
                                   : (c == 0 ? kLcdKnotCh0 : kLcdKnotCh1);
          const float r = sel ? kKnotRadius + 2.f : kKnotRadius;
          g.FillCircle(fill, kx, ky, r);
          g.DrawCircle(kLcdKnotRing, kx, ky, r, nullptr, sel ? 2.f : 1.f);
        } else {
          // Ghost: smaller, translucent. Still hit-testable for double-
          // click re-enable via FindKnotAt(includeDisabled=true).
          const IColor& fill = (c == 0) ? kLcdKnotGhostCh0 : kLcdKnotGhostCh1;
          g.FillCircle(fill, kx, ky, kKnotRadius - 1.f);
        }
      }
    }

    // Tangent handle on the currently-selected knot.
    if (selKnot >= 0) {
      const int selBase = Traits::kFirstKnotParamBase + selKnot * Traits::kKnotParamStride;
      if (del->GetParam(selBase + 0)->Bool()) {
        const bool linked = del->GetParam(selBase + 1)->Bool();
        DrawTangentHandle(g, selKnot, linked ? 0 : selCh);
      }
    }

    // Live "current input" dot per channel + vertical line to identity.
    for (int c = 0; c < 2; ++c) {
      const float amp = mCurrentLevelAmp[c];
      if (amp <= 0.f) continue;
      const double xVal = Traits::AmpToX(amp);
      if (xVal < Traits::kKnotMin || xVal > Traits::kKnotMax) continue;
      const double yVal = mSpline.process(xVal, c, numActive);
      const float dx = DataToScreenX(xVal);
      const float dy = DataToScreenY(yVal);
      const float identityY = DataToScreenY(xVal);

      const IColor grCol  = (c == 0) ? kLcdGrLineCh0   : kLcdGrLineCh1;
      const IColor dotCol = (c == 0) ? kLcdLevelDotCh0 : kLcdLevelDotCh1;
      g.DrawLine(grCol, dx, dy, dx, identityY, nullptr, 2.f);
      g.FillCircle(dotCol, dx, dy, 4.f);
      g.DrawCircle(kLcdKnotRing, dx, dy, 4.f, nullptr, 1.f);
    }

    // Reset-zoom button, painted only when actually zoomed in. Clicking it
    // returns the viewport to the full data range on both axes.
    if (mZoom > 1.001) {
      const IRECT btn = ResetButtonRect();
      g.FillRoundRect(IColor(220, 22, 38, 50), btn, 3.f);
      g.DrawRoundRect(kLcdFrame, btn, 3.f, nullptr, 1.f);
      static const IText kResetBtnText(10, IColor(255, 200, 230, 240),
                                       nullptr, EAlign::Center);
      g.DrawText(kResetBtnText, "1:1", btn);
    }

    // LCD frame.
    g.DrawRect(kLcdFrame, mRECT, nullptr, 1.f);
  }

  void OnMouseWheel(float x, float y, const IMouseMod&, float d) override
  {
    // Scroll-wheel zoom around the cursor: the data point under the cursor
    // stays anchored to the same pixel before/after the zoom change.
    const double cursorX = ScreenXToData(x);
    const double cursorY = ScreenYToData(y);

    // Per-tick zoom factor. Positive d (scroll up) = zoom in.
    const double factor = std::pow(1.25, static_cast<double>(d));
    const double newZoom = std::clamp(mZoom * factor, kMinZoom, kMaxZoom);
    if (newZoom == mZoom) return;
    mZoom = newZoom;

    // Recompute viewport centre so cursor's pre-zoom data value still maps
    // to its screen position. Derived from the inverse of ScreenXToData:
    //   cursorX = (centerX - hv) + frac * 2*hv
    //         => centerX = cursorX + hv*(1 - 2*frac)
    const double hv = VisibleHalfRange();
    const double fracX = (x - mRECT.L) / static_cast<double>(mRECT.W());
    const double fracY = (mRECT.B - y) / static_cast<double>(mRECT.H());
    mZoomCenterX = cursorX + hv * (1.0 - 2.0 * fracX);
    mZoomCenterY = cursorY + hv * (1.0 - 2.0 * fracY);
    ClampViewToDataRange();
    SetDirty(false);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    auto* del = GetDelegate();
    const bool preferCh1 = mod.R || mod.A;

    // Reset-zoom button in the top-right corner. Has to win the priority
    // race vs everything else (its rect overlaps the editor's drawable
    // area where knots and tangent handles also live).
    if (ResetButtonRect().Contains(x, y)) {
      ResetZoom();
      return;
    }

    // Tangent handle on the currently-selected knot takes priority — it
    // overlaps the knot area visually but the hit is the smaller dot.
    const KnotHit thit = FindTangentAt(x, y);
    if (thit.knot >= 0) {
      mDraggedKnot = thit.knot;
      mDraggedChannel = thit.channel;
      mDraggingTangent = true;
      const int base = Traits::kFirstKnotParamBase + thit.knot * Traits::kKnotParamStride;
      const int chBase = base + 2 + thit.channel * 4;
      del->BeginInformHostOfParamChangeFromUI(chBase + 2);  // Tan_chC
      SetDirty(false);
      return;
    }

    // Knot body hit. Right-button / Alt biases the hit toward ch1 when both
    // channels of an unlinked knot are stacked on screen.
    const KnotHit hit = FindKnotAt(x, y, preferCh1, /*includeDisabled=*/false);
    mDraggedKnot = hit.knot;
    mDraggedChannel = hit.channel;
    mDraggingTangent = false;
    if (hit.knot >= 0) {
      const int base = Traits::kFirstKnotParamBase + hit.knot * Traits::kKnotParamStride;
      const int chBase = base + 2 + hit.channel * 4;
      del->BeginInformHostOfParamChangeFromUI(chBase + 0);  // X_chC
      del->BeginInformHostOfParamChangeFromUI(chBase + 1);  // Y_chC
      // Tell the plugin to rebind the side-panel knobs to this knot/channel.
      static_cast<Plugin*>(del)->SetSelectedKnot(hit.knot, hit.channel);
    }
    else if (mZoom > 1.001) {
      // Empty area + zoomed in → start a viewport pan.
      mPanning = true;
      mPanStartMouseX = x;
      mPanStartMouseY = y;
      mPanStartCenterX = mZoomCenterX;
      mPanStartCenterY = mZoomCenterY;
    }
    SetDirty(false);
  }

  // Double-click toggles a knot's state. Matches the JUCE editor:
  //   - left / no-mod double-click on a knot → toggle `enabled` (add/remove)
  //   - right / alt double-click on a knot   → toggle `linked` (split L/R)
  // Disabled knots are hit-testable (drawn faintly as ghosts) so users can
  // re-enable them by double-clicking.
  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override
  {
    const bool preferCh1 = mod.R || mod.A;
    const KnotHit hit = FindKnotAt(x, y, preferCh1, /*includeDisabled=*/true);
    if (hit.knot < 0) {
      SetDirty(false);
      return;
    }
    const int base = Traits::kFirstKnotParamBase + hit.knot * Traits::kKnotParamStride;
    auto* del = GetDelegate();
    if (preferCh1) {
      const bool linked = del->GetParam(base + 1)->Bool();
      SetParamBoolFromUI(base + 1, !linked);
    } else {
      const bool enabled = del->GetParam(base + 0)->Bool();
      SetParamBoolFromUI(base + 0, !enabled);
    }
    // Make this knot the selection so the side panel binds to it.
    static_cast<Plugin*>(del)->SetSelectedKnot(hit.knot, hit.channel);
    SetDirty(false);
  }

  void OnMouseDrag(float x, float y, float, float, const IMouseMod&) override
  {
    if (mPanning) {
      // 1 screen pixel = (visible range / widget width) data units. Drag
      // right → viewport scrolls left (centre moves left) so the content
      // slides right with the cursor.
      const double hv = VisibleHalfRange();
      const double unitsPerPx = 2.0 * hv / static_cast<double>(mRECT.W());
      mZoomCenterX = mPanStartCenterX
                   - (x - mPanStartMouseX) * unitsPerPx;
      // Y is screen-inverted: drag down → viewport centre moves up in data.
      mZoomCenterY = mPanStartCenterY
                   + (y - mPanStartMouseY) * unitsPerPx;
      ClampViewToDataRange();
      SetDirty(false);
      return;
    }

    if (mDraggedKnot < 0) return;
    const int base = Traits::kFirstKnotParamBase + mDraggedKnot * Traits::kKnotParamStride;
    const int chBase = base + 2 + mDraggedChannel * 4;
    auto* del = GetDelegate();

    if (mDraggingTangent) {
      // Tangent t = dy / dx, with the cursor offset measured from the knot
      // in screen space (y inverted). When the cursor is dragged near the
      // vertical through the knot, slope diverges → clamp.
      const float kx = DataToScreenX(del->GetParam(chBase + 0)->Value());
      const float ky = DataToScreenY(del->GetParam(chBase + 1)->Value());
      const float dxScreen = x - kx;
      const float dyScreen = ky - y;  // up on screen = positive

      const int tanIdx = chBase + 2;
      const IParam* tanParam = del->GetParam(tanIdx);
      double tNew;
      if (std::abs(dxScreen) < 1.f) {
        tNew = (dyScreen >= 0.f) ? tanParam->GetMax() : tanParam->GetMin();
      } else {
        tNew = static_cast<double>(dyScreen) / dxScreen;
        // If the user drags past the vertical, sign flips weirdly — clamp.
        tNew = std::clamp(tNew, tanParam->GetMin(), tanParam->GetMax());
      }
      del->SendParameterValueFromUI(tanIdx, tanParam->ToNormalized(tNew));
    }
    else {
      const int xIdx = chBase + 0;
      const int yIdx = chBase + 1;
      const IParam* xParam = del->GetParam(xIdx);
      const IParam* yParam = del->GetParam(yIdx);
      const double xv = std::clamp(ScreenXToData(x), xParam->GetMin(), xParam->GetMax());
      const double yv = std::clamp(ScreenYToData(y), yParam->GetMin(), yParam->GetMax());
      del->SendParameterValueFromUI(xIdx, xParam->ToNormalized(xv));
      del->SendParameterValueFromUI(yIdx, yParam->ToNormalized(yv));
    }

    SetDirty(false);
  }

  void OnMouseUp(float, float, const IMouseMod&) override
  {
    if (mPanning) {
      mPanning = false;
      SetDirty(false);
      return;
    }
    if (mDraggedKnot >= 0) {
      const int base = Traits::kFirstKnotParamBase + mDraggedKnot * Traits::kKnotParamStride;
      const int chBase = base + 2 + mDraggedChannel * 4;
      auto* del = GetDelegate();
      if (mDraggingTangent) {
        del->EndInformHostOfParamChangeFromUI(chBase + 2);
      } else {
        del->EndInformHostOfParamChangeFromUI(chBase + 0);
        del->EndInformHostOfParamChangeFromUI(chBase + 1);
      }
    }
    mDraggedKnot = -1;
    mDraggedChannel = 0;
    mDraggingTangent = false;
    SetDirty(false);
  }

  void OnMouseOver(float x, float y, const IMouseMod&) override
  {
    const KnotHit hit = FindKnotAt(x, y);
    if (hit.knot != mHoverKnot || hit.channel != mHoverChannel) {
      mHoverKnot = hit.knot;
      mHoverChannel = hit.channel;
      SetDirty(false);
    }
  }

  void OnMouseOut() override
  {
    if (mHoverKnot != -1) {
      mHoverKnot = -1;
      mHoverChannel = 0;
      SetDirty(false);
    }
  }

  // Receives ISenderData<2, float> packets from the plugin's OnIdle. The
  // payload carries a per-channel linear amplitude (the meter widget needs
  // amp for its Log response); we stash it and read it back in Draw to
  // position the live level dot on the curve.
  void OnMsgFromDelegate(int msgTag, int dataSize, const void* pData) override
  {
    if (msgTag != ISender<>::kUpdateMessage) return;
    IByteStream stream(pData, dataSize);
    int pos = 0;
    ISenderData<2, float> d;
    pos = stream.Get(&d, pos);
    mCurrentLevelAmp[0] = d.vals[0];
    mCurrentLevelAmp[1] = d.vals[1];
    // No SetDirty here — OnIdle already repaints us every frame.
  }

private:
  static constexpr float kKnotRadius = 6.f;
  static constexpr float kKnotHitRadiusSq = 20.f * 20.f;
  static constexpr float kTangentHandleOffset = 32.f;   // pixels from knot center
  static constexpr float kTangentHandleRadius = 4.f;
  static constexpr float kTangentHitRadiusSq = 14.f * 14.f;

  struct KnotHit { int knot; int channel; };

  juicy::GuiSpline mSpline{ Traits::kMaxKnotsInSpline };
  int mDraggedKnot = -1;
  int mDraggedChannel = 0;
  bool mDraggingTangent = false;
  int mHoverKnot = -1;
  int mHoverChannel = 0;
  // Live per-channel linear amplitude packet (envelope-follower output for
  // a compressor, wet-path RMS for a waveshaper). Drawn as a dot riding
  // along the curve to show where the process is acting now.
  std::array<float, 2> mCurrentLevelAmp{};

  // Uniform 2D zoom on the LCD viewport, scroll-wheel driven; zoomCenter*
  // is the data point at the centre of the visible window. Reset returns
  // to mZoom = 1 (full kKnotMin..kKnotMax on both axes).
  double mZoom = 1.0;
  double mZoomCenterX = (Traits::kKnotMin + Traits::kKnotMax) * 0.5;
  double mZoomCenterY = (Traits::kKnotMin + Traits::kKnotMax) * 0.5;
  static constexpr double kMinZoom = 1.0;
  static constexpr double kMaxZoom = 16.0;

  // Pan state — set in OnMouseDown when the user clicks on empty LCD area
  // while zoomed in. OnMouseDrag then slides mZoomCenter* so the data
  // point under the cursor moves 1:1 with the cursor.
  bool   mPanning = false;
  float  mPanStartMouseX = 0.f;
  float  mPanStartMouseY = 0.f;
  double mPanStartCenterX = 0.0;
  double mPanStartCenterY = 0.0;

  // data ↔ screen mappings, both apply the zoom-and-pan viewport. At
  // zoom = 1 and the default centre, this is identical to the un-zoomed
  // mapping. Visible window half-range = totalRange / 2 / mZoom.
  double VisibleHalfRange() const
  {
    return (Traits::kKnotMax - Traits::kKnotMin) * 0.5 / mZoom;
  }
  float DataToScreenX(double v) const
  {
    const double hv = VisibleHalfRange();
    const double frac = (v - (mZoomCenterX - hv)) / (2.0 * hv);
    return mRECT.L + static_cast<float>(frac) * mRECT.W();
  }
  float DataToScreenY(double v) const
  {
    const double hv = VisibleHalfRange();
    const double frac = (v - (mZoomCenterY - hv)) / (2.0 * hv);
    return mRECT.B - static_cast<float>(frac) * mRECT.H();
  }
  double ScreenXToData(float x) const
  {
    const double hv = VisibleHalfRange();
    const double frac = (x - mRECT.L) / static_cast<double>(mRECT.W());
    return (mZoomCenterX - hv) + frac * 2.0 * hv;
  }
  double ScreenYToData(float y) const
  {
    const double hv = VisibleHalfRange();
    const double frac = (mRECT.B - y) / static_cast<double>(mRECT.H());
    return (mZoomCenterY - hv) + frac * 2.0 * hv;
  }

  // Keep the viewport inside [kKnotMin, kKnotMax] in both axes so the user
  // can't scroll/zoom off the data range.
  void ClampViewToDataRange()
  {
    if (mZoom <= 1.0) {
      mZoom = 1.0;
      mZoomCenterX = (Traits::kKnotMin + Traits::kKnotMax) * 0.5;
      mZoomCenterY = (Traits::kKnotMin + Traits::kKnotMax) * 0.5;
      return;
    }
    const double hv = VisibleHalfRange();
    mZoomCenterX = std::clamp(mZoomCenterX, Traits::kKnotMin + hv, Traits::kKnotMax - hv);
    mZoomCenterY = std::clamp(mZoomCenterY, Traits::kKnotMin + hv, Traits::kKnotMax - hv);
  }

  void ResetZoom()
  {
    mZoom = 1.0;
    mZoomCenterX = (Traits::kKnotMin + Traits::kKnotMax) * 0.5;
    mZoomCenterY = (Traits::kKnotMin + Traits::kKnotMax) * 0.5;
    SetDirty(false);
  }

  // Reset-zoom button in the LCD's top-right corner. 38 × 16 px, "1:1".
  IRECT ResetButtonRect() const
  {
    return IRECT(mRECT.R - 42.f, mRECT.T + 4.f,
                 mRECT.R - 4.f,  mRECT.T + 20.f);
  }

  void DrawCurve(IGraphics& g, int channel, const IColor& col, int numActive)
  {
    float prevX = 0.f, prevY = 0.f;
    constexpr int kNumSamples = 240;
    for (int i = 0; i <= kNumSamples; ++i) {
      const double xv = Traits::kKnotMin
                      + (Traits::kKnotMax - Traits::kKnotMin) * (i / double(kNumSamples));
      const double yv = mSpline.process(xv, channel, numActive);
      const float sx = DataToScreenX(xv);
      const float sy = DataToScreenY(yv);
      if (i > 0) g.DrawLine(col, prevX, prevY, sx, sy, nullptr, 2.f);
      prevX = sx;
      prevY = sy;
    }
  }

  // LCD-style grid driven by Traits::kMajorGrid + Traits::kMinorGrid (the
  // tick positions in data units). Labels along the bottom (X) and the
  // left (Y) edges in a low-contrast teal so they don't compete with the
  // curves. The y = x identity diagonal is drawn over the grid as a faint
  // reference.
  void DrawGrid(IGraphics& g)
  {
    // Minor lines first so the major lines paint on top.
    for (double v : Traits::kMinorGrid) {
      const float xs = DataToScreenX(v);
      const float ys = DataToScreenY(v);
      g.DrawLine(kLcdGridMinor, xs, mRECT.T + 1, xs, mRECT.B - 1, nullptr, 1.f);
      g.DrawLine(kLcdGridMinor, mRECT.L + 1, ys, mRECT.R - 1, ys, nullptr, 1.f);
    }

    for (double v : Traits::kMajorGrid) {
      const float xs = DataToScreenX(v);
      const float ys = DataToScreenY(v);
      g.DrawLine(kLcdGridMajor, xs, mRECT.T + 1, xs, mRECT.B - 1, nullptr, 1.f);
      g.DrawLine(kLcdGridMajor, mRECT.L + 1, ys, mRECT.R - 1, ys, nullptr, 1.f);

      char buf[16];
      Traits::FormatAxisLabel(v, buf, sizeof(buf));

      // X-axis label, bottom edge, slightly inset.
      const IRECT xLabel(xs - 18.f, mRECT.B - 15.f, xs + 18.f, mRECT.B - 3.f);
      g.DrawText(kLcdAxisLabelText, buf, xLabel);

      // Y-axis label, left edge. Skip the bottom-left corner where the two
      // axes overlap visually (same label twice reads as one).
      if (v != Traits::kKnotMin) {
        const IRECT yLabel(mRECT.L + 3.f, ys - 7.f, mRECT.L + 32.f, ys + 7.f);
        g.DrawText(kLcdAxisLabelTextLeft, buf, yLabel);
      }
    }

    // y = x identity diagonal — a "no-change" reference for the user when
    // reading the curve. Faint so the actual curve is clearly the figure.
    const float topRightX = DataToScreenX(Traits::kKnotMax);
    const float topRightY = DataToScreenY(Traits::kKnotMax);
    const float bottomLeftX = DataToScreenX(Traits::kKnotMin);
    const float bottomLeftY = DataToScreenY(Traits::kKnotMin);
    g.DrawLine(kLcdIdentityLine,
               bottomLeftX, bottomLeftY,
               topRightX,   topRightY,
               nullptr, 1.f);
  }

  // Small param-write helpers — wrap Begin/Send/End for a single-shot write.
  void SetParamFromUI(int paramIdx, double clampedValue)
  {
    auto* del = GetDelegate();
    const IParam* p = del->GetParam(paramIdx);
    del->BeginInformHostOfParamChangeFromUI(paramIdx);
    del->SendParameterValueFromUI(paramIdx, p->ToNormalized(clampedValue));
    del->EndInformHostOfParamChangeFromUI(paramIdx);
  }
  void SetParamBoolFromUI(int paramIdx, bool value)
  {
    SetParamFromUI(paramIdx, value ? 1.0 : 0.0);
  }

  // Hit-test knots, returning the nearest hit subject to channel-preference
  // and enabled-state filters. Mirrors juicy/SplineEditor::selectKnot — the
  // RMB / Alt modifier biases toward ch1 when both channels of an unlinked
  // knot sit at the same screen position. For double-click we also include
  // disabled knots so the user can target their (ghost-drawn) positions.
  KnotHit FindKnotAt(float x, float y,
                     bool preferCh1 = false,
                     bool includeDisabled = false)
  {
    auto* del = GetDelegate();
    KnotHit nearest[2] = {{-1, 0}, {-1, 1}};   // best ch0 hit, best ch1 hit
    float nearestDist2[2] = {kKnotHitRadiusSq, kKnotHitRadiusSq};

    for (int i = 0; i < Traits::kNumKnots; ++i) {
      const int base = Traits::kFirstKnotParamBase + i * Traits::kKnotParamStride;
      const bool enabled = del->GetParam(base + 0)->Bool();
      if (!includeDisabled && !enabled) continue;
      const bool linked = del->GetParam(base + 1)->Bool();
      for (int c = 0; c < 2; ++c) {
        if (linked && c > 0) break;
        const int chBase = base + 2 + c * 4;
        const float kx = DataToScreenX(del->GetParam(chBase + 0)->Value());
        const float ky = DataToScreenY(del->GetParam(chBase + 1)->Value());
        const float dx = kx - x;
        const float dy = ky - y;
        const float d2 = dx * dx + dy * dy;
        // Linked knot contributes to the ch0 hit slot; unlinked ch1 to ch1.
        const int hitCh = linked ? 0 : c;
        if (d2 < nearestDist2[hitCh]) {
          nearestDist2[hitCh] = d2;
          nearest[hitCh] = {i, hitCh};
        }
      }
    }

    // Pick channel preference; fall back to the other if the preferred has
    // no hit.
    int pick;
    if (preferCh1 && nearest[1].knot >= 0) {
      pick = 1;
    } else if (nearest[0].knot >= 0 && nearest[1].knot >= 0) {
      pick = (nearestDist2[0] <= nearestDist2[1]) ? 0 : 1;
    } else {
      pick = (nearest[0].knot >= 0) ? 0 : 1;
    }
    return nearest[pick];
  }

  // Tangent handle hit-test, on the currently-selected knot/channel only.
  // We don't show handles on every visible knot — too much visual clutter
  // for the small editor — so the user picks a knot first, then drags its
  // tangent.
  KnotHit FindTangentAt(float x, float y)
  {
    auto* plug = static_cast<Plugin*>(GetDelegate());
    const int knotIdx = plug->mSelectedKnot;
    const int channel = plug->mSelectedChannel;
    if (knotIdx < 0) return {-1, 0};

    auto* del = GetDelegate();
    const int base = Traits::kFirstKnotParamBase + knotIdx * Traits::kKnotParamStride;
    if (!del->GetParam(base + 0)->Bool()) return {-1, 0};
    // When linked, the side panel's "ch1" selection is meaningless; pin to ch0.
    const bool linked = del->GetParam(base + 1)->Bool();
    const int effectiveCh = linked ? 0 : channel;

    float hx, hy;
    TangentHandleScreenPos(knotIdx, effectiveCh, hx, hy);
    const float dx = hx - x;
    const float dy = hy - y;
    if (dx * dx + dy * dy < kTangentHitRadiusSq) {
      return {knotIdx, effectiveCh};
    }
    return {-1, 0};
  }

  // Compute the (x, y) screen position of the tangent handle for the given
  // knot/channel. Uses atan(t) to keep the handle on a fixed-radius circle,
  // so steep tangents don't push the handle off-screen.
  void TangentHandleScreenPos(int knotIdx, int channel, float& outX, float& outY)
  {
    auto* del = GetDelegate();
    const int base = Traits::kFirstKnotParamBase + knotIdx * Traits::kKnotParamStride;
    const int chBase = base + 2 + channel * 4;
    const float kx = DataToScreenX(del->GetParam(chBase + 0)->Value());
    const float ky = DataToScreenY(del->GetParam(chBase + 1)->Value());
    const double t = del->GetParam(chBase + 2)->Value();
    const double alpha = std::atan(t);
    outX = kx + kTangentHandleOffset * static_cast<float>(std::cos(alpha));
    outY = ky - kTangentHandleOffset * static_cast<float>(std::sin(alpha));
  }

  void DrawTangentHandle(IGraphics& g, int knotIdx, int channel)
  {
    auto* del = GetDelegate();
    const int base = Traits::kFirstKnotParamBase + knotIdx * Traits::kKnotParamStride;
    if (!del->GetParam(base + 0)->Bool()) return;
    const int chBase = base + 2 + channel * 4;
    const float kx = DataToScreenX(del->GetParam(chBase + 0)->Value());
    const float ky = DataToScreenY(del->GetParam(chBase + 1)->Value());

    float hx, hy;
    TangentHandleScreenPos(knotIdx, channel, hx, hy);

    g.DrawLine(kLcdTangentLine, kx, ky, hx, hy, nullptr, 1.f);

    const bool isDragging = (knotIdx == mDraggedKnot
                          && channel == mDraggedChannel
                          && mDraggingTangent);
    const IColor handleCol = isDragging ? kLcdSelectedHalo : kLcdTangentHandle;
    g.FillCircle(handleCol, hx, hy, kTangentHandleRadius);
    g.DrawCircle(kLcdKnotRing, hx, hy, kTangentHandleRadius, nullptr, 1.f);
  }

  // Populates mSpline from the live param values. Returns count of active
  // knots (fixed anchor at slot 0 + each enabled editable knot). Mirrors
  // the ProcessBlock-side UpdateSplineFromParams helper, per-channel.
  //
  // Slot 0 is a fixed (kAnchorX, kAnchorY, t=1, s=1) anchor — for the
  // waveshaper this pins the curve through the origin so a zero input
  // produces a zero output (no DC); for the compressor it pins the curve
  // at the noise-floor corner.
  //
  // Symmetry (Overdraw-only via Traits::IsChannelSymmetric): when on for
  // a channel, the plugin's DSP folds input via abs() before evaluating
  // the spline, so negative-X knots have no effect. We neuter them to
  // anchor coordinates here so the drawn curve matches the DSP-side
  // result.
  int RefreshSplineFromParams()
  {
    auto* del = GetDelegate();

    bool symPerCh[2];
    for (int c = 0; c < 2; ++c) {
      symPerCh[c] = Traits::IsChannelSymmetric(del, c);
      mSpline.setIsSymmetric(c, symPerCh[c]);
    }

    int n = 0;
    for (int c = 0; c < 2; ++c) {
      mSpline.knot(n).x[c] = Traits::kAnchorX;
      mSpline.knot(n).y[c] = Traits::kAnchorY;
      mSpline.knot(n).t[c] = 1.0;
      mSpline.knot(n).s[c] = 1.0;
    }
    ++n;

    for (int i = 0; i < Traits::kNumKnots; ++i) {
      const int base = Traits::kFirstKnotParamBase + i * Traits::kKnotParamStride;
      if (!del->GetParam(base + 0)->Bool()) continue;
      const bool linked = del->GetParam(base + 1)->Bool();
      for (int c = 0; c < 2; ++c) {
        const int chSrc = linked ? 0 : c;
        const int chBase = base + 2 + chSrc * 4;
        const double kx = del->GetParam(chBase + 0)->Value();
        const bool neuter = symPerCh[c] && (kx < 0.0);
        mSpline.knot(n).x[c] = neuter ? Traits::kAnchorX : kx;
        mSpline.knot(n).y[c] = neuter ? Traits::kAnchorY
                                      : del->GetParam(chBase + 1)->Value();
        mSpline.knot(n).t[c] = neuter ? 1.0 : del->GetParam(chBase + 2)->Value();
        mSpline.knot(n).s[c] = neuter ? 1.0 : del->GetParam(chBase + 3)->Value();
      }
      ++n;
    }
    return n;
  }
};

} // namespace iplug_helpers
