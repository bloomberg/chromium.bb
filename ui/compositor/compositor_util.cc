// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor_util.h"

#include "base/command_line.h"
#include "cc/base/switches.h"
#include "cc/output/renderer_settings.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/display/display_switches.h"
#include "ui/gfx/color_space_switches.h"

namespace ui {

cc::RendererSettings CreateRendererSettings(uint32_t (
    *get_texture_target)(gfx::BufferFormat format, gfx::BufferUsage usage)) {
  cc::RendererSettings renderer_settings;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  renderer_settings.partial_swap_enabled =
      !command_line->HasSwitch(switches::kUIDisablePartialSwap);
#if defined(OS_WIN)
  renderer_settings.finish_rendering_on_resize = true;
#elif defined(OS_MACOSX)
  renderer_settings.release_overlay_resources_after_gpu_query = true;
#endif
  renderer_settings.gl_composited_texture_quad_border =
      command_line->HasSwitch(cc::switches::kGlCompositedTextureQuadBorder);
  renderer_settings.show_overdraw_feedback =
      command_line->HasSwitch(cc::switches::kShowOverdrawFeedback);
  renderer_settings.enable_color_correct_rendering =
      base::FeatureList::IsEnabled(features::kColorCorrectRendering) ||
      command_line->HasSwitch(switches::kEnableHDR);
  // Populate buffer_to_texture_target_map for all buffer usage/formats.
  for (int usage_idx = 0; usage_idx <= static_cast<int>(gfx::BufferUsage::LAST);
       ++usage_idx) {
    gfx::BufferUsage usage = static_cast<gfx::BufferUsage>(usage_idx);
    for (int format_idx = 0;
         format_idx <= static_cast<int>(gfx::BufferFormat::LAST);
         ++format_idx) {
      gfx::BufferFormat format = static_cast<gfx::BufferFormat>(format_idx);
      renderer_settings.resource_settings
          .buffer_to_texture_target_map[std::make_pair(usage, format)] =
          get_texture_target(format, usage);
    }
  }
  renderer_settings.disallow_non_exact_resource_reuse =
      command_line->HasSwitch(cc::switches::kDisallowNonExactResourceReuse);
  renderer_settings.allow_antialiasing =
      !command_line->HasSwitch(cc::switches::kDisableCompositedAntialiasing);

  return renderer_settings;
}

}  // namespace ui
