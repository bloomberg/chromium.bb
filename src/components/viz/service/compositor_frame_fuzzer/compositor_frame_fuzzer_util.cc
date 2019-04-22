// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/compositor_frame_fuzzer/compositor_frame_fuzzer_util.h"

#include <limits>
#include <memory>
#include <utility>

#include "components/viz/common/quads/solid_color_draw_quad.h"
#include "components/viz/common/quads/tile_draw_quad.h"
#include "components/viz/common/resources/resource_sizes.h"
#include "components/viz/service/compositor_frame_fuzzer/fuzzer_browser_process.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace viz {

namespace {

// Hard limit on number of bytes of memory to allocate (e.g. for referenced
// bitmaps) in association with a single CompositorFrame. Currently 0.5 GiB;
// reduce this if bots are running out of memory.
constexpr uint64_t kMaxMappedMemory = 1 << 29;

// Handles inf / NaN by setting to 0.
double MakeNormal(double x) {
  return isnormal(x) ? x : 0;
}
float MakeNormal(float x) {
  return isnormal(x) ? x : 0;
}

// Normalizes value to a float in [0, 1]. Use to convert a fuzzed
// uint32 into a percentage.
float Normalize(uint32_t x) {
  return static_cast<float>(x) / std::numeric_limits<uint32_t>::max();
}

gfx::Size GetSizeFromProtobuf(const content::fuzzing::proto::Size& proto_size) {
  return gfx::Size(proto_size.width(), proto_size.height());
}

gfx::Rect GetRectFromProtobuf(const content::fuzzing::proto::Rect& proto_rect) {
  return gfx::Rect(proto_rect.x(), proto_rect.y(), proto_rect.width(),
                   proto_rect.height());
}

gfx::Transform GetTransformFromProtobuf(
    const content::fuzzing::proto::Transform& proto_transform) {
  gfx::Transform transform = gfx::Transform();

  // Note: There are no checks here that disallow a non-invertible transform
  // (for instance, if |scale_x| or |scale_y| is 0).
  transform.Scale(MakeNormal(proto_transform.scale_x()),
                  MakeNormal(proto_transform.scale_y()));

  transform.Rotate(MakeNormal(proto_transform.rotate()));

  transform.Translate(MakeNormal(proto_transform.translate_x()),
                      MakeNormal(proto_transform.translate_y()));

  return transform;
}

// Mutates a gfx::Rect to ensure width and height are both at least min_size.
// Use in case e.g. a 0-width/height Rect would cause a validation error on
// deserialization.
void ExpandToMinSize(gfx::Rect* rect, int min_size) {
  if (rect->width() < min_size) {
    // grow width to min_size in +x direction
    // (may be clamped if x + min_size overflows)
    rect->set_width(min_size);
  }

  // if previous attempt failed due to overflow
  if (rect->width() < min_size) {
    // grow width to min_size in -x direction
    rect->Offset(-(min_size - rect->width()), 0);
    rect->set_width(min_size);
  }

  if (rect->height() < min_size) {
    // grow height to min_size in +y direction
    // (may be clamped if y + min_size overflows)
    rect->set_height(min_size);
  }

  // if previous attempt failed due to overflow
  if (rect->height() < min_size) {
    // grow height to min_size in -y direction
    rect->Offset(0, -(min_size - rect->height()));
    rect->set_height(min_size);
  }
}

// Allocate and map memory for a bitmap filled with |color|.
FuzzedBitmap AllocateFuzzedBitmap(const gfx::Size& size, SkColor color) {
  SharedBitmapId shared_bitmap_id = SharedBitmap::GenerateId();
  std::unique_ptr<base::SharedMemory> shared_memory =
      bitmap_allocation::AllocateMappedBitmap(size, RGBA_8888);

  SkBitmap tile;
  SkImageInfo info = SkImageInfo::MakeN32Premul(size.width(), size.height());
  tile.installPixels(info, shared_memory->memory(), info.minRowBytes());
  tile.eraseColor(color);

  return FuzzedBitmap(shared_bitmap_id, size, std::move(shared_memory));
}

}  // namespace

FuzzedBitmap::FuzzedBitmap(SharedBitmapId id,
                           gfx::Size size,
                           std::unique_ptr<base::SharedMemory> shared_memory)
    : id(id), size(size), shared_memory(std::move(shared_memory)) {}
FuzzedBitmap::~FuzzedBitmap() = default;
FuzzedBitmap::FuzzedBitmap(FuzzedBitmap&& other) noexcept = default;

FuzzedData::FuzzedData() = default;
FuzzedData::~FuzzedData() = default;
FuzzedData::FuzzedData(FuzzedData&& other) noexcept = default;

FuzzedData GenerateFuzzedCompositorFrame(
    const content::fuzzing::proto::RenderPass& render_pass_spec) {
  FuzzedData fuzzed_data;
  uint32_t allocated_bytes = 0;

  fuzzed_data.frame.metadata.begin_frame_ack.source_id =
      BeginFrameArgs::kManualSourceId;
  fuzzed_data.frame.metadata.begin_frame_ack.sequence_number =
      BeginFrameArgs::kStartingFrameNumber;
  fuzzed_data.frame.metadata.begin_frame_ack.has_damage = true;
  fuzzed_data.frame.metadata.frame_token = 1;
  fuzzed_data.frame.metadata.device_scale_factor = 1;
  fuzzed_data.frame.metadata.local_surface_id_allocation_time =
      base::TimeTicks::Now();

  std::unique_ptr<RenderPass> pass = RenderPass::Create();
  gfx::Rect rp_output_rect =
      GetRectFromProtobuf(render_pass_spec.output_rect());
  gfx::Rect rp_damage_rect =
      GetRectFromProtobuf(render_pass_spec.damage_rect());

  // Handle constraints on RenderPass:
  // Ensure that |rp_output_rect| has non-zero area and that |rp_damage_rect| is
  // contained in |rp_output_rect|.
  ExpandToMinSize(&rp_output_rect, 1);
  rp_damage_rect.AdjustToFit(rp_output_rect);

  pass->SetNew(1, rp_output_rect, rp_damage_rect, gfx::Transform());

  for (const auto& quad_spec : render_pass_spec.quad_list()) {
    if (quad_spec.quad_case() ==
        content::fuzzing::proto::DrawQuad::QUAD_NOT_SET) {
      continue;
    }

    gfx::Rect quad_rect = GetRectFromProtobuf(quad_spec.rect());
    gfx::Rect quad_visible_rect = GetRectFromProtobuf(quad_spec.visible_rect());

    // Handle constraints on DrawQuad:
    // Ensure that |quad_rect| has non-zero area and that |quad_visible_rect|
    // is contained in |quad_rect|.
    ExpandToMinSize(&quad_rect, 1);
    quad_visible_rect.AdjustToFit(quad_rect);

    auto* shared_quad_state = pass->CreateAndAppendSharedQuadState();
    shared_quad_state->SetAll(
        GetTransformFromProtobuf(quad_spec.sqs().transform()),
        GetRectFromProtobuf(quad_spec.sqs().layer_rect()),
        GetRectFromProtobuf(quad_spec.sqs().visible_rect()), gfx::RRectF(),
        GetRectFromProtobuf(quad_spec.sqs().clip_rect()),
        quad_spec.sqs().is_clipped(), quad_spec.sqs().are_contents_opaque(),
        Normalize(quad_spec.sqs().opacity()), SkBlendMode::kSrcOver,
        quad_spec.sqs().sorting_context_id());

    switch (quad_spec.quad_case()) {
      case content::fuzzing::proto::DrawQuad::kSolidColorQuad: {
        auto* color_quad = pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
        color_quad->SetNew(
            shared_quad_state, quad_rect, quad_visible_rect,
            quad_spec.solid_color_quad().color(),
            quad_spec.solid_color_quad().force_anti_aliasing_off());
        break;
      }
      case content::fuzzing::proto::DrawQuad::kTileQuad: {
        gfx::Size tile_size =
            GetSizeFromProtobuf(quad_spec.tile_quad().texture_size());

        // Skip TileDrawQuads with bitmaps that cannot be allocated (size is 0
        // or would overflow our limit on allocated memory for this
        // CompositorFrame.)
        uint64_t tile_bytes;
        if (!ResourceSizes::MaybeSizeInBytes<uint64_t>(tile_size, RGBA_8888,
                                                       &tile_bytes) ||
            tile_bytes > kMaxMappedMemory - allocated_bytes) {
          continue;
        }

        FuzzedBitmap fuzzed_bitmap = AllocateFuzzedBitmap(
            tile_size, quad_spec.tile_quad().texture_color());
        allocated_bytes += tile_bytes;

        TransferableResource transferable_resource =
            TransferableResource::MakeSoftware(fuzzed_bitmap.id,
                                               fuzzed_bitmap.size, RGBA_8888);

        auto* tile_quad = pass->CreateAndAppendDrawQuad<TileDrawQuad>();
        tile_quad->SetNew(
            shared_quad_state, quad_rect, quad_visible_rect,
            quad_spec.tile_quad().needs_blending(), transferable_resource.id,
            gfx::RectF(
                GetRectFromProtobuf(quad_spec.tile_quad().tex_coord_rect())),
            tile_size, quad_spec.tile_quad().swizzle_contents(),
            quad_spec.tile_quad().is_premultiplied(),
            quad_spec.tile_quad().nearest_neighbor(),
            quad_spec.tile_quad().force_anti_aliasing_off());

        fuzzed_data.frame.resource_list.push_back(transferable_resource);
        fuzzed_data.allocated_bitmaps.push_back(std::move(fuzzed_bitmap));

        break;
      }
      case content::fuzzing::proto::DrawQuad::QUAD_NOT_SET: {
        NOTREACHED();
      }
    }
  }

  fuzzed_data.frame.render_pass_list.push_back(std::move(pass));
  return fuzzed_data;
}

}  // namespace viz
