// Copyright (c) 2022-2023 the rbfx project.
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT> or the accompanying LICENSE file.

#pragma once

#include "Urho3D/Core/Object.h"

namespace Urho3D
{

/// VR session has started.
URHO3D_EVENT(E_VRSESSIONSTART, VRSessionStart)
{
}

/// Paused VR session is being resumed, such as headset put back on.
URHO3D_EVENT(E_VRRESUME, VRResume)
{
}

/// VR session is being paused, such as headset sensor reporting removed.
URHO3D_EVENT(E_VRPAUSE, VRPause)
{
}

/// An external source is terminating our VR instance.
URHO3D_EVENT(E_VREXIT, VRExit)
{
}

/// Interaction profile has been changed, which means bindings have been remapped.
URHO3D_EVENT(E_VRINTERACTIONPROFILECHANGED, VRInteractionProfileChanged)
{
}

/// An input binding has been changed.
URHO3D_EVENT(E_BINDINGCHANGED, VRBindingChange)
{
    URHO3D_PARAM(P_NAME, Name);     // String
    URHO3D_PARAM(P_DATA, Data);     // Variant
    URHO3D_PARAM(P_DELTA, Delta);  // Variant
    URHO3D_PARAM(P_EXTRADELTA, ExtraDelta); // Variant
    URHO3D_PARAM(P_ACTIVE, Active);     // bool
    URHO3D_PARAM(P_BINDING, Binding);   // Binding pointer
}

/// Controller model has been changed.
URHO3D_EVENT(E_VRCONTROLLERCHANGE, VRControllerChange)
{
    URHO3D_PARAM(P_HAND, Hand); // int
}

} // namespace Urho3D
