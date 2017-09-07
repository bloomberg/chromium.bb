// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/compositing/render_pass_struct_traits.h"

#include "base/numerics/safe_conversions.h"
#include "base/trace_event/trace_event.h"
#include "services/viz/public/cpp/compositing/shared_quad_state_struct_traits.h"

namespace mojo {

// static
bool StructTraits<viz::mojom::RenderPassDataView,
                  std::unique_ptr<cc::RenderPass>>::
    Read(viz::mojom::RenderPassDataView data,
         std::unique_ptr<cc::RenderPass>* out) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug.ipc"),
               "StructTraits::RenderPass::Read");
  *out = cc::RenderPass::Create();
  if (!data.ReadOutputRect(&(*out)->output_rect) ||
      !data.ReadDamageRect(&(*out)->damage_rect) ||
      !data.ReadTransformToRootTarget(&(*out)->transform_to_root_target) ||
      !data.ReadFilters(&(*out)->filters) ||
      !data.ReadBackgroundFilters(&(*out)->background_filters) ||
      !data.ReadColorSpace(&(*out)->color_space)) {
    return false;
  }
  (*out)->id = data.id();
  // RenderPass ids are never zero.
  if (!(*out)->id)
    return false;
  (*out)->has_transparent_background = data.has_transparent_background();
  (*out)->cache_render_pass = data.cache_render_pass();
  (*out)->has_damage_from_contributing_content =
      data.has_damage_from_contributing_content();

  mojo::ArrayDataView<viz::mojom::DrawQuadDataView> quads;
  data.GetQuadListDataView(&quads);
  viz::SharedQuadState* last_sqs = nullptr;
  viz::DrawQuad* last_draw_quad = nullptr;
  for (size_t i = 0; i < quads.size(); ++i) {
    viz::mojom::DrawQuadDataView quad_data_view;
    quads.GetDataView(i, &quad_data_view);
    viz::mojom::DrawQuadStateDataView quad_state_data_view;
    quad_data_view.GetDrawQuadStateDataView(&quad_state_data_view);

    viz::DrawQuad* quad =
        AllocateAndConstruct(quad_state_data_view.tag(), &(*out)->quad_list);
    if (!quad)
      return false;
    if (!quads.Read(i, quad))
      return false;

    // Read the SharedQuadState.
    viz::mojom::SharedQuadStateDataView sqs_data_view;
    quad_data_view.GetSqsDataView(&sqs_data_view);
    // If there is no seralized SharedQuadState then used the last deseriaized
    // one.
    if (!sqs_data_view.is_null()) {
      last_sqs = (*out)->CreateAndAppendSharedQuadState();
      if (!quad_data_view.ReadSqs(last_sqs))
        return false;
    }
    quad->shared_quad_state = last_sqs;
    if (!quad->shared_quad_state)
      return false;

    // If this quad is a fallback SurfaceDrawQuad then update the previous
    // primary SurfaceDrawQuad to point to this quad.
    if (quad->material == viz::DrawQuad::SURFACE_CONTENT) {
      const cc::SurfaceDrawQuad* surface_draw_quad =
          cc::SurfaceDrawQuad::MaterialCast(quad);
      if (surface_draw_quad->surface_draw_quad_type ==
          cc::SurfaceDrawQuadType::FALLBACK) {
        // A fallback quad must immediately follow a primary SurfaceDrawQuad.
        if (!last_draw_quad ||
            last_draw_quad->material != viz::DrawQuad::SURFACE_CONTENT) {
          return false;
        }
        cc::SurfaceDrawQuad* last_surface_draw_quad =
            static_cast<cc::SurfaceDrawQuad*>(last_draw_quad);
        // Only one fallback quad is currently supported.
        if (last_surface_draw_quad->surface_draw_quad_type !=
            cc::SurfaceDrawQuadType::PRIMARY) {
          return false;
        }
        last_surface_draw_quad->fallback_quad = surface_draw_quad;
      }
    }
    last_draw_quad = quad;
  }
  return true;
}

}  // namespace mojo
