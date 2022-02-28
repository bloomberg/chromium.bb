// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/quads/picture_draw_quad.h"

#include "base/trace_event/traced_value.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "components/viz/common/resources/platform_color.h"

namespace viz {

PictureDrawQuad::PictureDrawQuad() = default;

PictureDrawQuad::PictureDrawQuad(const PictureDrawQuad& other) = default;

PictureDrawQuad::~PictureDrawQuad() = default;

void PictureDrawQuad::SetNew(const SharedQuadState* shared_quad_state,
                             const gfx::Rect& rect,
                             const gfx::Rect& visible_rect,
                             bool needs_blending,
                             const gfx::RectF& tex_coord_rect,
                             const gfx::Size& texture_size,
                             bool nearest_neighbor,
                             ResourceFormat format,
                             const gfx::Rect& content,
                             float scale,
                             ImageAnimationMap animation_map,
                             scoped_refptr<cc::DisplayItemList> display_items) {
  ContentDrawQuadBase::SetNew(shared_quad_state,
                              DrawQuad::Material::kPictureContent, rect,
                              visible_rect, needs_blending, tex_coord_rect,
                              texture_size, false, nearest_neighbor, false);
  content_rect = content;
  contents_scale = scale;
  image_animation_map = std::move(animation_map);
  display_item_list = std::move(display_items);
  texture_format = format;
}

void PictureDrawQuad::SetAll(const SharedQuadState* shared_quad_state,
                             const gfx::Rect& rect,
                             const gfx::Rect& visible_rect,
                             bool needs_blending,
                             const gfx::RectF& tex_coord_rect,
                             const gfx::Size& texture_size,
                             bool nearest_neighbor,
                             ResourceFormat format,
                             const gfx::Rect& content,
                             float scale,
                             ImageAnimationMap animation_map,
                             scoped_refptr<cc::DisplayItemList> display_items) {
  ContentDrawQuadBase::SetAll(shared_quad_state,
                              DrawQuad::Material::kPictureContent, rect,
                              visible_rect, needs_blending, tex_coord_rect,
                              texture_size, false, nearest_neighbor, false);
  content_rect = content;
  contents_scale = scale;
  image_animation_map = std::move(animation_map);
  display_item_list = std::move(display_items);
  texture_format = format;
}

const PictureDrawQuad* PictureDrawQuad::MaterialCast(const DrawQuad* quad) {
  DCHECK(quad->material == DrawQuad::Material::kPictureContent);
  return static_cast<const PictureDrawQuad*>(quad);
}

void PictureDrawQuad::ExtendValue(base::trace_event::TracedValue* value) const {
  ContentDrawQuadBase::ExtendValue(value);
  cc::MathUtil::AddToTracedValue("content_rect", content_rect, value);
  value->SetDouble("contents_scale", contents_scale);
  value->SetInteger("texture_format", texture_format);
  // TODO(piman): display_item_list?
}

}  // namespace viz
