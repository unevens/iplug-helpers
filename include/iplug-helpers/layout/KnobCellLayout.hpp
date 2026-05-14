// iplug-helpers — layout/KnobCellLayout.hpp
//
// A "labeled knob" cell stacks three pieces top-down with 1 px gaps:
//
//   [bold name label, 18 px]  (renders 15 pt Roboto-Bold)
//   [disc,           36 × 36] (centred horizontally)
//   [caption,        18 px]   (15 pt editable ICaptionControl)
//
// The whole 74 px block is centred vertically inside the supplied cell —
// so callers can hand the helpers any size of cell and the knob+label+
// caption stack will float in the middle. Matches the visual rhythm
// shared by Curvessor and Overdraw.
//
// Helpers:
//   KnobDiscRectIn(cell)    → IRECT for the IVKnobControl
//   KnobLabelRectIn(cell)   → IRECT for the bold name label ITextControl
//   KnobCaptionRectIn(cell) → IRECT for the editable ICaptionControl
//   AttachLabeledKnob(...)  → attaches all three controls and returns the
//                             IVKnobControl pointer (for the caller's
//                             cached side-panel rebinding).
//
// Typical usage in mLayoutFunc:
//   auto attachLabeledKnob =
//     [&](const IRECT& cell, int paramIdx, const char* name) {
//       return iplug_helpers::AttachLabeledKnob(
//         pGraphics, cell, paramIdx, name,
//         kCurvessorStyle.labelText, knobStyleNoLabel,
//         iplug_helpers::kSideCaptionText, iplug_helpers::kCaptionBg);
//     };

#pragma once

#include "IControls.h"

namespace iplug_helpers {

inline constexpr float kKnobDiscW   = 36.f;
inline constexpr float kKnobDiscH   = 36.f;
inline constexpr float kKnobLabelH  = 18.f;  // 15 pt bold name label
inline constexpr float kKnobGap     = 1.f;
inline constexpr float kKnobCapH    = 18.f;  // 15 pt editable caption
inline constexpr float kKnobPairH   = kKnobLabelH + kKnobGap + kKnobDiscH
                                    + kKnobGap + kKnobCapH;  // 74

// Matrix knobs share the disc size but their captions sit below a 36 px
// cell with no label band, so we expose a separate constant for the
// matrix caption strip height.
inline constexpr float kMatrixCapH  = 18.f;

inline iplug::igraphics::IRECT
KnobDiscRectIn(const iplug::igraphics::IRECT& cell)
{
  const float pairT = cell.T + (cell.H() - kKnobPairH) * 0.5f;
  const float discT = pairT + kKnobLabelH + kKnobGap;
  const float cx = cell.MW();
  return iplug::igraphics::IRECT(
    cx - kKnobDiscW * 0.5f, discT,
    cx + kKnobDiscW * 0.5f, discT + kKnobDiscH);
}

inline iplug::igraphics::IRECT
KnobLabelRectIn(const iplug::igraphics::IRECT& cell)
{
  const float pairT = cell.T + (cell.H() - kKnobPairH) * 0.5f;
  // Full cell width so long names like "Stereo Link" still fit. The disc
  // stays 36 px and centred.
  return iplug::igraphics::IRECT(
    cell.L, pairT, cell.R, pairT + kKnobLabelH);
}

inline iplug::igraphics::IRECT
KnobCaptionRectIn(const iplug::igraphics::IRECT& cell)
{
  const float pairT = cell.T + (cell.H() - kKnobPairH) * 0.5f;
  const float capT  = pairT + kKnobLabelH + kKnobGap
                    + kKnobDiscH + kKnobGap;
  return iplug::igraphics::IRECT(
    cell.L, capT, cell.R, capT + kKnobCapH);
}

// Attach a bold name label + 36 px disc + editable 15 pt caption inside
// `cell`. Returns the disc control (IVKnobControl) so the caller can
// cache it for dynamic param rebinding.
inline iplug::igraphics::IControl*
AttachLabeledKnob(
  iplug::igraphics::IGraphics* g,
  const iplug::igraphics::IRECT& cell,
  int paramIdx,
  const char* name,
  const iplug::igraphics::IText& labelText,
  const iplug::igraphics::IVStyle& knobStyle,
  const iplug::igraphics::IText& captionText,
  const iplug::igraphics::IColor& captionBg,
  bool showCaptionUnit = true)
{
  using namespace iplug::igraphics;
  g->AttachControl(new ITextControl(
    KnobLabelRectIn(cell), name, labelText));
  IControl* knob = g->AttachControl(new IVKnobControl(
    KnobDiscRectIn(cell), paramIdx, "", knobStyle));
  g->AttachControl(new ICaptionControl(
    KnobCaptionRectIn(cell), paramIdx, captionText, captionBg,
    showCaptionUnit));
  return knob;
}

} // namespace iplug_helpers
