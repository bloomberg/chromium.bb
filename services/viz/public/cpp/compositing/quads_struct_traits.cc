// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/quads_struct_traits.h"

#include "ui/gfx/mojo/transform_struct_traits.h"

namespace mojo {

viz::DrawQuad* AllocateAndConstruct(
    viz::mojom::DrawQuadStateDataView::Tag material,
    cc::QuadList* list) {
  viz::DrawQuad* quad = nullptr;
  switch (material) {
    case viz::mojom::DrawQuadStateDataView::Tag::DEBUG_BORDER_QUAD_STATE:
      quad = list->AllocateAndConstruct<cc::DebugBorderDrawQuad>();
      quad->material = viz::DrawQuad::DEBUG_BORDER;
      return quad;
    case viz::mojom::DrawQuadStateDataView::Tag::RENDER_PASS_QUAD_STATE:
      quad = list->AllocateAndConstruct<cc::RenderPassDrawQuad>();
      quad->material = viz::DrawQuad::RENDER_PASS;
      return quad;
    case viz::mojom::DrawQuadStateDataView::Tag::SOLID_COLOR_QUAD_STATE:
      quad = list->AllocateAndConstruct<cc::SolidColorDrawQuad>();
      quad->material = viz::DrawQuad::SOLID_COLOR;
      return quad;
    case viz::mojom::DrawQuadStateDataView::Tag::STREAM_VIDEO_QUAD_STATE:
      quad = list->AllocateAndConstruct<cc::StreamVideoDrawQuad>();
      quad->material = viz::DrawQuad::STREAM_VIDEO_CONTENT;
      return quad;
    case viz::mojom::DrawQuadStateDataView::Tag::SURFACE_QUAD_STATE:
      quad = list->AllocateAndConstruct<cc::SurfaceDrawQuad>();
      quad->material = viz::DrawQuad::SURFACE_CONTENT;
      return quad;
    case viz::mojom::DrawQuadStateDataView::Tag::TEXTURE_QUAD_STATE:
      quad = list->AllocateAndConstruct<cc::TextureDrawQuad>();
      quad->material = viz::DrawQuad::TEXTURE_CONTENT;
      return quad;
    case viz::mojom::DrawQuadStateDataView::Tag::TILE_QUAD_STATE:
      quad = list->AllocateAndConstruct<cc::TileDrawQuad>();
      quad->material = viz::DrawQuad::TILED_CONTENT;
      return quad;
    case viz::mojom::DrawQuadStateDataView::Tag::YUV_VIDEO_QUAD_STATE:
      quad = list->AllocateAndConstruct<cc::YUVVideoDrawQuad>();
      quad->material = viz::DrawQuad::YUV_VIDEO_CONTENT;
      return quad;
  }
  NOTREACHED();
  return nullptr;
}

// static
bool StructTraits<viz::mojom::DebugBorderQuadStateDataView, viz::DrawQuad>::
    Read(viz::mojom::DebugBorderQuadStateDataView data, viz::DrawQuad* out) {
  cc::DebugBorderDrawQuad* quad = static_cast<cc::DebugBorderDrawQuad*>(out);
  quad->color = data.color();
  quad->width = data.width();
  return true;
}

// static
bool StructTraits<viz::mojom::RenderPassQuadStateDataView, viz::DrawQuad>::Read(
    viz::mojom::RenderPassQuadStateDataView data,
    viz::DrawQuad* out) {
  cc::RenderPassDrawQuad* quad = static_cast<cc::RenderPassDrawQuad*>(out);
  quad->resources.ids[cc::RenderPassDrawQuad::kMaskResourceIdIndex] =
      data.mask_resource_id();
  quad->resources.count = data.mask_resource_id() ? 1 : 0;
  quad->render_pass_id = data.render_pass_id();
  // RenderPass ids are never zero.
  if (!quad->render_pass_id)
    return false;
  return data.ReadMaskUvRect(&quad->mask_uv_rect) &&
         data.ReadMaskTextureSize(&quad->mask_texture_size) &&
         data.ReadFiltersScale(&quad->filters_scale) &&
         data.ReadFiltersOrigin(&quad->filters_origin) &&
         data.ReadTexCoordRect(&quad->tex_coord_rect);
}

// static
bool StructTraits<viz::mojom::SolidColorQuadStateDataView, viz::DrawQuad>::Read(
    viz::mojom::SolidColorQuadStateDataView data,
    viz::DrawQuad* out) {
  cc::SolidColorDrawQuad* quad = static_cast<cc::SolidColorDrawQuad*>(out);
  quad->force_anti_aliasing_off = data.force_anti_aliasing_off();
  quad->color = data.color();
  return true;
}

// static
bool StructTraits<viz::mojom::StreamVideoQuadStateDataView, viz::DrawQuad>::
    Read(viz::mojom::StreamVideoQuadStateDataView data, viz::DrawQuad* out) {
  cc::StreamVideoDrawQuad* quad = static_cast<cc::StreamVideoDrawQuad*>(out);
  quad->resources.ids[cc::StreamVideoDrawQuad::kResourceIdIndex] =
      data.resource_id();
  quad->resources.count = 1;
  return data.ReadResourceSizeInPixels(
             &quad->overlay_resources
                  .size_in_pixels[cc::StreamVideoDrawQuad::kResourceIdIndex]) &&
         data.ReadMatrix(&quad->matrix);
}

// static
viz::mojom::SurfaceDrawQuadType
EnumTraits<viz::mojom::SurfaceDrawQuadType, cc::SurfaceDrawQuadType>::ToMojom(
    cc::SurfaceDrawQuadType surface_draw_quad_type) {
  switch (surface_draw_quad_type) {
    case cc::SurfaceDrawQuadType::PRIMARY:
      return viz::mojom::SurfaceDrawQuadType::PRIMARY;
    case cc::SurfaceDrawQuadType::FALLBACK:
      return viz::mojom::SurfaceDrawQuadType::FALLBACK;
  }
  NOTREACHED();
  return viz::mojom::SurfaceDrawQuadType::PRIMARY;
}

// static
bool EnumTraits<viz::mojom::SurfaceDrawQuadType, cc::SurfaceDrawQuadType>::
    FromMojom(viz::mojom::SurfaceDrawQuadType input,
              cc::SurfaceDrawQuadType* out) {
  switch (input) {
    case viz::mojom::SurfaceDrawQuadType::PRIMARY:
      *out = cc::SurfaceDrawQuadType::PRIMARY;
      return true;
    case viz::mojom::SurfaceDrawQuadType::FALLBACK:
      *out = cc::SurfaceDrawQuadType::FALLBACK;
      return true;
  }
  return false;
}

// static
bool StructTraits<viz::mojom::SurfaceQuadStateDataView, viz::DrawQuad>::Read(
    viz::mojom::SurfaceQuadStateDataView data,
    viz::DrawQuad* out) {
  cc::SurfaceDrawQuad* quad = static_cast<cc::SurfaceDrawQuad*>(out);
  return data.ReadSurfaceDrawQuadType(&quad->surface_draw_quad_type) &&
         data.ReadSurface(&quad->surface_id);
}

// static
bool StructTraits<viz::mojom::TextureQuadStateDataView, viz::DrawQuad>::Read(
    viz::mojom::TextureQuadStateDataView data,
    viz::DrawQuad* out) {
  cc::TextureDrawQuad* quad = static_cast<cc::TextureDrawQuad*>(out);

  quad->resources.ids[cc::TextureDrawQuad::kResourceIdIndex] =
      data.resource_id();
  if (!data.ReadResourceSizeInPixels(
          &quad->overlay_resources
               .size_in_pixels[cc::TextureDrawQuad::kResourceIdIndex])) {
    return false;
  }

  quad->resources.count = 1;
  quad->premultiplied_alpha = data.premultiplied_alpha();
  if (!data.ReadUvTopLeft(&quad->uv_top_left) ||
      !data.ReadUvBottomRight(&quad->uv_bottom_right)) {
    return false;
  }
  quad->background_color = data.background_color();
  base::span<float> vertex_opacity_array(quad->vertex_opacity);
  if (!data.ReadVertexOpacity(&vertex_opacity_array))
    return false;

  quad->y_flipped = data.y_flipped();
  quad->nearest_neighbor = data.nearest_neighbor();
  quad->secure_output_only = data.secure_output_only();
  return true;
}

// static
bool StructTraits<viz::mojom::TileQuadStateDataView, viz::DrawQuad>::Read(
    viz::mojom::TileQuadStateDataView data,
    viz::DrawQuad* out) {
  cc::TileDrawQuad* quad = static_cast<cc::TileDrawQuad*>(out);
  if (!data.ReadTexCoordRect(&quad->tex_coord_rect) ||
      !data.ReadTextureSize(&quad->texture_size)) {
    return false;
  }

  quad->swizzle_contents = data.swizzle_contents();
  quad->nearest_neighbor = data.nearest_neighbor();
  quad->resources.ids[cc::TileDrawQuad::kResourceIdIndex] = data.resource_id();
  quad->resources.count = 1;
  return true;
}

viz::mojom::YUVColorSpace
EnumTraits<viz::mojom::YUVColorSpace, cc::YUVVideoDrawQuad::ColorSpace>::
    ToMojom(cc::YUVVideoDrawQuad::ColorSpace color_space) {
  switch (color_space) {
    case cc::YUVVideoDrawQuad::REC_601:
      return viz::mojom::YUVColorSpace::REC_601;
    case cc::YUVVideoDrawQuad::REC_709:
      return viz::mojom::YUVColorSpace::REC_709;
    case cc::YUVVideoDrawQuad::JPEG:
      return viz::mojom::YUVColorSpace::JPEG;
  }
  NOTREACHED();
  return viz::mojom::YUVColorSpace::JPEG;
}

// static
bool EnumTraits<viz::mojom::YUVColorSpace, cc::YUVVideoDrawQuad::ColorSpace>::
    FromMojom(viz::mojom::YUVColorSpace input,
              cc::YUVVideoDrawQuad::ColorSpace* out) {
  switch (input) {
    case viz::mojom::YUVColorSpace::REC_601:
      *out = cc::YUVVideoDrawQuad::REC_601;
      return true;
    case viz::mojom::YUVColorSpace::REC_709:
      *out = cc::YUVVideoDrawQuad::REC_709;
      return true;
    case viz::mojom::YUVColorSpace::JPEG:
      *out = cc::YUVVideoDrawQuad::JPEG;
      return true;
  }
  return false;
}

// static
bool StructTraits<viz::mojom::YUVVideoQuadStateDataView, viz::DrawQuad>::Read(
    viz::mojom::YUVVideoQuadStateDataView data,
    viz::DrawQuad* out) {
  cc::YUVVideoDrawQuad* quad = static_cast<cc::YUVVideoDrawQuad*>(out);
  if (!data.ReadYaTexCoordRect(&quad->ya_tex_coord_rect) ||
      !data.ReadUvTexCoordRect(&quad->uv_tex_coord_rect) ||
      !data.ReadYaTexSize(&quad->ya_tex_size) ||
      !data.ReadUvTexSize(&quad->uv_tex_size) ||
      !data.ReadVideoColorSpace(&quad->video_color_space)) {
    return false;
  }
  quad->resources.ids[cc::YUVVideoDrawQuad::kYPlaneResourceIdIndex] =
      data.y_plane_resource_id();
  quad->resources.ids[cc::YUVVideoDrawQuad::kUPlaneResourceIdIndex] =
      data.u_plane_resource_id();
  quad->resources.ids[cc::YUVVideoDrawQuad::kVPlaneResourceIdIndex] =
      data.v_plane_resource_id();
  quad->resources.ids[cc::YUVVideoDrawQuad::kAPlaneResourceIdIndex] =
      data.a_plane_resource_id();
  static_assert(cc::YUVVideoDrawQuad::kAPlaneResourceIdIndex ==
                    viz::DrawQuad::Resources::kMaxResourceIdCount - 1,
                "The A plane resource should be the last resource ID.");
  quad->resources.count = data.a_plane_resource_id() ? 4 : 3;

  if (!data.ReadColorSpace(&quad->color_space))
    return false;
  quad->resource_offset = data.resource_offset();
  quad->resource_multiplier = data.resource_multiplier();
  quad->bits_per_channel = data.bits_per_channel();
  if (quad->bits_per_channel < cc::YUVVideoDrawQuad::kMinBitsPerChannel ||
      quad->bits_per_channel > cc::YUVVideoDrawQuad::kMaxBitsPerChannel) {
    return false;
  }
  quad->require_overlay = data.require_overlay();
  return true;
}

// static
bool StructTraits<viz::mojom::DrawQuadDataView, viz::DrawQuad>::Read(
    viz::mojom::DrawQuadDataView data,
    viz::DrawQuad* out) {
  if (!data.ReadRect(&out->rect) || !data.ReadVisibleRect(&out->visible_rect)) {
    return false;
  }
  out->needs_blending = data.needs_blending();
  return data.ReadDrawQuadState(out);
}

}  // namespace mojo
