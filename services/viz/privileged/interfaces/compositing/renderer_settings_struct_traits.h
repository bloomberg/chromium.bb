// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef SERVICES_VIZ_PRIVILEGED_INTERFACES_COMPOSITING_RENDERER_SETTINGS_STRUCT_TRAITS_H_
#define SERVICES_VIZ_PRIVILEGED_INTERFACES_COMPOSITING_RENDERER_SETTINGS_STRUCT_TRAITS_H_

#include "components/viz/common/display/renderer_settings.h"
#include "services/viz/privileged/interfaces/compositing/renderer_settings.mojom.h"
#include "services/viz/privileged/interfaces/compositing/renderer_settings_struct_traits.h"

namespace mojo {
template <>
struct StructTraits<viz::mojom::RendererSettingsDataView,
                    viz::RendererSettings> {
  static const viz::ResourceSettings& resource_settings(
      const viz::RendererSettings& input) {
    return input.resource_settings;
  }

  static bool allow_antialiasing(const viz::RendererSettings& input) {
    return input.allow_antialiasing;
  }

  static bool force_antialiasing(const viz::RendererSettings& input) {
    return input.force_antialiasing;
  }

  static bool force_blending_with_shaders(const viz::RendererSettings& input) {
    return input.force_blending_with_shaders;
  }

  static bool partial_swap_enabled(const viz::RendererSettings& input) {
    return input.partial_swap_enabled;
  }

  static bool finish_rendering_on_resize(const viz::RendererSettings& input) {
    return input.finish_rendering_on_resize;
  }

  static bool should_clear_root_render_pass(
      const viz::RendererSettings& input) {
    return input.should_clear_root_render_pass;
  }

  static bool release_overlay_resources_after_gpu_query(
      const viz::RendererSettings& input) {
    return input.release_overlay_resources_after_gpu_query;
  }

  static bool gl_composited_overlay_candidate_quad_border(
      const viz::RendererSettings& input) {
    return input.gl_composited_overlay_candidate_quad_border;
  }

  static bool show_overdraw_feedback(const viz::RendererSettings& input) {
    return input.show_overdraw_feedback;
  }

  static bool enable_color_correct_rendering(
      const viz::RendererSettings& input) {
    return input.enable_color_correct_rendering;
  }

  static int highp_threshold_min(const viz::RendererSettings& input) {
    return input.highp_threshold_min;
  }

  static bool disallow_non_exact_resource_reuse(
      const viz::RendererSettings& input) {
    return input.disallow_non_exact_resource_reuse;
  }

  static int slow_down_compositing_scale_factor(
      const viz::RendererSettings& input) {
    return input.slow_down_compositing_scale_factor;
  }

  static bool use_skia_renderer(const viz::RendererSettings& input) {
    return input.use_skia_renderer;
  }

  static bool Read(viz::mojom::RendererSettingsDataView data,
                   viz::RendererSettings* out);
};

}  // namespace mojo

#endif  // SERVICES_VIZ_PRIVILEGED_INTERFACES_COMPOSITING_RENDERER_SETTINGS_STRUCT_TRAITS_H_
