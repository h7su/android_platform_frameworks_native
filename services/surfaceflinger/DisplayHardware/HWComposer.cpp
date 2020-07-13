/*
 * Copyright (C) 2010 The Android Open Source Project
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

// #define LOG_NDEBUG 0

#undef LOG_TAG
#define LOG_TAG "HWComposer"
#define ATRACE_TAG ATRACE_TAG_GRAPHICS

#include <compositionengine/Output.h>
#include <compositionengine/OutputLayer.h>
#include <compositionengine/impl/OutputLayerCompositionState.h>
#include <log/log.h>
#include <ui/DebugUtils.h>
#include <ui/GraphicBuffer.h>
#include <utils/Errors.h>
#include <utils/Trace.h>

#include "HWComposer.h"
#include "HWC2.h"
#include "ComposerHal.h"

#include "../Layer.h"           // needed only for debugging
#include "../SurfaceFlinger.h"

#define LOG_HWC_DISPLAY_ERROR(hwcDisplayId, msg) \
    ALOGE("%s failed for HWC display %" PRIu64 ": %s", __FUNCTION__, hwcDisplayId, msg)

#define LOG_DISPLAY_ERROR(displayId, msg) \
    ALOGE("%s failed for display %s: %s", __FUNCTION__, to_string(displayId).c_str(), msg)

#define LOG_HWC_ERROR(what, error, displayId)                          \
    ALOGE("%s: %s failed for display %s: %s (%d)", __FUNCTION__, what, \
          to_string(displayId).c_str(), to_string(error).c_str(), static_cast<int32_t>(error))

#define RETURN_IF_INVALID_DISPLAY(displayId, ...)            \
    do {                                                     \
        if (mDisplayData.count(displayId) == 0) {            \
            LOG_DISPLAY_ERROR(displayId, "Invalid display"); \
            return __VA_ARGS__;                              \
        }                                                    \
    } while (false)

#define RETURN_IF_HWC_ERROR_FOR(what, error, displayId, ...) \
    do {                                                     \
        if (error != HWC2::Error::None) {                    \
            LOG_HWC_ERROR(what, error, displayId);           \
            return __VA_ARGS__;                              \
        }                                                    \
    } while (false)

#define RETURN_IF_HWC_ERROR(error, displayId, ...) \
    RETURN_IF_HWC_ERROR_FOR(__FUNCTION__, error, displayId, __VA_ARGS__)

namespace {

using android::hardware::Return;
using android::hardware::Void;

class ComposerCallbackBridge : public android::Hwc2::IComposerCallback {
public:
    ComposerCallbackBridge(HWC2::ComposerCallback* callback, int32_t sequenceId)
          : mCallback(callback), mSequenceId(sequenceId) {}

    Return<void> onHotplug(android::Hwc2::Display display,
                           IComposerCallback::Connection conn) override {
        HWC2::Connection connection = static_cast<HWC2::Connection>(conn);
        mCallback->onHotplugReceived(mSequenceId, display, connection);
        return Void();
    }

    Return<void> onRefresh(android::Hwc2::Display display) override {
        mCallback->onRefreshReceived(mSequenceId, display);
        return Void();
    }

    Return<void> onVsync(android::Hwc2::Display display, int64_t timestamp) override {
        mCallback->onVsyncReceived(mSequenceId, display, timestamp);
        return Void();
    }

private:
    HWC2::ComposerCallback* mCallback;
    const int32_t mSequenceId;
};

} // namespace

namespace android {

HWComposer::~HWComposer() = default;

namespace impl {

HWComposer::HWComposer(std::unique_ptr<Hwc2::Composer> composer) : mComposer(std::move(composer)) {
    loadCapabilities();
}

HWComposer::HWComposer(const std::string& composerServiceName)
      : mComposer(std::make_unique<Hwc2::impl::Composer>(composerServiceName)) {
    loadCapabilities();
}

HWComposer::~HWComposer() {
    mDisplayData.clear();
}

void HWComposer::registerCallback(HWC2::ComposerCallback* callback, int32_t sequenceId) {
    if (mRegisteredCallback) {
        ALOGW("Callback already registered. Ignored extra registration attempt.");
        return;
    }
    mRegisteredCallback = true;
    sp<ComposerCallbackBridge> callbackBridge(new ComposerCallbackBridge(callback, sequenceId));
    mComposer->registerCallback(callbackBridge);
}

bool HWComposer::getDisplayIdentificationData(hwc2_display_t hwcDisplayId, uint8_t* outPort,
                                              DisplayIdentificationData* outData) const {
    const auto error = static_cast<HWC2::Error>(
            mComposer->getDisplayIdentificationData(hwcDisplayId, outPort, outData));
    if (error != HWC2::Error::None) {
        if (error != HWC2::Error::Unsupported) {
            LOG_HWC_DISPLAY_ERROR(hwcDisplayId, to_string(error).c_str());
        }
        return false;
    }
    return true;
}

bool HWComposer::hasCapability(HWC2::Capability capability) const {
    return mCapabilities.count(capability) > 0;
}

bool HWComposer::hasDisplayCapability(const std::optional<DisplayId>& displayId,
                                      HWC2::DisplayCapability capability) const {
    if (!displayId) {
        // Checkout global capabilities for displays without a corresponding HWC display.
        if (capability == HWC2::DisplayCapability::SkipClientColorTransform) {
            return hasCapability(HWC2::Capability::SkipClientColorTransform);
        }
        return false;
    }
    RETURN_IF_INVALID_DISPLAY(*displayId, false);
    return mDisplayData.at(*displayId).hwcDisplay->getCapabilities().count(capability) > 0;
}

void HWComposer::validateChange(HWC2::Composition from, HWC2::Composition to) {
    bool valid = true;
    switch (from) {
        case HWC2::Composition::Client:
            valid = false;
            break;
        case HWC2::Composition::Device:
        case HWC2::Composition::SolidColor:
            valid = (to == HWC2::Composition::Client);
            break;
        case HWC2::Composition::Cursor:
        case HWC2::Composition::Sideband:
            valid = (to == HWC2::Composition::Client ||
                    to == HWC2::Composition::Device);
            break;
        default:
            break;
    }

    if (!valid) {
        ALOGE("Invalid layer type change: %s --> %s", to_string(from).c_str(),
                to_string(to).c_str());
    }
}

std::optional<DisplayIdentificationInfo> HWComposer::onHotplug(hwc2_display_t hwcDisplayId,
                                                               HWC2::Connection connection) {
    switch (connection) {
        case HWC2::Connection::Connected:
            return onHotplugConnect(hwcDisplayId);
        case HWC2::Connection::Disconnected:
            return onHotplugDisconnect(hwcDisplayId);
        case HWC2::Connection::Invalid:
            return {};
    }
}

bool HWComposer::onVsync(hwc2_display_t hwcDisplayId, int64_t timestamp) {
    const auto displayId = toPhysicalDisplayId(hwcDisplayId);
    if (!displayId) {
        LOG_HWC_DISPLAY_ERROR(hwcDisplayId, "Invalid HWC display");
        return false;
    }

    RETURN_IF_INVALID_DISPLAY(*displayId, false);

    auto& displayData = mDisplayData[*displayId];
    if (displayData.isVirtual) {
        LOG_DISPLAY_ERROR(*displayId, "Invalid operation on virtual display");
        return false;
    }

    {
        std::lock_guard lock(displayData.lastHwVsyncLock);

        // There have been reports of HWCs that signal several vsync events
        // with the same timestamp when turning the display off and on. This
        // is a bug in the HWC implementation, but filter the extra events
        // out here so they don't cause havoc downstream.
        if (timestamp == displayData.lastHwVsync) {
            ALOGW("Ignoring duplicate VSYNC event from HWC for display %s (t=%" PRId64 ")",
                  to_string(*displayId).c_str(), timestamp);
            return false;
        }

        displayData.lastHwVsync = timestamp;
    }

    const auto tag = "HW_VSYNC_" + to_string(*displayId);
    ATRACE_INT(tag.c_str(), displayData.vsyncTraceToggle);
    displayData.vsyncTraceToggle = !displayData.vsyncTraceToggle;

    return true;
}

std::optional<DisplayId> HWComposer::allocateVirtualDisplay(uint32_t width, uint32_t height,
                                                            ui::PixelFormat* format) {
    if (mRemainingHwcVirtualDisplays == 0) {
        ALOGE("%s: No remaining virtual displays", __FUNCTION__);
        return {};
    }

    if (SurfaceFlinger::maxVirtualDisplaySize != 0 &&
        (width > SurfaceFlinger::maxVirtualDisplaySize ||
         height > SurfaceFlinger::maxVirtualDisplaySize)) {
        ALOGE("%s: Display size %ux%u exceeds maximum dimension of %" PRIu64, __FUNCTION__, width,
              height, SurfaceFlinger::maxVirtualDisplaySize);
        return {};
    }
    hwc2_display_t hwcDisplayId = 0;
    const auto error = static_cast<HWC2::Error>(
            mComposer->createVirtualDisplay(width, height, format, &hwcDisplayId));
    if (error != HWC2::Error::None) {
        ALOGE("%s: Failed to create HWC virtual display", __FUNCTION__);
        return {};
    }
    auto display = std::make_unique<HWC2::impl::Display>(*mComposer.get(), mCapabilities,
                                                         hwcDisplayId, HWC2::DisplayType::Virtual);
    display->setConnected(true);

    DisplayId displayId;
    if (mFreeVirtualDisplayIds.empty()) {
        displayId = getVirtualDisplayId(mNextVirtualDisplayId++);
    } else {
        displayId = *mFreeVirtualDisplayIds.begin();
        mFreeVirtualDisplayIds.erase(displayId);
    }

    auto& displayData = mDisplayData[displayId];
    displayData.hwcDisplay = std::move(display);
    displayData.isVirtual = true;

    --mRemainingHwcVirtualDisplays;
    return displayId;
}

void HWComposer::allocatePhysicalDisplay(hwc2_display_t hwcDisplayId, DisplayId displayId) {
    if (!mInternalHwcDisplayId) {
        mInternalHwcDisplayId = hwcDisplayId;
    } else if (mInternalHwcDisplayId != hwcDisplayId && !mExternalHwcDisplayId) {
        mExternalHwcDisplayId = hwcDisplayId;
    }

    auto& displayData = mDisplayData[displayId];
    auto newDisplay =
            std::make_unique<HWC2::impl::Display>(*mComposer.get(), mCapabilities, hwcDisplayId,
                                                  HWC2::DisplayType::Physical);
    newDisplay->setConnected(true);
    displayData.hwcDisplay = std::move(newDisplay);
    mPhysicalDisplayIdMap[hwcDisplayId] = displayId;
}

HWC2::Layer* HWComposer::createLayer(DisplayId displayId) {
    RETURN_IF_INVALID_DISPLAY(displayId, nullptr);

    HWC2::Layer* layer;
    auto error = mDisplayData[displayId].hwcDisplay->createLayer(&layer);
    RETURN_IF_HWC_ERROR(error, displayId, nullptr);
    return layer;
}

void HWComposer::destroyLayer(DisplayId displayId, HWC2::Layer* layer) {
    RETURN_IF_INVALID_DISPLAY(displayId);

    auto error = mDisplayData[displayId].hwcDisplay->destroyLayer(layer);
    RETURN_IF_HWC_ERROR(error, displayId);
}

nsecs_t HWComposer::getRefreshTimestamp(DisplayId displayId) const {
    RETURN_IF_INVALID_DISPLAY(displayId, 0);
    const auto& displayData = mDisplayData.at(displayId);
    // this returns the last refresh timestamp.
    // if the last one is not available, we estimate it based on
    // the refresh period and whatever closest timestamp we have.
    std::lock_guard lock(displayData.lastHwVsyncLock);
    nsecs_t now = systemTime(CLOCK_MONOTONIC);
    const auto config = getActiveConfig(displayId);
    if (!config) return 0;
    auto vsyncPeriod = config->getVsyncPeriod();
    return now - ((now - displayData.lastHwVsync) % vsyncPeriod);
}

bool HWComposer::isConnected(DisplayId displayId) const {
    RETURN_IF_INVALID_DISPLAY(displayId, false);
    return mDisplayData.at(displayId).hwcDisplay->isConnected();
}

std::vector<std::shared_ptr<const HWC2::Display::Config>> HWComposer::getConfigs(
        DisplayId displayId) const {
    RETURN_IF_INVALID_DISPLAY(displayId, {});

    const auto& displayData = mDisplayData.at(displayId);
    auto configs = displayData.hwcDisplay->getConfigs();
    if (displayData.configMap.empty()) {
        for (size_t i = 0; i < configs.size(); ++i) {
            displayData.configMap[i] = configs[i];
        }
    }
    return configs;
}

std::shared_ptr<const HWC2::Display::Config> HWComposer::getActiveConfig(
        DisplayId displayId) const {
    RETURN_IF_INVALID_DISPLAY(displayId, nullptr);

    std::shared_ptr<const HWC2::Display::Config> config;
    auto error = mDisplayData.at(displayId).hwcDisplay->getActiveConfig(&config);
    if (error == HWC2::Error::BadConfig) {
        LOG_DISPLAY_ERROR(displayId, "No active config");
        return nullptr;
    }

    RETURN_IF_HWC_ERROR(error, displayId, nullptr);

    if (!config) {
        LOG_DISPLAY_ERROR(displayId, "Unknown config");
        return nullptr;
    }

    return config;
}

int HWComposer::getActiveConfigIndex(DisplayId displayId) const {
    RETURN_IF_INVALID_DISPLAY(displayId, -1);

    int index;
    auto error = mDisplayData.at(displayId).hwcDisplay->getActiveConfigIndex(&index);
    if (error == HWC2::Error::BadConfig) {
        LOG_DISPLAY_ERROR(displayId, "No active config");
        return -1;
    }

    RETURN_IF_HWC_ERROR(error, displayId, -1);

    if (index < 0) {
        LOG_DISPLAY_ERROR(displayId, "Unknown config");
        return -1;
    }

    return index;
}

std::vector<ui::ColorMode> HWComposer::getColorModes(DisplayId displayId) const {
    RETURN_IF_INVALID_DISPLAY(displayId, {});

    std::vector<ui::ColorMode> modes;
    auto error = mDisplayData.at(displayId).hwcDisplay->getColorModes(&modes);
    RETURN_IF_HWC_ERROR(error, displayId, {});
    return modes;
}

status_t HWComposer::setActiveColorMode(DisplayId displayId, ui::ColorMode mode,
                                        ui::RenderIntent renderIntent) {
    RETURN_IF_INVALID_DISPLAY(displayId, BAD_INDEX);

    auto& displayData = mDisplayData[displayId];
    auto error = displayData.hwcDisplay->setColorMode(mode, renderIntent);
    RETURN_IF_HWC_ERROR_FOR(("setColorMode(" + decodeColorMode(mode) + ", " +
                             decodeRenderIntent(renderIntent) + ")")
                                    .c_str(),
                            error, displayId, UNKNOWN_ERROR);

    return NO_ERROR;
}

void HWComposer::setVsyncEnabled(DisplayId displayId, HWC2::Vsync enabled) {
    RETURN_IF_INVALID_DISPLAY(displayId);
    auto& displayData = mDisplayData[displayId];

    if (displayData.isVirtual) {
        LOG_DISPLAY_ERROR(displayId, "Invalid operation on virtual display");
        return;
    }

    // NOTE: we use our own internal lock here because we have to call
    // into the HWC with the lock held, and we want to make sure
    // that even if HWC blocks (which it shouldn't), it won't
    // affect other threads.
    std::lock_guard lock(displayData.vsyncEnabledLock);
    if (enabled == displayData.vsyncEnabled) {
        return;
    }

    ATRACE_CALL();
    auto error = displayData.hwcDisplay->setVsyncEnabled(enabled);
    RETURN_IF_HWC_ERROR(error, displayId);

    displayData.vsyncEnabled = enabled;

    const auto tag = "HW_VSYNC_ON_" + to_string(displayId);
    ATRACE_INT(tag.c_str(), enabled == HWC2::Vsync::Enable ? 1 : 0);
}

status_t HWComposer::setClientTarget(DisplayId displayId, uint32_t slot,
                                     const sp<Fence>& acquireFence, const sp<GraphicBuffer>& target,
                                     ui::Dataspace dataspace) {
    RETURN_IF_INVALID_DISPLAY(displayId, BAD_INDEX);

    ALOGV("%s for display %s", __FUNCTION__, to_string(displayId).c_str());
    auto& hwcDisplay = mDisplayData[displayId].hwcDisplay;
    auto error = hwcDisplay->setClientTarget(slot, target, acquireFence, dataspace);
    RETURN_IF_HWC_ERROR(error, displayId, BAD_VALUE);
    return NO_ERROR;
}

status_t HWComposer::prepare(DisplayId displayId, const compositionengine::Output& output) {
    ATRACE_CALL();

    RETURN_IF_INVALID_DISPLAY(displayId, BAD_INDEX);

    auto& displayData = mDisplayData[displayId];
    auto& hwcDisplay = displayData.hwcDisplay;
    if (!hwcDisplay->isConnected()) {
        return NO_ERROR;
    }

    uint32_t numTypes = 0;
    uint32_t numRequests = 0;

    HWC2::Error error = HWC2::Error::None;

    // First try to skip validate altogether when there is no client
    // composition.  When there is client composition, since we haven't
    // rendered to the client target yet, we should not attempt to skip
    // validate.
    //
    // displayData.hasClientComposition hasn't been updated for this frame.
    // The check below is incorrect.  We actually rely on HWC here to fall
    // back to validate when there is any client layer.
    displayData.validateWasSkipped = false;
    if (!displayData.hasClientComposition) {
        sp<Fence> outPresentFence;
        uint32_t state = UINT32_MAX;
        error = hwcDisplay->presentOrValidate(&numTypes, &numRequests, &outPresentFence , &state);
        if (error != HWC2::Error::HasChanges) {
            RETURN_IF_HWC_ERROR_FOR("presentOrValidate", error, displayId, UNKNOWN_ERROR);
        }
        if (state == 1) { //Present Succeeded.
            std::unordered_map<HWC2::Layer*, sp<Fence>> releaseFences;
            error = hwcDisplay->getReleaseFences(&releaseFences);
            displayData.releaseFences = std::move(releaseFences);
            displayData.lastPresentFence = outPresentFence;
            displayData.validateWasSkipped = true;
            displayData.presentError = error;
            return NO_ERROR;
        }
        // Present failed but Validate ran.
    } else {
        error = hwcDisplay->validate(&numTypes, &numRequests);
    }
    ALOGV("SkipValidate failed, Falling back to SLOW validate/present");
    if (error != HWC2::Error::HasChanges) {
        RETURN_IF_HWC_ERROR_FOR("validate", error, displayId, BAD_INDEX);
    }

    std::unordered_map<HWC2::Layer*, HWC2::Composition> changedTypes;
    changedTypes.reserve(numTypes);
    error = hwcDisplay->getChangedCompositionTypes(&changedTypes);
    RETURN_IF_HWC_ERROR_FOR("getChangedCompositionTypes", error, displayId, BAD_INDEX);

    displayData.displayRequests = static_cast<HWC2::DisplayRequest>(0);
    std::unordered_map<HWC2::Layer*, HWC2::LayerRequest> layerRequests;
    layerRequests.reserve(numRequests);
    error = hwcDisplay->getRequests(&displayData.displayRequests,
            &layerRequests);
    RETURN_IF_HWC_ERROR_FOR("getRequests", error, displayId, BAD_INDEX);

    displayData.hasClientComposition = false;
    displayData.hasDeviceComposition = false;
    for (auto& outputLayer : output.getOutputLayersOrderedByZ()) {
        auto& state = outputLayer->editState();
        LOG_FATAL_IF(!state.hwc.);
        auto hwcLayer = (*state.hwc).hwcLayer;

        if (auto it = changedTypes.find(hwcLayer.get()); it != changedTypes.end()) {
            auto newCompositionType = it->second;
            validateChange(static_cast<HWC2::Composition>((*state.hwc).hwcCompositionType),
                           newCompositionType);
            (*state.hwc).hwcCompositionType =
                    static_cast<Hwc2::IComposerClient::Composition>(newCompositionType);
        }

        switch ((*state.hwc).hwcCompositionType) {
            case Hwc2::IComposerClient::Composition::CLIENT:
                displayData.hasClientComposition = true;
                break;
            case Hwc2::IComposerClient::Composition::DEVICE:
            case Hwc2::IComposerClient::Composition::SOLID_COLOR:
            case Hwc2::IComposerClient::Composition::CURSOR:
            case Hwc2::IComposerClient::Composition::SIDEBAND:
                displayData.hasDeviceComposition = true;
                break;
            default:
                break;
        }

        state.clearClientTarget = false;
        if (auto it = layerRequests.find(hwcLayer.get()); it != layerRequests.end()) {
            auto request = it->second;
            if (request == HWC2::LayerRequest::ClearClientTarget) {
                state.clearClientTarget = true;
            } else {
                LOG_DISPLAY_ERROR(displayId,
                                  ("Unknown layer request " + to_string(request)).c_str());
            }
        }
    }

    error = hwcDisplay->acceptChanges();
    RETURN_IF_HWC_ERROR_FOR("acceptChanges", error, displayId, BAD_INDEX);

    return NO_ERROR;
}

bool HWComposer::hasDeviceComposition(const std::optional<DisplayId>& displayId) const {
    if (!displayId) {
        // Displays without a corresponding HWC display are never composed by
        // the device
        return false;
    }

    RETURN_IF_INVALID_DISPLAY(*displayId, false);
    return mDisplayData.at(*displayId).hasDeviceComposition;
}

bool HWComposer::hasFlipClientTargetRequest(const std::optional<DisplayId>& displayId) const {
    if (!displayId) {
        // Displays without a corresponding HWC display are never composed by
        // the device
        return false;
    }

    RETURN_IF_INVALID_DISPLAY(*displayId, false);
    return ((static_cast<uint32_t>(mDisplayData.at(*displayId).displayRequests) &
             static_cast<uint32_t>(HWC2::DisplayRequest::FlipClientTarget)) != 0);
}

bool HWComposer::hasClientComposition(const std::optional<DisplayId>& displayId) const {
    if (!displayId) {
        // Displays without a corresponding HWC display are always composed by
        // the client
        return true;
    }

    RETURN_IF_INVALID_DISPLAY(*displayId, true);
    return mDisplayData.at(*displayId).hasClientComposition;
}

sp<Fence> HWComposer::getPresentFence(DisplayId displayId) const {
    RETURN_IF_INVALID_DISPLAY(displayId, Fence::NO_FENCE);
    return mDisplayData.at(displayId).lastPresentFence;
}

sp<Fence> HWComposer::getLayerReleaseFence(DisplayId displayId, HWC2::Layer* layer) const {
    RETURN_IF_INVALID_DISPLAY(displayId, Fence::NO_FENCE);
    auto displayFences = mDisplayData.at(displayId).releaseFences;
    if (displayFences.count(layer) == 0) {
        ALOGV("getLayerReleaseFence: Release fence not found");
        return Fence::NO_FENCE;
    }
    return displayFences[layer];
}

status_t HWComposer::presentAndGetReleaseFences(DisplayId displayId) {
    ATRACE_CALL();

    RETURN_IF_INVALID_DISPLAY(displayId, BAD_INDEX);

    auto& displayData = mDisplayData[displayId];
    auto& hwcDisplay = displayData.hwcDisplay;

    if (displayData.validateWasSkipped) {
        // explicitly flush all pending commands
        auto error = static_cast<HWC2::Error>(mComposer->executeCommands());
        RETURN_IF_HWC_ERROR_FOR("executeCommands", error, displayId, UNKNOWN_ERROR);
        RETURN_IF_HWC_ERROR_FOR("present", displayData.presentError, displayId, UNKNOWN_ERROR);
        return NO_ERROR;
    }

    auto error = hwcDisplay->present(&displayData.lastPresentFence);
    RETURN_IF_HWC_ERROR_FOR("present", error, displayId, UNKNOWN_ERROR);

    std::unordered_map<HWC2::Layer*, sp<Fence>> releaseFences;
    error = hwcDisplay->getReleaseFences(&releaseFences);
    RETURN_IF_HWC_ERROR_FOR("getReleaseFences", error, displayId, UNKNOWN_ERROR);

    displayData.releaseFences = std::move(releaseFences);

    return NO_ERROR;
}

status_t HWComposer::setPowerMode(DisplayId displayId, int32_t intMode) {
    RETURN_IF_INVALID_DISPLAY(displayId, BAD_INDEX);

    const auto& displayData = mDisplayData[displayId];
    if (displayData.isVirtual) {
        LOG_DISPLAY_ERROR(displayId, "Invalid operation on virtual display");
        return INVALID_OPERATION;
    }

    auto mode = static_cast<HWC2::PowerMode>(intMode);
    if (mode == HWC2::PowerMode::Off) {
        setVsyncEnabled(displayId, HWC2::Vsync::Disable);
    }

    auto& hwcDisplay = displayData.hwcDisplay;
    switch (mode) {
        case HWC2::PowerMode::Off:
        case HWC2::PowerMode::On:
            ALOGV("setPowerMode: Calling HWC %s", to_string(mode).c_str());
            {
                auto error = hwcDisplay->setPowerMode(mode);
                if (error != HWC2::Error::None) {
                    LOG_HWC_ERROR(("setPowerMode(" + to_string(mode) + ")").c_str(),
                                  error, displayId);
                }
            }
            break;
        case HWC2::PowerMode::Doze:
        case HWC2::PowerMode::DozeSuspend:
            ALOGV("setPowerMode: Calling HWC %s", to_string(mode).c_str());
            {
                bool supportsDoze = false;
                auto error = hwcDisplay->supportsDoze(&supportsDoze);
                if (error != HWC2::Error::None) {
                    LOG_HWC_ERROR("supportsDoze", error, displayId);
                }

                if (!supportsDoze) {
                    mode = HWC2::PowerMode::On;
                }

                error = hwcDisplay->setPowerMode(mode);
                if (error != HWC2::Error::None) {
                    LOG_HWC_ERROR(("setPowerMode(" + to_string(mode) + ")").c_str(),
                                  error, displayId);
                }
            }
            break;
        default:
            ALOGV("setPowerMode: Not calling HWC");
            break;
    }

    return NO_ERROR;
}

status_t HWComposer::setActiveConfig(DisplayId displayId, size_t configId) {
    RETURN_IF_INVALID_DISPLAY(displayId, BAD_INDEX);

    auto& displayData = mDisplayData[displayId];
    if (displayData.configMap.count(configId) == 0) {
        LOG_DISPLAY_ERROR(displayId, ("Invalid config " + std::to_string(configId)).c_str());
        return BAD_INDEX;
    }

    auto error = displayData.hwcDisplay->setActiveConfig(displayData.configMap[configId]);
    RETURN_IF_HWC_ERROR(error, displayId, UNKNOWN_ERROR);
    return NO_ERROR;
}

status_t HWComposer::setColorTransform(DisplayId displayId, const mat4& transform) {
    RETURN_IF_INVALID_DISPLAY(displayId, BAD_INDEX);

    auto& displayData = mDisplayData[displayId];
    bool isIdentity = transform == mat4();
    auto error = displayData.hwcDisplay->setColorTransform(transform,
            isIdentity ? HAL_COLOR_TRANSFORM_IDENTITY :
            HAL_COLOR_TRANSFORM_ARBITRARY_MATRIX);
    RETURN_IF_HWC_ERROR(error, displayId, UNKNOWN_ERROR);
    return NO_ERROR;
}

void HWComposer::disconnectDisplay(DisplayId displayId) {
    RETURN_IF_INVALID_DISPLAY(displayId);
    auto& displayData = mDisplayData[displayId];

    // If this was a virtual display, add its slot back for reuse by future
    // virtual displays
    if (displayData.isVirtual) {
        mFreeVirtualDisplayIds.insert(displayId);
        ++mRemainingHwcVirtualDisplays;
    }

    const auto hwcDisplayId = displayData.hwcDisplay->getId();

    // TODO(b/74619554): Select internal/external display from remaining displays.
    if (hwcDisplayId == mInternalHwcDisplayId) {
        mInternalHwcDisplayId.reset();
    } else if (hwcDisplayId == mExternalHwcDisplayId) {
        mExternalHwcDisplayId.reset();
    }

    mPhysicalDisplayIdMap.erase(hwcDisplayId);
    mDisplayData.erase(displayId);
}

status_t HWComposer::setOutputBuffer(DisplayId displayId, const sp<Fence>& acquireFence,
                                     const sp<GraphicBuffer>& buffer) {
    RETURN_IF_INVALID_DISPLAY(displayId, BAD_INDEX);
    const auto& displayData = mDisplayData[displayId];

    if (!displayData.isVirtual) {
        LOG_DISPLAY_ERROR(displayId, "Invalid operation on physical display");
        return INVALID_OPERATION;
    }

    auto error = displayData.hwcDisplay->setOutputBuffer(buffer, acquireFence);
    RETURN_IF_HWC_ERROR(error, displayId, UNKNOWN_ERROR);
    return NO_ERROR;
}

void HWComposer::clearReleaseFences(DisplayId displayId) {
    RETURN_IF_INVALID_DISPLAY(displayId);
    mDisplayData[displayId].releaseFences.clear();
}

status_t HWComposer::getHdrCapabilities(DisplayId displayId, HdrCapabilities* outCapabilities) {
    RETURN_IF_INVALID_DISPLAY(displayId, BAD_INDEX);

    auto& hwcDisplay = mDisplayData[displayId].hwcDisplay;
    auto error = hwcDisplay->getHdrCapabilities(outCapabilities);
    RETURN_IF_HWC_ERROR(error, displayId, UNKNOWN_ERROR);
    return NO_ERROR;
}

int32_t HWComposer::getSupportedPerFrameMetadata(DisplayId displayId) const {
    RETURN_IF_INVALID_DISPLAY(displayId, 0);
    return mDisplayData.at(displayId).hwcDisplay->getSupportedPerFrameMetadata();
}

std::vector<ui::RenderIntent> HWComposer::getRenderIntents(DisplayId displayId,
                                                           ui::ColorMode colorMode) const {
    RETURN_IF_INVALID_DISPLAY(displayId, {});

    std::vector<ui::RenderIntent> renderIntents;
    auto error = mDisplayData.at(displayId).hwcDisplay->getRenderIntents(colorMode, &renderIntents);
    RETURN_IF_HWC_ERROR(error, displayId, {});
    return renderIntents;
}

mat4 HWComposer::getDataspaceSaturationMatrix(DisplayId displayId, ui::Dataspace dataspace) {
    RETURN_IF_INVALID_DISPLAY(displayId, {});

    mat4 matrix;
    auto error = mDisplayData[displayId].hwcDisplay->getDataspaceSaturationMatrix(dataspace,
            &matrix);
    RETURN_IF_HWC_ERROR(error, displayId, {});
    return matrix;
}

status_t HWComposer::getDisplayedContentSamplingAttributes(DisplayId displayId,
                                                           ui::PixelFormat* outFormat,
                                                           ui::Dataspace* outDataspace,
                                                           uint8_t* outComponentMask) {
    RETURN_IF_INVALID_DISPLAY(displayId, BAD_INDEX);
    const auto error =
            mDisplayData[displayId]
                    .hwcDisplay->getDisplayedContentSamplingAttributes(outFormat, outDataspace,
                                                                       outComponentMask);
    if (error == HWC2::Error::Unsupported) RETURN_IF_HWC_ERROR(error, displayId, INVALID_OPERATION);
    RETURN_IF_HWC_ERROR(error, displayId, UNKNOWN_ERROR);
    return NO_ERROR;
}

status_t HWComposer::setDisplayContentSamplingEnabled(DisplayId displayId, bool enabled,
                                                      uint8_t componentMask, uint64_t maxFrames) {
    RETURN_IF_INVALID_DISPLAY(displayId, BAD_INDEX);
    const auto error =
            mDisplayData[displayId].hwcDisplay->setDisplayContentSamplingEnabled(enabled,
                                                                                 componentMask,
                                                                                 maxFrames);

    if (error == HWC2::Error::Unsupported) RETURN_IF_HWC_ERROR(error, displayId, INVALID_OPERATION);
    if (error == HWC2::Error::BadParameter) RETURN_IF_HWC_ERROR(error, displayId, BAD_VALUE);
    RETURN_IF_HWC_ERROR(error, displayId, UNKNOWN_ERROR);
    return NO_ERROR;
}

status_t HWComposer::getDisplayedContentSample(DisplayId displayId, uint64_t maxFrames,
                                               uint64_t timestamp, DisplayedFrameStats* outStats) {
    RETURN_IF_INVALID_DISPLAY(displayId, BAD_INDEX);
    const auto error =
            mDisplayData[displayId].hwcDisplay->getDisplayedContentSample(maxFrames, timestamp,
                                                                          outStats);
    RETURN_IF_HWC_ERROR(error, displayId, UNKNOWN_ERROR);
    return NO_ERROR;
}

status_t HWComposer::setDisplayBrightness(DisplayId displayId, float brightness) {
    RETURN_IF_INVALID_DISPLAY(displayId, BAD_INDEX);
    const auto error = mDisplayData[displayId].hwcDisplay->setDisplayBrightness(brightness);
    if (error == HWC2::Error::Unsupported) {
        RETURN_IF_HWC_ERROR(error, displayId, INVALID_OPERATION);
    }
    if (error == HWC2::Error::BadParameter) {
        RETURN_IF_HWC_ERROR(error, displayId, BAD_VALUE);
    }
    RETURN_IF_HWC_ERROR(error, displayId, UNKNOWN_ERROR);
    return NO_ERROR;
}

bool HWComposer::isUsingVrComposer() const {
    return getComposer()->isUsingVrComposer();
}

void HWComposer::dump(std::string& result) const {
    result.append(mComposer->dumpDebugInfo());
}

std::optional<DisplayId> HWComposer::toPhysicalDisplayId(hwc2_display_t hwcDisplayId) const {
    if (const auto it = mPhysicalDisplayIdMap.find(hwcDisplayId);
        it != mPhysicalDisplayIdMap.end()) {
        return it->second;
    }
    return {};
}

std::optional<hwc2_display_t> HWComposer::fromPhysicalDisplayId(DisplayId displayId) const {
    if (const auto it = mDisplayData.find(displayId);
        it != mDisplayData.end() && !it->second.isVirtual) {
        return it->second.hwcDisplay->getId();
    }
    return {};
}

bool HWComposer::shouldIgnoreHotplugConnect(hwc2_display_t hwcDisplayId,
                                            bool hasDisplayIdentificationData) const {
    if (isUsingVrComposer() && mInternalHwcDisplayId) {
        ALOGE("Ignoring connection of external display %" PRIu64 " in VR mode", hwcDisplayId);
        return true;
    }

    if (mHasMultiDisplaySupport && !hasDisplayIdentificationData) {
        ALOGE("Ignoring connection of display %" PRIu64 " without identification data",
              hwcDisplayId);
        return true;
    }

    if (!mHasMultiDisplaySupport && mInternalHwcDisplayId && mExternalHwcDisplayId) {
        ALOGE("Ignoring connection of tertiary display %" PRIu64, hwcDisplayId);
        return true;
    }

    return false;
}

std::optional<DisplayIdentificationInfo> HWComposer::onHotplugConnect(hwc2_display_t hwcDisplayId) {
    std::optional<DisplayIdentificationInfo> info;
    if (const auto displayId = toPhysicalDisplayId(hwcDisplayId)) {
        info = DisplayIdentificationInfo{.id = *displayId,
                                         .name = std::string()};
    } else {
        uint8_t port;
        DisplayIdentificationData data;
        const bool hasDisplayIdentificationData =
                getDisplayIdentificationData(hwcDisplayId, &port, &data);
        if (mPhysicalDisplayIdMap.empty()) {
            mHasMultiDisplaySupport = hasDisplayIdentificationData;
            ALOGI("Switching to %s multi-display mode",
                  mHasMultiDisplaySupport ? "generalized" : "legacy");
        }

        if (shouldIgnoreHotplugConnect(hwcDisplayId, hasDisplayIdentificationData)) {
            return {};
        }

        info = [this, hwcDisplayId, &port, &data, hasDisplayIdentificationData] {
            const bool isPrimary = !mInternalHwcDisplayId;
            if (mHasMultiDisplaySupport) {
                if (const auto info = parseDisplayIdentificationData(port, data)) {
                    return *info;
                }
                ALOGE("Failed to parse identification data for display %" PRIu64, hwcDisplayId);
            } else {
                ALOGW_IF(hasDisplayIdentificationData,
                         "Ignoring identification data for display %" PRIu64, hwcDisplayId);
                port = isPrimary ? HWC_DISPLAY_PRIMARY : HWC_DISPLAY_EXTERNAL;
            }

            return DisplayIdentificationInfo{.id = getFallbackDisplayId(port),
                                             .name = isPrimary ? "Internal display"
                                                               : "External display"};
        }();
    }

    if (!isConnected(info->id)) {
        allocatePhysicalDisplay(hwcDisplayId, info->id);
    }
    return info;
}

std::optional<DisplayIdentificationInfo> HWComposer::onHotplugDisconnect(
        hwc2_display_t hwcDisplayId) {
    const auto displayId = toPhysicalDisplayId(hwcDisplayId);
    if (!displayId) {
        ALOGE("Ignoring disconnection of invalid HWC display %" PRIu64, hwcDisplayId);
        return {};
    }

    // The display will later be destroyed by a call to
    // destroyDisplay(). For now we just mark it disconnected.
    if (isConnected(*displayId)) {
        mDisplayData[*displayId].hwcDisplay->setConnected(false);
    } else {
        ALOGW("Attempted to disconnect unknown display %" PRIu64, hwcDisplayId);
    }
    // The cleanup of Disconnect is handled through HWComposer::disconnectDisplay
    // via SurfaceFlinger's onHotplugReceived callback handling
    return DisplayIdentificationInfo{.id = *displayId,
                                     .name = std::string()};
}

void HWComposer::loadCapabilities() {
    static_assert(sizeof(HWC2::Capability) == sizeof(int32_t), "Capability size has changed");
    auto capabilities = mComposer->getCapabilities();
    for (auto capability : capabilities) {
        mCapabilities.emplace(static_cast<HWC2::Capability>(capability));
    }
}

uint32_t HWComposer::getMaxVirtualDisplayCount() const {
    return mComposer->getMaxVirtualDisplayCount();
}

} // namespace impl
} // namespace android
