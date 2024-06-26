/*
 * Copyright (C) 2013 The Android Open Source Project
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

#ifndef ANDROID_BATTERYSERVICE_H
#define ANDROID_BATTERYSERVICE_H

#include <sys/types.h>
#include <utils/Errors.h>
#include <utils/String8.h>

namespace android {

#include "BatteryServiceConstants.h"

// must be kept in sync with definitions in
// frameworks/base/core/java/android/os/BatteryManager.java
enum {
    BATTERY_PROP_CHARGE_COUNTER = 1, // equals BATTERY_PROPERTY_CHARGE_COUNTER
    BATTERY_PROP_CURRENT_NOW = 2, // equals BATTERY_PROPERTY_CURRENT_NOW
    BATTERY_PROP_CURRENT_AVG = 3, // equals BATTERY_PROPERTY_CURRENT_AVERAGE
    BATTERY_PROP_CAPACITY = 4, // equals BATTERY_PROPERTY_CAPACITY
    BATTERY_PROP_ENERGY_COUNTER = 5, // equals BATTERY_PROPERTY_ENERGY_COUNTER
    BATTERY_PROP_BATTERY_STATUS = 6, // equals BATTERY_PROPERTY_BATTERY_STATUS
    BATTERY_PROP_MANUFACTURING_DATE = 7, // equals BATTERY_PROPERTY_MANUFACTURING_DATE
    BATTERY_PROP_FIRST_USAGE_DATE = 8, // equals BATTERY_PROPERTY_FIRST_USAGE_DATE
    BATTERY_PROP_CHARGING_POLICY = 9, // equals BATTERY_PROPERTY_CHARGING_POLICY
    BATTERY_PROP_STATE_OF_HEALTH = 10, // equals BATTERY_PROPERTY_STATE_OF_HEALTH
    BATTERY_PROP_PART_STATUS = 12, // equals BATTERY_PROPERTY_PART_STATUS
};

struct BatteryProperties {
    bool chargerAcOnline;
    bool chargerUsbOnline;
    bool chargerWirelessOnline;
    bool chargerDockOnline;
    int maxChargingCurrent;
    int maxChargingVoltage;
    int batteryStatus;
    int batteryHealth;
    bool batteryPresent;
    int batteryLevel;
    int batteryVoltage;
    int batteryTemperature;
    int batteryCurrent;
    int batteryCycleCount;
    int batteryFullCharge;
    int batteryChargeCounter;
    String8 batteryTechnology;
};

struct BatteryProperty {
    int64_t valueInt64;
};

}; // namespace android

#endif // ANDROID_BATTERYSERVICE_H
