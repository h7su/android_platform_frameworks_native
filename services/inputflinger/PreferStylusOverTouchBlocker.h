/*
 * Copyright (C) 2022 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <optional>
#include <set>
#include "InputListener.h"

namespace android {

/**
 * When stylus is down, all touch is ignored.
 * TODO(b/210159205): delete this when simultaneous stylus and touch is supported
 */
class PreferStylusOverTouchBlocker {
public:
    /**
     * Process the provided event and emit 0 or more events that should be used instead of it.
     * In the majority of cases, the returned result will just be the provided args (array with
     * only 1 element), unmodified.
     *
     * If the gesture should be blocked, the returned result may be:
     *
     * a) An empty array, if the current event should just be ignored completely
     * b) An array of N elements, containing N-1 events with ACTION_CANCEL and the current event.
     *
     * The returned result is intended to be reinjected into the original event stream in
     * replacement of the incoming event.
     */
    std::vector<NotifyMotionArgs> processMotion(const NotifyMotionArgs& args);
    std::string dump() const;

    void notifyInputDevicesChanged(const std::vector<InputDeviceInfo>& inputDevices);

    void notifyDeviceReset(const NotifyDeviceResetArgs& args);

private:
    // Stores the device id's of styli that are currently down.
    std::set<int32_t /*deviceId*/> mActiveStyli;
    // For each device, store the last touch event as long as the touch is down. Upon liftoff,
    // the entry is erased.
    std::map<int32_t /*deviceId*/, NotifyMotionArgs> mLastTouchEvents;
    // Device ids of devices for which the current touch gesture is canceled, if it canceled as
    // HOVER_EXIT, device will remove when stylus goes up.
    std::map<int32_t /*deviceId*/, bool /*cancelAsHover*/> mCanceledDevices;
    // The device is hovering, it used be confirmed the hover action.
    std::set<int32_t /*deviceId*/> mDeviceHovering;

    // Device ids of input devices where we encountered simultaneous touch and stylus
    // events. For these devices, we don't do any event processing (nothing is blocked or altered).
    std::set<int32_t /*deviceId*/> mDevicesWithMixedToolType;
};

} // namespace android