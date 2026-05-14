// iplug-helpers — controls/Palette.hpp
//
// Visual palette shared by Curvessor and Overdraw iPlug2 ports —
// "rack on a spacestation": dark panel chassis with an LCD-style spline
// editor, low-contrast teal grid, bright cyan + warm amber accents.
//
// All constants are `inline` so this header can be included in multiple
// TUs without ODR violations.

#pragma once

#include "IGraphicsStructs.h"

namespace iplug_helpers {

using iplug::igraphics::IColor;
using iplug::igraphics::IText;
using iplug::igraphics::IVStyle;
using iplug::igraphics::EAlign;

// ----- Chassis (panel) — used when no background image is loaded yet. -----
inline const IColor kPanelBg          (255,  14,  18,  24);
inline const IColor kPanelFrame       (255,  44,  56,  68);

// ----- LCD area (spline editor + meters). -----
inline const IColor kLcdBg            (255,   6,  12,  16);
inline const IColor kLcdFrame         (255,  60,  90, 105);
inline const IColor kLcdGridMajor     (255,  44,  72,  88);
inline const IColor kLcdGridMinor     (255,  22,  38,  48);
inline const IColor kLcdAxisLabel     (255,  90, 140, 158);
inline const IColor kLcdIdentityLine  (160,  60,  90, 105);

// ----- Spline content. -----
inline const IColor kLcdCurveCh0      (255,  80, 200, 230);  // bright cyan
inline const IColor kLcdCurveCh1      (255, 240, 170,  90);  // warm amber
inline const IColor kLcdKnotCh0       (255, 150, 220, 245);
inline const IColor kLcdKnotCh1       (255, 255, 200, 130);
inline const IColor kLcdKnotGhostCh0  (130,  80, 160, 200);
inline const IColor kLcdKnotGhostCh1  (130, 200, 130,  80);
inline const IColor kLcdKnotRing      (255,  10,  16,  22);
inline const IColor kLcdSelectedHalo  (255, 255, 255, 255);
inline const IColor kLcdTangentLine   (220, 220, 235, 200);
inline const IColor kLcdTangentHandle (255, 250, 220, 100);
inline const IColor kLcdLevelDotCh0   (255, 200, 240, 255);
inline const IColor kLcdLevelDotCh1   (255, 255, 220, 180);
inline const IColor kLcdGrLineCh0     (190, 130, 210, 240);
inline const IColor kLcdGrLineCh1     (190, 245, 190, 130);

// ----- Text styles. -----
inline const IText kTitleText(20, IColor(255, 200, 220, 230),
                              nullptr, EAlign::Center);
inline const IText kVersionText(10, IColor(255, 110, 140, 155),
                                nullptr, EAlign::Far);
inline const IText kLcdAxisLabelText(10, kLcdAxisLabel, nullptr, EAlign::Center);
inline const IText kLcdAxisLabelTextLeft(10, kLcdAxisLabel, nullptr, EAlign::Near);

// ----- Editable caption text styles (15 pt for both side-panel and matrix). -----
inline const IText kSideCaptionText  (15, IColor(255, 200, 220, 230),
                                      nullptr, EAlign::Center);
inline const IText kMatrixCaptionText(15, IColor(255, 200, 220, 230),
                                      nullptr, EAlign::Center);
// Background for the caption boxes — transparent so the panel shows through.
inline const IColor kCaptionBg(0, 0, 0, 0);

// ----- Matrix column-header / row-label text (15 pt Roboto-Bold). -----
inline const IText kMatrixHeaderText(15, IColor(255, 200, 220, 230),
                                     "Roboto-Bold", EAlign::Center);
inline const IText kRowLabelText    (15, IColor(255, 200, 220, 230),
                                     "Roboto-Bold", EAlign::Center);

// ----- The shared IVStyle for every panel control (knobs, switches, tabs,
//       meters). Dark base, cyan accents, slim 1 px frames. Label / value
//       text both at 15 pt so knob names, button face text, and matrix
//       headers line up with the 15 pt editable caption readouts.
inline const IVStyle kPanelStyle = iplug::igraphics::DEFAULT_STYLE
  .WithColor(iplug::igraphics::kBG, IColor(255,  20,  26,  32))
  .WithColor(iplug::igraphics::kFG, IColor(255,  60, 110, 130))
  .WithColor(iplug::igraphics::kPR, IColor(255,  80, 200, 230))
  .WithColor(iplug::igraphics::kFR, IColor(255,  60,  85, 100))
  .WithColor(iplug::igraphics::kHL, IColor(255, 120, 180, 200))
  .WithColor(iplug::igraphics::kSH, IColor(255,   4,   8,  12))
  .WithColor(iplug::igraphics::kX1, IColor(255, 240, 170,  90))
  .WithFrameThickness(1.f)
  .WithLabelText(IText(15, IColor(255, 200, 220, 230),
                       "Roboto-Bold", EAlign::Center))
  .WithValueText(IText(15, IColor(255, 200, 220, 230),
                       nullptr, EAlign::Center));

} // namespace iplug_helpers
