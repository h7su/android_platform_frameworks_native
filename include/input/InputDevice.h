/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef _LIBINPUT_INPUT_DEVICE_H
#define _LIBINPUT_INPUT_DEVICE_H

#include <input/Input.h>
#include <input/KeyCharacterMap.h>
#include <vector>

namespace android {

/*
 * Identifies a device.
 */
struct InputDeviceIdentifier {
    inline InputDeviceIdentifier() :
            bus(0), vendor(0), product(0), version(0) {
    }

    // Information provided by the kernel.
    std::string name;
    std::string location;
    std::string uniqueId;
    uint16_t bus;
    uint16_t vendor;
    uint16_t product;
    uint16_t version;

    // A composite input device descriptor string that uniquely identifies the device
    // even across reboots or reconnections.  The value of this field is used by
    // upper layers of the input system to associate settings with individual devices.
    // It is hashed from whatever kernel provided information is available.
    // Ideally, the way this value is computed should not change between Android releases
    // because that would invalidate persistent settings that rely on it.
    std::string descriptor;

    // A value added to uniquely identify a device in the absence of a unique id. This
    // is intended to be a minimum way to distinguish from other active devices and may
    // reuse values that are not associated with an input anymore.
    uint16_t nonce;

    /**
     * Return InputDeviceIdentifier.name that has been adjusted as follows:
     *     - all characters besides alphanumerics, dash,
     *       and underscore have been replaced with underscores.
     * This helps in situations where a file that matches the device name is needed,
     * while conforming to the filename limitations.
     */
    std::string getCanonicalName() const;
};

/*
 * Describes the characteristics and capabilities of an input device.
 */
class InputDeviceInfo {
public:
    InputDeviceInfo();
    InputDeviceInfo(const InputDeviceInfo& other);
    ~InputDeviceInfo();

    struct MotionRange {
        int32_t axis;
        uint32_t source;
        float min;
        float max;
        float flat;
        float fuzz;
        float resolution;
    };

    void initialize(int32_t id, int32_t generation, int32_t controllerNumber,
            const InputDeviceIdentifier& identifier, const std::string& alias, bool isExternal,
            bool hasMic);

    inline int32_t getId() const { return mId; }
    inline int32_t getControllerNumber() const { return mControllerNumber; }
    inline int32_t getGeneration() const { return mGeneration; }
    inline const InputDeviceIdentifier& getIdentifier() const { return mIdentifier; }
    inline const std::string& getAlias() const { return mAlias; }
    inline const std::string& getDisplayName() const {
        return mAlias.empty() ? mIdentifier.name : mAlias;
    }
    inline bool isExternal() const { return mIsExternal; }
    inline bool hasMic() const { return mHasMic; }
    inline uint32_t getSources() const { return mSources; }

    const MotionRange* getMotionRange(int32_t axis, uint32_t source) const;

    void addSource(uint32_t source);
    void addMotionRange(int32_t axis, uint32_t source,
            float min, float max, float flat, float fuzz, float resolution);
    void addMotionRange(const MotionRange& range);

    inline void setKeyboardType(int32_t keyboardType) { mKeyboardType = keyboardType; }
    inline int32_t getKeyboardType() const { return mKeyboardType; }

    inline void setKeyCharacterMap(const sp<KeyCharacterMap>& value) {
        mKeyCharacterMap = value;
    }

    inline sp<KeyCharacterMap> getKeyCharacterMap() const {
        return mKeyCharacterMap;
    }

    inline void setVibrator(bool hasVibrator) { mHasVibrator = hasVibrator; }
    inline bool hasVibrator() const { return mHasVibrator; }

    inline void setButtonUnderPad(bool hasButton) { mHasButtonUnderPad = hasButton; }
    inline bool hasButtonUnderPad() const { return mHasButtonUnderPad; }

    inline const std::vector<MotionRange>& getMotionRanges() const {
        return mMotionRanges;
    }

private:
    int32_t mId;
    int32_t mGeneration;
    int32_t mControllerNumber;
    InputDeviceIdentifier mIdentifier;
    std::string mAlias;
    bool mIsExternal;
    bool mHasMic;
    uint32_t mSources;
    int32_t mKeyboardType;
    sp<KeyCharacterMap> mKeyCharacterMap;
    bool mHasVibrator;
    bool mHasButtonUnderPad;

    std::vector<MotionRange> mMotionRanges;
};

/* Types of input device configuration files. */
enum InputDeviceConfigurationFileType {
    INPUT_DEVICE_CONFIGURATION_FILE_TYPE_CONFIGURATION = 0,     /* .idc file */
    INPUT_DEVICE_CONFIGURATION_FILE_TYPE_KEY_LAYOUT = 1,        /* .kl file */
    INPUT_DEVICE_CONFIGURATION_FILE_TYPE_KEY_CHARACTER_MAP = 2, /* .kcm file */
};

/*
 * Gets the path of an input device configuration file, if one is available.
 * Considers both system provided and user installed configuration files.
 *
 * The device identifier is used to construct several default configuration file
 * names to try based on the device name, vendor, product, and version.
 *
 * Returns an empty string if not found.
 */
extern std::string getInputDeviceConfigurationFilePathByDeviceIdentifier(
        const InputDeviceIdentifier& deviceIdentifier,
        InputDeviceConfigurationFileType type);

/*
 * Gets the path of an input device configuration file, if one is available.
 * Considers both system provided and user installed configuration files.
 *
 * The name is case-sensitive and is used to construct the filename to resolve.
 * All characters except 'a'-'z', 'A'-'Z', '0'-'9', '-', and '_' are replaced by underscores.
 *
 * Returns an empty string if not found.
 */
extern std::string getInputDeviceConfigurationFilePathByName(
        const std::string& name, InputDeviceConfigurationFileType type);

enum ReservedInputDeviceId : int32_t {
    // Device id of a special "virtual" keyboard that is always present.
    VIRTUAL_KEYBOARD_ID = -1,
    // Device id of the "built-in" keyboard if there is one.
    BUILT_IN_KEYBOARD_ID = 0,
    // First device id available for dynamic devices
    END_RESERVED_ID = 1,
};

} // namespace android

#endif // _LIBINPUT_INPUT_DEVICE_H
