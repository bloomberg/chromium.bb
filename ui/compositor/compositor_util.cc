// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor_util.h"

#include "base/command_line.h"
#include "cc/base/switches.h"
#include "components/viz/common/display/renderer_settings.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/color_space_switches.h"

namespace ui {

viz::ResourceSettings CreateResourceSettings(
    const viz::BufferToTextureTargetMap& image_targets) {
  viz::ResourceSettings resource_settings;
  resource_settings.buffer_to_texture_target_map = image_targets;
  return resource_settings;
}

viz::RendererSettings CreateRendererSettings(
    const viz::BufferToTextureTargetMap& image_targets) {
  viz::RendererSettings renderer_settings;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  renderer_settings.partial_swap_enabled =
      !command_line->HasSwitch(switches::kUIDisablePartialSwap);
#if defined(OS_WIN)
  renderer_settings.finish_rendering_on_resize = true;
#elif defined(OS_MACOSX)
  renderer_settings.release_overlay_resources_after_gpu_query = true;
#endif
  renderer_settings.gl_composited_overlay_candidate_quad_border =
      command_line->HasSwitch(
          cc::switches::kGlCompositedOverlayCandidateQuadBorder);
  renderer_settings.show_overdraw_feedback =
      command_line->HasSwitch(cc::switches::kShowOverdrawFeedback);
  renderer_settings.enable_color_correct_rendering =
      base::FeatureList::IsEnabled(features::kColorCorrectRendering);
  renderer_settings.resource_settings = CreateResourceSettings(image_targets);
  renderer_settings.show_overdraw_feedback =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          cc::switches::kShowOverdrawFeedback);
  renderer_settings.disallow_non_exact_resource_reuse =
      command_line->HasSwitch(cc::switches::kDisallowNonExactResourceReuse);
  renderer_settings.allow_antialiasing =
      !command_line->HasSwitch(cc::switches::kDisableCompositedAntialiasing);

  return renderer_settings;
}

}  // namespace ui
