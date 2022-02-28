// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/mojom/render_frame_metadata_mojom_traits.h"

#include "build/build_config.h"
#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "services/viz/public/cpp/compositing/selection_mojom_traits.h"
#include "services/viz/public/cpp/compositing/vertical_scroll_direction_mojom_traits.h"
#include "ui/gfx/geometry/mojom/geometry_mojom_traits.h"
#include "ui/gfx/mojom/selection_bound_mojom_traits.h"

namespace mojo {

// static
bool StructTraits<cc::mojom::DelegatedInkBrowserMetadataDataView,
                  cc::DelegatedInkBrowserMetadata>::
    Read(cc::mojom::DelegatedInkBrowserMetadataDataView data,
         cc::DelegatedInkBrowserMetadata* out) {
  out->delegated_ink_is_hovering = data.delegated_ink_is_hovering();
  return true;
}

// static
bool StructTraits<
    cc::mojom::RenderFrameMetadataDataView,
    cc::RenderFrameMetadata>::Read(cc::mojom::RenderFrameMetadataDataView data,
                                   cc::RenderFrameMetadata* out) {
  out->root_background_color = data.root_background_color();
  out->is_scroll_offset_at_top = data.is_scroll_offset_at_top();
  out->is_mobile_optimized = data.is_mobile_optimized();
  out->device_scale_factor = data.device_scale_factor();
  out->page_scale_factor = data.page_scale_factor();
  out->external_page_scale_factor = data.external_page_scale_factor();
  out->top_controls_height = data.top_controls_height();
  out->top_controls_shown_ratio = data.top_controls_shown_ratio();
#if defined(OS_ANDROID)
  out->bottom_controls_height = data.bottom_controls_height();
  out->bottom_controls_shown_ratio = data.bottom_controls_shown_ratio();
  out->top_controls_min_height_offset = data.top_controls_min_height_offset();
  out->bottom_controls_min_height_offset =
      data.bottom_controls_min_height_offset();
  out->min_page_scale_factor = data.min_page_scale_factor();
  out->max_page_scale_factor = data.max_page_scale_factor();
  out->root_overflow_y_hidden = data.root_overflow_y_hidden();
  out->has_transparent_background = data.has_transparent_background();
#endif
  return data.ReadRootScrollOffset(&out->root_scroll_offset) &&
         data.ReadSelection(&out->selection) &&
         data.ReadDelegatedInkMetadata(&out->delegated_ink_metadata) &&
#if defined(OS_ANDROID)
         data.ReadScrollableViewportSize(&out->scrollable_viewport_size) &&
         data.ReadRootLayerSize(&out->root_layer_size) &&
#endif
         data.ReadViewportSizeInPixels(&out->viewport_size_in_pixels) &&
         data.ReadLocalSurfaceId(&out->local_surface_id) &&
         data.ReadNewVerticalScrollDirection(
             &out->new_vertical_scroll_direction) &&
         data.ReadVisualPropertiesUpdateDuration(
             &out->visual_properties_update_duration);
}

}  // namespace mojo
