/* * Copyright (C) 2016 The Android Open Source Project
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

#include <sstream>

#include "hwc2_test_layers.h"

hwc2_test_layers::hwc2_test_layers(const std::vector<hwc2_layer_t> &layers,
        hwc2_test_coverage_t coverage, int32_t display_width,
        int32_t display_height)
    : test_layers(),
      display_width(display_width),
      display_height(display_height)
{
    for (auto layer: layers)
        test_layers.emplace(std::piecewise_construct,
                std::forward_as_tuple(layer), std::forward_as_tuple(coverage,
                display_width, display_height));

    /* Iterate over the layers in order and assign z orders in the same order.
     * This allows us to iterate over z orders in the same way when computing
     * visible regions */
    uint32_t next_z_order = layers.size();

    for (auto &test_layer: test_layers)
        test_layer.second.set_z_order(next_z_order--);

    set_visible_regions();
}

std::string hwc2_test_layers::dump() const
{
    std::stringstream dmp;
    for (auto &test_layer: test_layers)
        dmp << test_layer.second.dump();
    return dmp.str();
}

void hwc2_test_layers::reset()
{
    for (auto &test_layer: test_layers)
        test_layer.second.reset();
    set_visible_regions();
}

bool hwc2_test_layers::advance()
{
    for (auto &test_layer: test_layers) {
        if (test_layer.second.advance()) {
            set_visible_regions();
            return true;
        }
        test_layer.second.reset();
    }
    return false;
}

bool hwc2_test_layers::advance_visible_regions()
{
    for (auto &test_layer: test_layers) {
        if (test_layer.second.advance_visible_region()) {
            set_visible_regions();
            return true;
        }
        test_layer.second.reset();
    }
    return false;
}

bool hwc2_test_layers::contains(hwc2_layer_t layer) const
{
    return test_layers.find(layer) != test_layers.end();
}

int hwc2_test_layers::get_buffer(hwc2_layer_t layer,
        buffer_handle_t *out_handle, int32_t *out_acquire_fence)
{
    return test_layers.find(layer)->second.get_buffer(out_handle,
            out_acquire_fence);
}

hwc2_blend_mode_t hwc2_test_layers::get_blend_mode(hwc2_layer_t layer) const
{
    return test_layers.find(layer)->second.get_blend_mode();
}

const hwc_color_t hwc2_test_layers::get_color(hwc2_layer_t layer) const
{
    return test_layers.find(layer)->second.get_color();
}

hwc2_composition_t hwc2_test_layers::get_composition(hwc2_layer_t layer) const
{
    return test_layers.find(layer)->second.get_composition();
}

const std::pair<int32_t, int32_t> hwc2_test_layers::get_cursor(
        hwc2_layer_t layer) const
{
    return test_layers.find(layer)->second.get_cursor();
}

android_dataspace_t hwc2_test_layers::get_dataspace(hwc2_layer_t layer) const
{
    return test_layers.find(layer)->second.get_dataspace();
}

const hwc_rect_t hwc2_test_layers::get_display_frame(hwc2_layer_t layer) const
{
    return test_layers.find(layer)->second.get_display_frame();
}

float hwc2_test_layers::get_plane_alpha(hwc2_layer_t layer) const
{
    return test_layers.find(layer)->second.get_plane_alpha();
}

const hwc_frect_t hwc2_test_layers::get_source_crop(hwc2_layer_t layer) const
{
    return test_layers.find(layer)->second.get_source_crop();
}

const hwc_region_t hwc2_test_layers::get_surface_damage(hwc2_layer_t layer) const
{
    return test_layers.find(layer)->second.get_surface_damage();
}

hwc_transform_t hwc2_test_layers::get_transform(hwc2_layer_t layer) const
{
    return test_layers.find(layer)->second.get_transform();
}

const hwc_region_t hwc2_test_layers::get_visible_region(hwc2_layer_t layer) const
{
    return test_layers.find(layer)->second.get_visible_region();
}

uint32_t hwc2_test_layers::get_z_order(hwc2_layer_t layer) const
{
    return test_layers.find(layer)->second.get_z_order();
}

void hwc2_test_layers::set_visible_regions()
{
    /* The region of the display that is covered by layers above the current
     * layer */
    android::Region above_opaque_layers;

    /* Iterate over test layers from max z order to min z order. */
    for (auto &test_layer: test_layers) {
        android::Region visible_region;

        /* Set the visible region of this layer */
        if (test_layer.second.get_composition() != HWC2_COMPOSITION_CURSOR) {
            const auto display_frame = test_layer.second.get_display_frame();

            visible_region.set(android::Rect(display_frame.left,
                    display_frame.top, display_frame.right,
                    display_frame.bottom));
        } else {
            const auto buffer_area = test_layer.second.get_buffer_area();

            visible_region.set(android::Rect(0, 0, buffer_area.first,
                    buffer_area.second));
        }

        /* Remove the area covered by opaque layers above this layer
         * from this layer's visible region */
        visible_region.subtractSelf(above_opaque_layers);

        test_layer.second.set_visible_region(visible_region);

        /* If this layer is opaque, store the region it covers */
        if (test_layer.second.get_plane_alpha() == 1.0f)
            above_opaque_layers.orSelf(visible_region);
    }
}
