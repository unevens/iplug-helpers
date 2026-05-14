// iplug-helpers — controls/LightMarkerMeterControl.hpp
//
// IVMeterControl with light-coloured dB markers. Upstream's DrawMarkers
// hardcodes DEFAULT_TEXT (black) which is unreadable on the dark
// "spacestation" panel. This subclass reimplements Draw() to use a light
// text style for the "-24 dB" / "0 dB" / etc. tick labels alongside each
// meter.

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
