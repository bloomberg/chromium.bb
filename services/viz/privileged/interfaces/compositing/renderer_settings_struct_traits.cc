// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/privileged/interfaces/compositing/renderer_settings_struct_traits.h"
#include "services/viz/public/cpp/compositing/resource_settings_struct_traits.h"

namespace mojo {

// static
bool StructTraits<viz::mojom::RendererSettingsDataView, viz::RendererSettings>::
    Read(viz::mojom::RendererSettingsDataView data,
         viz::RendererSettings* out) {
  out->allow_antialiasing = data.allow_antialiasing();
  out->force_antialiasing = data.force_antialiasing();
  out->force_blending_with_shaders = data.force_blending_with_shaders();
  out->partial_swap_enabled = data.partial_swap_enabled();
  out->finish_rendering_on_resize = data.finish_rendering_on_resize();
  out->should_clear_root_render_pass = data.should_clear_root_render_pass();
  out->release_overlay_resources_after_gpu_query =
      data.release_overlay_resources_after_gpu_query();
  out->gl_composited_overlay_candidate_quad_border =
      data.gl_composited_overlay_candidate_quad_border();
  out->show_overdraw_feedback = data.show_overdraw_feedback();
  out->enable_color_correct_rendering = data.enable_color_correct_rendering();
  out->highp_threshold_min = data.highp_threshold_min();
  out->disallow_non_exact_resource_reuse =
      data.disallow_non_exact_resource_reuse();
  return data.ReadResourceSettings(&out->resource_settings);
}

}  // namespace mojo
