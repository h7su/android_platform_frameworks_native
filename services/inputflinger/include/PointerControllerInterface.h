/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include <input/DisplayViewport.h>
#include <input/Input.h>
#include <utils/BitSet.h>

namespace android {

/**
 * Interface for tracking a mouse / touch pad pointer and touch pad spots.
 *
 * The spots are sprites on screen that visually represent the positions of
 * fingers
 *
 * The pointer controller is responsible for providing synchronization and for tracking
 * display orientation changes if needed. It works in the display panel's coordinate space, which
 * is the same coordinate space used by InputReader.
 */
class PointerControllerInterface {
protected:
    PointerControllerInterface() { }
    virtual ~PointerControllerInterface() { }

public:
    /* Gets the bounds of the region that the pointer can traverse.
     * Returns true if the bounds are available. */
    virtual bool getBounds(float* outMinX, float* outMinY,
            float* outMaxX, float* outMaxY) const = 0;

    /* Move the pointer. */
    virtual void move(float deltaX, float deltaY) = 0;

    /* Sets a mask that indicates which buttons are pressed. */
    virtual void setButtonState(int32_t buttonState) = 0;

    /* Gets a mask that indicates which buttons are pressed. */
    virtual int32_t getButtonState() const = 0;

    /* Sets the absolute location of the pointer. */
    virtual void setPosition(float x, float y) = 0;

    /* Gets the absolute location of the pointer. */
    virtual void getPosition(float* outX, float* outY) const = 0;

    enum class Transition {
        // Fade/unfade immediately.
        IMMEDIATE,
        // Fade/unfade gradually.
        GRADUAL,
    };

    /* Fades the pointer out now. */
    virtual void fade(Transition transition) = 0;

    /* Makes the pointer visible if it has faded out.
     * The pointer never unfades itself automatically.  This method must be called
     * by the client whenever the pointer is moved or a button is pressed and it
     * wants to ensure that the pointer becomes visible again. */
    virtual void unfade(Transition transition) = 0;

    enum class Presentation {
        // Show the mouse pointer.
        POINTER,
        // Show spots and a spot anchor in place of the mouse pointer.
        SPOT,
        // Show the stylus hover pointer.
        STYLUS_HOVER,

        ftl_last = STYLUS_HOVER,
    };

    /* Sets the mode of the pointer controller. */
    virtual void setPresentation(Presentation presentation) = 0;

    /* Sets the spots for the current gesture.
     * The spots are not subject to the inactivity timeout like the pointer
     * itself it since they are expected to remain visible for so long as
     * the fingers are on the touch pad.
     *
     * The values of the AMOTION_EVENT_AXIS_PRESSURE axis is significant.
     * For spotCoords, pressure != 0 indicates that the spot's location is being
     * pressed (not hovering).
     */
    virtual void setSpots(const PointerCoords* spotCoords, const uint32_t* spotIdToIndex,
            BitSet32 spotIdBits, int32_t displayId) = 0;

    /* Removes all spots. */
    virtual void clearSpots() = 0;

    /* Gets the id of the display where the pointer should be shown. */
    virtual int32_t getDisplayId() const = 0;

    /* Sets the associated display of this pointer. Pointer should show on that display. */
    virtual void setDisplayViewport(const DisplayViewport& displayViewport) = 0;
};

} // namespace android
