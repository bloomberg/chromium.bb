// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/message_loop/message_loop.h"
#include "cc/ipc/begin_frame_args_struct_traits.h"
#include "cc/ipc/copy_output_request_struct_traits.h"
#include "cc/ipc/copy_output_result_struct_traits.h"
#include "cc/ipc/filter_operation_struct_traits.h"
#include "cc/ipc/filter_operations_struct_traits.h"
#include "cc/ipc/frame_sink_id_struct_traits.h"
#include "cc/ipc/local_surface_id_struct_traits.h"
#include "cc/ipc/quads_struct_traits.h"
#include "cc/ipc/render_pass_struct_traits.h"
#include "cc/ipc/selection_struct_traits.h"
#include "cc/ipc/shared_quad_state_struct_traits.h"
#include "cc/ipc/surface_id_struct_traits.h"
#include "cc/ipc/texture_mailbox_struct_traits.h"
#include "cc/ipc/transferable_resource_struct_traits.h"
#include "cc/output/compositor_frame.h"
#include "cc/quads/debug_border_draw_quad.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "components/viz/common/frame_sinks/begin_frame_args.h"
#include "components/viz/common/quads/resource_format.h"
#include "components/viz/common/resources/resource_settings.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/resources/transferable_resource.h"
#include "components/viz/common/surfaces/surface_info.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "gpu/ipc/common/mailbox_holder_struct_traits.h"
#include "gpu/ipc/common/mailbox_struct_traits.h"
#include "gpu/ipc/common/sync_token_struct_traits.h"
#include "ipc/ipc_message_utils.h"
#include "mojo/common/common_custom_types_struct_traits.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/viz/public/cpp/compositing/compositor_frame_metadata_struct_traits.h"
#include "services/viz/public/cpp/compositing/compositor_frame_struct_traits.h"
#include "services/viz/public/cpp/compositing/resource_settings_struct_traits.h"
#include "services/viz/public/cpp/compositing/returned_resource_struct_traits.h"
#include "services/viz/public/cpp/compositing/surface_info_struct_traits.h"
#include "services/viz/public/cpp/compositing/surface_sequence_struct_traits.h"
#include "services/viz/public/interfaces/compositing/compositor_frame.mojom.h"
#include "services/viz/public/interfaces/compositing/returned_resource.mojom.h"
#include "services/viz/public/interfaces/compositing/surface_info.mojom.h"
#include "services/viz/public/interfaces/compositing/surface_sequence.mojom.h"
#include "skia/public/interfaces/bitmap_skbitmap_struct_traits.h"
#include "skia/public/interfaces/blur_image_filter_tile_mode_struct_traits.h"
#include "skia/public/interfaces/image_filter_struct_traits.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/mojo/geometry_struct_traits.h"
#include "ui/gfx/ipc/color/gfx_param_traits.h"
#include "ui/gfx/mojo/buffer_types_struct_traits.h"
#include "ui/gfx/mojo/selection_bound_struct_traits.h"
#include "ui/gfx/mojo/transform_struct_traits.h"
#include "ui/latency/mojo/latency_info_struct_traits.h"

namespace viz {

namespace {

using StructTraitsTest = testing::Test;

// Test StructTrait serialization and deserialization for copyable type. |input|
// will be serialized and then deserialized into |output|.
template <class MojomType, class Type>
void SerializeAndDeserialize(const Type& input, Type* output) {
  MojomType::Deserialize(MojomType::Serialize(&input), output);
}

// Test StructTrait serialization and deserialization for move only type.
// |input| will be serialized and then deserialized into |output|.
template <class MojomType, class Type>
void SerializeAndDeserialize(Type&& input, Type* output) {
  MojomType::Deserialize(MojomType::Serialize(&input), output);
}

}  // namespace

TEST_F(StructTraitsTest, ResourceSettings) {
  constexpr size_t kArbitrarySize = 32;
  constexpr bool kArbitraryBool = true;
  ResourceSettings input;
  input.texture_id_allocation_chunk_size = kArbitrarySize;
  input.use_gpu_memory_buffer_resources = kArbitraryBool;
  input.buffer_to_texture_target_map =
      DefaultBufferToTextureTargetMapForTesting();

  ResourceSettings output;
  SerializeAndDeserialize<mojom::ResourceSettings>(input, &output);

  EXPECT_EQ(input.texture_id_allocation_chunk_size,
            output.texture_id_allocation_chunk_size);
  EXPECT_EQ(input.use_gpu_memory_buffer_resources,
            output.use_gpu_memory_buffer_resources);
  EXPECT_EQ(input.buffer_to_texture_target_map,
            output.buffer_to_texture_target_map);
}

TEST_F(StructTraitsTest, SurfaceSequence) {
  const FrameSinkId frame_sink_id(2016, 1234);
  const uint32_t sequence = 0xfbadbeef;

  SurfaceSequence input(frame_sink_id, sequence);
  SurfaceSequence output;
  mojom::SurfaceSequence::Deserialize(mojom::SurfaceSequence::Serialize(&input),
                                      &output);

  EXPECT_EQ(frame_sink_id, output.frame_sink_id);
  EXPECT_EQ(sequence, output.sequence);
}

// Note that this is a fairly trivial test of CompositorFrame serialization as
// most of the heavy lifting has already been done by CompositorFrameMetadata,
// RenderPass, and QuadListBasic unit tests.
TEST_F(StructTraitsTest, CompositorFrame) {
  std::unique_ptr<cc::RenderPass> render_pass = cc::RenderPass::Create();
  render_pass->SetNew(1, gfx::Rect(5, 6), gfx::Rect(2, 3), gfx::Transform());

  // SharedQuadState.
  const gfx::Transform sqs_quad_to_target_transform(
      1.f, 2.f, 3.f, 4.f, 5.f, 6.f, 7.f, 8.f, 9.f, 10.f, 11.f, 12.f, 13.f, 14.f,
      15.f, 16.f);
  const gfx::Rect sqs_layer_rect(1234, 5678);
  const gfx::Rect sqs_visible_layer_rect(12, 34, 56, 78);
  const gfx::Rect sqs_clip_rect(123, 456, 789, 101112);
  const bool sqs_is_clipped = true;
  const float sqs_opacity = 0.9f;
  const SkBlendMode sqs_blend_mode = SkBlendMode::kSrcOver;
  const int sqs_sorting_context_id = 1337;
  cc::SharedQuadState* sqs = render_pass->CreateAndAppendSharedQuadState();
  sqs->SetAll(sqs_quad_to_target_transform, sqs_layer_rect,
              sqs_visible_layer_rect, sqs_clip_rect, sqs_is_clipped,
              sqs_opacity, sqs_blend_mode, sqs_sorting_context_id);

  // DebugBorderDrawQuad.
  const gfx::Rect rect1(1234, 4321, 1357, 7531);
  const SkColor color1 = SK_ColorRED;
  const int32_t width1 = 1337;
  cc::DebugBorderDrawQuad* debug_quad =
      render_pass->CreateAndAppendDrawQuad<cc::DebugBorderDrawQuad>();
  debug_quad->SetNew(sqs, rect1, rect1, color1, width1);

  // SolidColorDrawQuad.
  const gfx::Rect rect2(2468, 8642, 4321, 1234);
  const uint32_t color2 = 0xffffffff;
  const bool force_anti_aliasing_off = true;
  cc::SolidColorDrawQuad* solid_quad =
      render_pass->CreateAndAppendDrawQuad<cc::SolidColorDrawQuad>();
  solid_quad->SetNew(sqs, rect2, rect2, color2, force_anti_aliasing_off);

  // TransferableResource constants.
  const uint32_t tr_id = 1337;
  const ResourceFormat tr_format = ALPHA_8;
  const gfx::BufferFormat tr_buffer_format = gfx::BufferFormat::R_8;
  const uint32_t tr_filter = 1234;
  const gfx::Size tr_size(1234, 5678);
  TransferableResource resource;
  resource.id = tr_id;
  resource.format = tr_format;
  resource.buffer_format = tr_buffer_format;
  resource.filter = tr_filter;
  resource.size = tr_size;

  // CompositorFrameMetadata constants.
  const float device_scale_factor = 2.6f;
  const gfx::Vector2dF root_scroll_offset(1234.5f, 6789.1f);
  const float page_scale_factor = 1337.5f;
  const gfx::SizeF scrollable_viewport_size(1337.7f, 1234.5f);
  const uint32_t content_source_id = 3;
  const BeginFrameAck begin_frame_ack(5, 10, false);

  cc::CompositorFrame input;
  input.metadata.device_scale_factor = device_scale_factor;
  input.metadata.root_scroll_offset = root_scroll_offset;
  input.metadata.page_scale_factor = page_scale_factor;
  input.metadata.scrollable_viewport_size = scrollable_viewport_size;
  input.render_pass_list.push_back(std::move(render_pass));
  input.resource_list.push_back(resource);
  input.metadata.content_source_id = content_source_id;
  input.metadata.begin_frame_ack = begin_frame_ack;

  cc::CompositorFrame output;
  SerializeAndDeserialize<mojom::CompositorFrame>(input, &output);

  EXPECT_EQ(device_scale_factor, output.metadata.device_scale_factor);
  EXPECT_EQ(root_scroll_offset, output.metadata.root_scroll_offset);
  EXPECT_EQ(page_scale_factor, output.metadata.page_scale_factor);
  EXPECT_EQ(scrollable_viewport_size, output.metadata.scrollable_viewport_size);
  EXPECT_EQ(content_source_id, output.metadata.content_source_id);
  EXPECT_EQ(begin_frame_ack, output.metadata.begin_frame_ack);

  ASSERT_EQ(1u, output.resource_list.size());
  TransferableResource out_resource = output.resource_list[0];
  EXPECT_EQ(tr_id, out_resource.id);
  EXPECT_EQ(tr_format, out_resource.format);
  EXPECT_EQ(tr_buffer_format, out_resource.buffer_format);
  EXPECT_EQ(tr_filter, out_resource.filter);
  EXPECT_EQ(tr_size, out_resource.size);

  EXPECT_EQ(1u, output.render_pass_list.size());
  const cc::RenderPass* out_render_pass = output.render_pass_list[0].get();
  ASSERT_EQ(2u, out_render_pass->quad_list.size());
  ASSERT_EQ(1u, out_render_pass->shared_quad_state_list.size());

  const cc::SharedQuadState* out_sqs =
      out_render_pass->shared_quad_state_list.ElementAt(0);
  EXPECT_EQ(sqs_quad_to_target_transform, out_sqs->quad_to_target_transform);
  EXPECT_EQ(sqs_layer_rect, out_sqs->quad_layer_rect);
  EXPECT_EQ(sqs_visible_layer_rect, out_sqs->visible_quad_layer_rect);
  EXPECT_EQ(sqs_clip_rect, out_sqs->clip_rect);
  EXPECT_EQ(sqs_is_clipped, out_sqs->is_clipped);
  EXPECT_EQ(sqs_opacity, out_sqs->opacity);
  EXPECT_EQ(sqs_blend_mode, out_sqs->blend_mode);
  EXPECT_EQ(sqs_sorting_context_id, out_sqs->sorting_context_id);

  const cc::DebugBorderDrawQuad* out_debug_border_draw_quad =
      cc::DebugBorderDrawQuad::MaterialCast(
          out_render_pass->quad_list.ElementAt(0));
  EXPECT_EQ(rect1, out_debug_border_draw_quad->rect);
  EXPECT_EQ(rect1, out_debug_border_draw_quad->visible_rect);
  EXPECT_EQ(color1, out_debug_border_draw_quad->color);
  EXPECT_EQ(width1, out_debug_border_draw_quad->width);

  const cc::SolidColorDrawQuad* out_solid_color_draw_quad =
      cc::SolidColorDrawQuad::MaterialCast(
          out_render_pass->quad_list.ElementAt(1));
  EXPECT_EQ(rect2, out_solid_color_draw_quad->rect);
  EXPECT_EQ(rect2, out_solid_color_draw_quad->visible_rect);
  EXPECT_EQ(color2, out_solid_color_draw_quad->color);
  EXPECT_EQ(force_anti_aliasing_off,
            out_solid_color_draw_quad->force_anti_aliasing_off);
}

TEST_F(StructTraitsTest, SurfaceInfo) {
  const SurfaceId surface_id(
      FrameSinkId(1234, 4321),
      LocalSurfaceId(5678,
                     base::UnguessableToken::Deserialize(143254, 144132)));
  constexpr float device_scale_factor = 1.234f;
  constexpr gfx::Size size(987, 123);

  const SurfaceInfo input(surface_id, device_scale_factor, size);
  SurfaceInfo output;
  SerializeAndDeserialize<mojom::SurfaceInfo>(input, &output);

  EXPECT_EQ(input.id(), output.id());
  EXPECT_EQ(input.size_in_pixels(), output.size_in_pixels());
  EXPECT_EQ(input.device_scale_factor(), output.device_scale_factor());
}

TEST_F(StructTraitsTest, ReturnedResource) {
  const cc::RenderPassId id = 1337u;
  const gpu::CommandBufferNamespace command_buffer_namespace = gpu::IN_PROCESS;
  const int32_t extra_data_field = 0xbeefbeef;
  const gpu::CommandBufferId command_buffer_id(
      gpu::CommandBufferId::FromUnsafeValue(0xdeadbeef));
  const uint64_t release_count = 0xdeadbeefdead;
  const gpu::SyncToken sync_token(command_buffer_namespace, extra_data_field,
                                  command_buffer_id, release_count);
  const int count = 1234;
  const bool lost = true;

  ReturnedResource input;
  input.id = id;
  input.sync_token = sync_token;
  input.count = count;
  input.lost = lost;

  ReturnedResource output;
  SerializeAndDeserialize<mojom::ReturnedResource>(input, &output);

  EXPECT_EQ(id, output.id);
  EXPECT_EQ(sync_token, output.sync_token);
  EXPECT_EQ(count, output.count);
  EXPECT_EQ(lost, output.lost);
}

TEST_F(StructTraitsTest, CompositorFrameMetadata) {
  const float device_scale_factor = 2.6f;
  const gfx::Vector2dF root_scroll_offset(1234.5f, 6789.1f);
  const float page_scale_factor = 1337.5f;
  const gfx::SizeF scrollable_viewport_size(1337.7f, 1234.5f);
  const gfx::SizeF root_layer_size(1234.5f, 5432.1f);
  const float min_page_scale_factor = 3.5f;
  const float max_page_scale_factor = 4.6f;
  const bool root_overflow_x_hidden = true;
  const bool root_overflow_y_hidden = true;
  const bool may_contain_video = true;
  const bool is_resourceless_software_draw_with_scroll_or_animation = true;
  const float top_bar_height(1234.5f);
  const float top_bar_shown_ratio(1.0f);
  const float bottom_bar_height(1234.5f);
  const float bottom_bar_shown_ratio(1.0f);
  const uint32_t root_background_color = 1337;
  cc::Selection<gfx::SelectionBound> selection;
  selection.start.SetEdge(gfx::PointF(1234.5f, 67891.f),
                          gfx::PointF(5432.1f, 1987.6f));
  selection.start.set_visible(true);
  selection.start.set_type(gfx::SelectionBound::CENTER);
  selection.end.SetEdge(gfx::PointF(1337.5f, 52124.f),
                        gfx::PointF(1234.3f, 8765.6f));
  selection.end.set_visible(false);
  selection.end.set_type(gfx::SelectionBound::RIGHT);
  ui::LatencyInfo latency_info;
  latency_info.set_trace_id(5);
  latency_info.AddLatencyNumber(
      ui::LATENCY_BEGIN_SCROLL_LISTENER_UPDATE_MAIN_COMPONENT, 1337, 7331);
  std::vector<ui::LatencyInfo> latency_infos = {latency_info};
  std::vector<SurfaceId> referenced_surfaces;
  SurfaceId id(FrameSinkId(1234, 4321),
               LocalSurfaceId(5678, base::UnguessableToken::Create()));
  referenced_surfaces.push_back(id);
  std::vector<SurfaceId> activation_dependencies;
  SurfaceId id2(FrameSinkId(4321, 1234),
                LocalSurfaceId(8765, base::UnguessableToken::Create()));
  activation_dependencies.push_back(id2);
  uint32_t frame_token = 0xdeadbeef;
  uint64_t begin_frame_ack_sequence_number = 0xdeadbeef;

  cc::CompositorFrameMetadata input;
  input.device_scale_factor = device_scale_factor;
  input.root_scroll_offset = root_scroll_offset;
  input.page_scale_factor = page_scale_factor;
  input.scrollable_viewport_size = scrollable_viewport_size;
  input.root_layer_size = root_layer_size;
  input.min_page_scale_factor = min_page_scale_factor;
  input.max_page_scale_factor = max_page_scale_factor;
  input.root_overflow_x_hidden = root_overflow_x_hidden;
  input.root_overflow_y_hidden = root_overflow_y_hidden;
  input.may_contain_video = may_contain_video;
  input.is_resourceless_software_draw_with_scroll_or_animation =
      is_resourceless_software_draw_with_scroll_or_animation;
  input.top_controls_height = top_bar_height;
  input.top_controls_shown_ratio = top_bar_shown_ratio;
  input.bottom_controls_height = bottom_bar_height;
  input.bottom_controls_shown_ratio = bottom_bar_shown_ratio;
  input.root_background_color = root_background_color;
  input.selection = selection;
  input.latency_info = latency_infos;
  input.referenced_surfaces = referenced_surfaces;
  input.activation_dependencies = activation_dependencies;
  input.frame_token = frame_token;
  input.begin_frame_ack.sequence_number = begin_frame_ack_sequence_number;

  cc::CompositorFrameMetadata output;
  SerializeAndDeserialize<mojom::CompositorFrameMetadata>(input, &output);
  EXPECT_EQ(device_scale_factor, output.device_scale_factor);
  EXPECT_EQ(root_scroll_offset, output.root_scroll_offset);
  EXPECT_EQ(page_scale_factor, output.page_scale_factor);
  EXPECT_EQ(scrollable_viewport_size, output.scrollable_viewport_size);
  EXPECT_EQ(root_layer_size, output.root_layer_size);
  EXPECT_EQ(min_page_scale_factor, output.min_page_scale_factor);
  EXPECT_EQ(max_page_scale_factor, output.max_page_scale_factor);
  EXPECT_EQ(root_overflow_x_hidden, output.root_overflow_x_hidden);
  EXPECT_EQ(root_overflow_y_hidden, output.root_overflow_y_hidden);
  EXPECT_EQ(may_contain_video, output.may_contain_video);
  EXPECT_EQ(is_resourceless_software_draw_with_scroll_or_animation,
            output.is_resourceless_software_draw_with_scroll_or_animation);
  EXPECT_EQ(top_bar_height, output.top_controls_height);
  EXPECT_EQ(top_bar_shown_ratio, output.top_controls_shown_ratio);
  EXPECT_EQ(bottom_bar_height, output.bottom_controls_height);
  EXPECT_EQ(bottom_bar_shown_ratio, output.bottom_controls_shown_ratio);
  EXPECT_EQ(root_background_color, output.root_background_color);
  EXPECT_EQ(selection, output.selection);
  EXPECT_EQ(latency_infos.size(), output.latency_info.size());
  ui::LatencyInfo::LatencyComponent component;
  EXPECT_TRUE(output.latency_info[0].FindLatency(
      ui::LATENCY_BEGIN_SCROLL_LISTENER_UPDATE_MAIN_COMPONENT, 1337,
      &component));
  EXPECT_EQ(7331, component.sequence_number);
  EXPECT_EQ(referenced_surfaces.size(), output.referenced_surfaces.size());
  for (uint32_t i = 0; i < referenced_surfaces.size(); ++i)
    EXPECT_EQ(referenced_surfaces[i], output.referenced_surfaces[i]);
  EXPECT_EQ(activation_dependencies.size(),
            output.activation_dependencies.size());
  for (uint32_t i = 0; i < activation_dependencies.size(); ++i)
    EXPECT_EQ(activation_dependencies[i], output.activation_dependencies[i]);
  EXPECT_EQ(frame_token, output.frame_token);
  EXPECT_EQ(begin_frame_ack_sequence_number,
            output.begin_frame_ack.sequence_number);
}

}  // namespace viz
