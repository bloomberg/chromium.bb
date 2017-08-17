// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/message_loop/message_loop.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/common/resources/resource_settings.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/viz/privileged/interfaces/compositing/renderer_settings_struct_traits.h"
#include "services/viz/public/cpp/compositing/resource_settings_struct_traits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace viz {

namespace {

using StructTraitsTest = testing::Test;

constexpr size_t kArbitrarySize = 32;
constexpr bool kArbitraryBool = true;

TEST_F(StructTraitsTest, RendererSettings) {
  ResourceSettings arbitrary_resource_settings;
  arbitrary_resource_settings.texture_id_allocation_chunk_size = kArbitrarySize;
  arbitrary_resource_settings.use_gpu_memory_buffer_resources = kArbitraryBool;
  arbitrary_resource_settings.buffer_to_texture_target_map =
      DefaultBufferToTextureTargetMapForTesting();
  RendererSettings input;

  // Set |input| to non-default values.
  input.resource_settings = arbitrary_resource_settings;
  input.allow_antialiasing = false;
  input.force_antialiasing = true;
  input.force_blending_with_shaders = true;
  input.partial_swap_enabled = true;
  input.finish_rendering_on_resize = true;
  input.should_clear_root_render_pass = false;
  input.release_overlay_resources_after_gpu_query = true;
  input.gl_composited_overlay_candidate_quad_border = true;
  input.show_overdraw_feedback = true;
  input.enable_color_correct_rendering = true;
  input.highp_threshold_min = -1;
  input.disallow_non_exact_resource_reuse = true;

  RendererSettings output;
  mojom::RendererSettings::Deserialize(
      mojom::RendererSettings::Serialize(&input), &output);
  EXPECT_EQ(input.resource_settings.texture_id_allocation_chunk_size,
            output.resource_settings.texture_id_allocation_chunk_size);
  EXPECT_EQ(input.resource_settings.use_gpu_memory_buffer_resources,
            output.resource_settings.use_gpu_memory_buffer_resources);
  EXPECT_EQ(input.resource_settings.buffer_to_texture_target_map,
            output.resource_settings.buffer_to_texture_target_map);
  EXPECT_EQ(input.allow_antialiasing, output.allow_antialiasing);
  EXPECT_EQ(input.force_antialiasing, output.force_antialiasing);
  EXPECT_EQ(input.force_blending_with_shaders,
            output.force_blending_with_shaders);
  EXPECT_EQ(input.partial_swap_enabled, output.partial_swap_enabled);
  EXPECT_EQ(input.finish_rendering_on_resize,
            output.finish_rendering_on_resize);
  EXPECT_EQ(input.should_clear_root_render_pass,
            output.should_clear_root_render_pass);
  EXPECT_EQ(input.release_overlay_resources_after_gpu_query,
            output.release_overlay_resources_after_gpu_query);
  EXPECT_EQ(input.gl_composited_overlay_candidate_quad_border,
            output.gl_composited_overlay_candidate_quad_border);
  EXPECT_EQ(input.show_overdraw_feedback, output.show_overdraw_feedback);
  EXPECT_EQ(input.enable_color_correct_rendering,
            output.enable_color_correct_rendering);
  EXPECT_EQ(input.highp_threshold_min, output.highp_threshold_min);
  EXPECT_EQ(input.disallow_non_exact_resource_reuse,
            output.disallow_non_exact_resource_reuse);
}

}  // namespace

}  // namespace viz
