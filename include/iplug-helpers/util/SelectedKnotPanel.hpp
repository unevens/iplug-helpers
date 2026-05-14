// iplug-helpers — util/SelectedKnotPanel.hpp
//
// Two tiny primitives for plugins that keep a "knot panel" — a row of
// side-panel knobs (X / Y / Tangent / Smoothness / Link…) whose param
// bindings switch every time the user picks a different spline knot.
//
//   RebindControlToParam(plugin, ctrl, paramIdx)
//       Used inside SetSelectedKnot(...). SetParamIdx alone updates
//       mVals[].idx but leaves mVals[].value at the previously-bound
//       param's value, so without an explicit pull the knob briefly
//       displays the wrong value until the next host SetValueFromDelegate
//       cycle. This helper pulls the new param's normalized value
//       through immediately and marks the control dirty.
//
//   SyncControlFromParam(plugin, ctrl)
//       Used inside OnIdle. The spline editor changes knot params via
//       SendParameterValueFromUI directly (no mVals tying the editor to
//       those params), so iPlug2's automatic UpdatePeers path doesn't
//       reach the side-panel knobs — without a pull they freeze on
//       whatever they showed at the last click. Call this once per
//       cached side-panel pointer per OnIdle tick.
//
// Both functions accept a null `ctrl` (returns immediately) — convenient
// for cached pointers that the editor's OnUIClose nulled.

#pragma once

#include "IPlugParameter.h"
#include "IControl.h"

namespace iplug_helpers {

// Rebind a control to a new param index AND push the new normalized
// value through immediately. Mark dirty without triggering a host
// notification (SetDirty(false)) since the param itself hasn't changed.
template <class Plugin>
inline void RebindControlToParam(
  Plugin& plugin,
  iplug::igraphics::IControl* ctrl,
  int paramIdx)
{
  if (!ctrl) return;
  ctrl->SetParamIdx(paramIdx);
  if (const iplug::IParam* p = plugin.GetParam(paramIdx)) {
    ctrl->SetValue(p->GetNormalized());
  }
  ctrl->SetDirty(false);
}

// Pull the control's currently-bound param value back into the control,
// so a panel knob whose param was changed elsewhere (e.g. via the spline
// editor's direct SendParameterValueFromUI path) tracks the live value.
template <class Plugin>
inline void SyncControlFromParam(
  Plugin& plugin,
  iplug::igraphics::IControl* ctrl)
{
  if (!ctrl) return;
  const int idx = ctrl->GetParamIdx();
  if (idx < 0) return;
  if (const iplug::IParam* p = plugin.GetParam(idx)) {
    ctrl->SetValueFromDelegate(p->GetNormalized());
  }
}

} // namespace iplug_helpers
