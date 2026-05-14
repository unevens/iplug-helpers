// iplug-helpers — util/LinkablePairPropagation.hpp
//
// Linkable-pair propagation for plugins that store params in the
// (ch0, ch1, link-bool) triple layout used by Curvessor and Overdraw.
// When the user drags ch0 or ch1 of a linked pair, copy the new value
// to the other channel so both knobs stay in sync during the drag (and
// at rest, so unlinking later doesn't pop ch1 to a stale value).
//
// Usage: call from the plugin's OnParamChangeUI override.
//
//   void MyPlugin::OnParamChangeUI(int paramIdx, EParamSource source) {
//   #if IPLUG_EDITOR
//     static constexpr int kLinkableCh0Bases[] = {
//       kInputGain_ch0, kOutputGain_ch0, kWet_ch0, ...
//     };
//     iplug_helpers::PropagateLinkedParam(
//       *this, paramIdx, source, kLinkableCh0Bases, mPropagatingLinkChange);
//   #endif
//   }
//
// `mPropagatingLinkChange` is a bool member on the plugin used to guard
// against recursion: SendParameterValueFromUI(other) re-enters
// OnParamChangeUI for the other channel, which would otherwise try to
// mirror back to the first and loop forever.

#pragma once

#include "IPlugStructs.h"   // EParamSource
#include "IPlugParameter.h"
#include "IGraphics.h"
#include "IControl.h"

#include <cmath>
#include <cstddef>

namespace iplug_helpers {

// Returns true if it handled the propagation (paramIdx matched a
// linkable pair); the caller doesn't need that return value because all
// the work happens through SendParameterValueFromUI + ForControlWithParam
// side effects, but it's there if the plugin wants to short-circuit
// further work.
//
// Templated on the plugin class only so we can call
// `plugin.GetParam(...)`, `plugin.SendParameterValueFromUI(...)`, and
// `plugin.GetUI()` without dragging in the full iPlug2 class hierarchy.
template <class Plugin, std::size_t N>
bool PropagateLinkedParam(
  Plugin& plugin,
  int paramIdx,
  iplug::EParamSource source,
  const int (&linkableCh0Bases)[N],
  bool& propagatingFlag)
{
  if (propagatingFlag) return true;

  // Only propagate user-driven UI changes. Skip kHost / kPresetRecall /
  // kReset / kUnknown so that:
  //  - auval's "did the value persist after init?" check isn't broken by
  //    cross-channel cascades during the test setter;
  //  - preset / state restoration doesn't have one channel overwriting
  //    the other on the way in.
  if (source != iplug::kUI) return false;

  for (int ch0 : linkableCh0Bases) {
    if (paramIdx != ch0 && paramIdx != ch0 + 1) continue;

    // Linked? If not, leave the channels independent.
    if (!plugin.GetParam(ch0 + 2)->Bool()) return true;

    const int other = (paramIdx == ch0) ? (ch0 + 1) : ch0;
    const double norm = plugin.GetParam(paramIdx)->GetNormalized();

    // No-op if already in sync — avoids gratuitous SetDirty churn.
    if (std::abs(plugin.GetParam(other)->GetNormalized() - norm) < 1e-9)
      return true;

    // Update the param itself (so DSP sees the new value when the
    // toggle later flips to off, and so the host's automation lane
    // shows both channels moving together) and push the new value into
    // any UI knob currently bound to it. The host-side notification
    // fires through SendParameterValueFromUI; the UI-side update needs
    // the explicit SetValueFromDelegate because the calling knob
    // doesn't have `other` in its mVals so iPlug2's automatic
    // UpdatePeers path doesn't reach.
    propagatingFlag = true;
    plugin.SendParameterValueFromUI(other, norm);
    if (auto* ui = plugin.GetUI()) {
      ui->ForControlWithParam(other,
        [norm](iplug::igraphics::IControl* ctrl) {
          ctrl->SetValueFromDelegate(norm);
        });
    }
    propagatingFlag = false;
    return true;
  }
  return false;
}

} // namespace iplug_helpers
