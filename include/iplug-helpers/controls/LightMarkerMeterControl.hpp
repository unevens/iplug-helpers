// iplug-helpers — controls/LightMarkerMeterControl.hpp
//
// IVMeterControl with light-coloured dB markers + always-visible per-track
// labels.
//
// Two visual tweaks vs upstream's IVMeterControl:
//
// 1. DrawMarkers reimplementation: upstream hardcodes DEFAULT_TEXT (black)
//    for the "-24 dB" / "0 dB" / etc. tick labels, which is unreadable on
//    the dark "spacestation" panel. We use a light text style instead.
//
// 2. Per-track name labels (the "L" / "R" or "M" / "S" letter passed in
//    the trackNames initializer-list at construction): upstream's
//    IVTrackControlBase::DrawTrack paints the name BEFORE the bar fill, so
//    on anything but a silent track the bar overdraws the label. We
//    override DrawTrackName to a no-op (suppress the base render) and
//    override DrawTrack to paint our own crisp label in a small fixed
//    strip at the top of the track AFTER the bar is drawn — always
//    visible, lined up across both channels.

#pragma once

#include "IControls.h"
#include "wdlstring.h"
#include <cmath>

namespace iplug_helpers {

template<int MAXNC>
class LightMarkerMeterControl : public iplug::igraphics::IVMeterControl<MAXNC>
{
  using Base = iplug::igraphics::IVMeterControl<MAXNC>;
public:
  using Base::Base;  // inherit ctors

  void Draw(iplug::igraphics::IGraphics& g) override
  {
    this->DrawBackground(g, this->mRECT);
    this->DrawWidget(g);
    this->DrawLabel(g);

    if (this->mResponse == Base::EResponse::Log)
      DrawLightMarkers(g);

    if (this->mStyle.drawFrame)
      g.DrawRect(this->GetColor(iplug::igraphics::kFR),
                 this->mWidgetBounds, &this->mBlend,
                 this->mStyle.frameThickness);
  }

  // Suppress the base render of the per-track name — its default position
  // is centred in the full track rect, so the bar fill paints over it.
  // We render our own version on top of the bar in DrawTrack below.
  void DrawTrackName(iplug::igraphics::IGraphics&,
                     const iplug::igraphics::IRECT&,
                     int /*chIdx*/) override
  {
  }

  // Draw the bar via the base, THEN paint a crisp track-name letter in a
  // small fixed strip at the top of the track. The strip is positioned so
  // the meter's typical operating range (well below the peak markers)
  // never reaches it, and even on peaks the text sits on top of the bar.
  void DrawTrack(iplug::igraphics::IGraphics& g,
                 const iplug::igraphics::IRECT& r, int chIdx) override
  {
    Base::DrawTrack(g, r, chIdx);
    if (!this->HasTrackNames()) return;

    using iplug::igraphics::IText;
    using iplug::igraphics::IColor;
    using iplug::igraphics::IRECT;
    using iplug::igraphics::EAlign;
    using iplug::igraphics::EDirection;

    static const IText kTrackText(11, IColor(255, 230, 240, 250),
                                  "Roboto-Bold", EAlign::Center);
    const float stripH = 14.f;
    IRECT labelR = (this->mDirection == EDirection::Vertical)
                     ? IRECT(r.L, r.T, r.R, r.T + stripH)
                     : IRECT(r.L, r.T, r.L + stripH, r.B);
    g.DrawText(kTrackText, this->GetTrackName(chIdx), labelR);
  }

private:
  void DrawLightMarkers(iplug::igraphics::IGraphics& g)
  {
    using iplug::igraphics::IText;
    using iplug::igraphics::IColor;
    using iplug::igraphics::IRECT;
    using iplug::igraphics::EAlign;
    using iplug::igraphics::EDirection;

    static const IText kMarkerText(11, IColor(255, 200, 220, 230),
                                    nullptr, EAlign::Center);
    const float lowPointAbs = std::fabs(this->mLowRangeDB);
    const float rangeDB     = std::fabs(this->mHighRangeDB - this->mLowRangeDB);

    for (auto pt : this->mMarkers)
    {
      const float linearPos = (pt + lowPointAbs) / rangeDB;
      IRECT r = this->mWidgetBounds.FracRect(this->mDirection, linearPos);

      if (this->mDirection == EDirection::Vertical) {
        r.B = r.T + 10.f;
        g.DrawLine(this->GetColor(iplug::igraphics::kHL),
                   r.L, r.T, r.R, r.T);
      }
      else {
        r.L = r.R - 10.f;
        g.DrawLine(this->GetColor(iplug::igraphics::kHL),
                   r.MW(), r.T, r.MW(), r.B);
      }

      if (this->mStyle.showValue) {
        WDL_String str;
        str.SetFormatted(32, "%i dB", pt);
        g.DrawText(kMarkerText, str.Get(), r);
      }
    }
  }
};

} // namespace iplug_helpers
