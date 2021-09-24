// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_QUADS_YUV_VIDEO_DRAW_QUAD_H_
#define COMPONENTS_VIZ_COMMON_QUADS_YUV_VIDEO_DRAW_QUAD_H_

#include "components/viz/common/quads/draw_quad.h"
#include "components/viz/common/resources/resource_id.h"
#include "components/viz/common/viz_common_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/video_types.h"

namespace viz {

class VIZ_COMMON_EXPORT YUVVideoDrawQuad : public DrawQuad {
 public:
  static const size_t kYPlaneResourceIdIndex = 0;
  static const size_t kUPlaneResourceIdIndex = 1;
  static const size_t kVPlaneResourceIdIndex = 2;
  static const size_t kAPlaneResourceIdIndex = 3;

  enum : uint32_t { kMinBitsPerChannel = 8, kMaxBitsPerChannel = 24 };

  ~YUVVideoDrawQuad() override;

  YUVVideoDrawQuad();
  YUVVideoDrawQuad(const YUVVideoDrawQuad& other);

  void SetNew(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              // |*_rect| contains non-normalized coordinates.
              // TODO(reveman): Make the use of normalized vs non-normalized
              // coordinates consistent across all quad types: crbug.com/487370
              const gfx::RectF& ya_rect,
              const gfx::RectF& uv_rect,
              const gfx::Size& ya_size,
              const gfx::Size& uv_size,
              ResourceId y_plane_resource_id,
              ResourceId u_plane_resource_id,
              ResourceId v_plane_resource_id,
              ResourceId a_plane_resource_id,
              const gfx::ColorSpace& color_space,
              float offset,
              float multiplier,
              uint32_t bits);

  void SetAll(const SharedQuadState* shared_quad_state,
              const gfx::Rect& rect,
              const gfx::Rect& visible_rect,
              bool needs_blending,
              // |*_rect| contains non-normalized coordinates.
              // TODO(reveman): Make the use of normalized vs non-normalized
              // coordinates consistent across all quad types: crbug.com/487370
              const gfx::RectF& ya_rect,
              const gfx::RectF& uv_rect,
              const gfx::Size& ya_size,
              const gfx::Size& uv_size,
              ResourceId y_plane_resource_id,
              ResourceId u_plane_resource_id,
              ResourceId v_plane_resource_id,
              ResourceId a_plane_resource_id,
              const gfx::ColorSpace& color_space,
              float offset,
              float multiplier,
              uint32_t bits,
              gfx::ProtectedVideoType video_type,
              gfx::HDRMetadata metadata);

  gfx::RectF ya_tex_coord_rect;
  gfx::RectF uv_tex_coord_rect;
  gfx::Size ya_tex_size;
  gfx::Size uv_tex_size;
  float resource_offset = 0.0f;
  float resource_multiplier = 1.0f;
  uint32_t bits_per_channel = 8;
  // TODO(hubbe): Move to ResourceProvider::ScopedSamplerGL.
  gfx::ColorSpace video_color_space;
  gfx::ProtectedVideoType protected_video_type =
      gfx::ProtectedVideoType::kClear;
  gfx::HDRMetadata hdr_metadata;

  // This optional damage is in target render pass coordinate space.
  absl::optional<gfx::Rect> damage_rect;

  static const YUVVideoDrawQuad* MaterialCast(const DrawQuad*);

  ResourceId y_plane_resource_id() const {
    return resources.ids[kYPlaneResourceIdIndex];
  }
  ResourceId u_plane_resource_id() const {
    return resources.ids[kUPlaneResourceIdIndex];
  }
  ResourceId v_plane_resource_id() const {
    return resources.ids[kVPlaneResourceIdIndex];
  }
  ResourceId a_plane_resource_id() const {
    return resources.ids[kAPlaneResourceIdIndex];
  }

 private:
  void ExtendValue(base::trace_event::TracedValue* value) const override;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_QUADS_YUV_VIDEO_DRAW_QUAD_H_
