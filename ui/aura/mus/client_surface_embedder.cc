// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/client_surface_embedder.h"

#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_owner.h"
#include "ui/gfx/geometry/dip_util.h"

namespace aura {

ClientSurfaceEmbedder::ClientSurfaceEmbedder(Window* window)
    : window_(window),
      surface_layer_owner_(std::make_unique<ui::LayerOwner>(
          std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR))) {
  surface_layer_owner_->layer()->set_name("ClientSurfaceEmbedder");
  surface_layer_owner_->layer()->SetMasksToBounds(true);
  // The frame provided by the parent window->layer() needs to show through
  // the surface layer.
  surface_layer_owner_->layer()->SetFillsBoundsOpaquely(false);

  window_->layer()->Add(surface_layer_owner_->layer());

  // Window's layer may contain content from this client (the embedder), e.g.
  // this is the case with window decorations provided by Window Manager.
  // This content should appear underneath the content of the embedded client.
  window_->layer()->StackAtTop(surface_layer_owner_->layer());
}

ClientSurfaceEmbedder::~ClientSurfaceEmbedder() = default;

void ClientSurfaceEmbedder::SetSurfaceId(const viz::SurfaceId& surface_id) {
  // Set the background to transparent to avoid a flash of color before the
  // client surface is rendered. See https://crbug.com/930199
  const gfx::Size size = window_->bounds().size();
  surface_layer_owner_->layer()->SetBounds(gfx::Rect(size));
  surface_layer_owner_->layer()->SetShowSurface(
      surface_id, size, SK_ColorTRANSPARENT,
      cc::DeadlinePolicy::UseDefaultDeadline(),
      false /* stretch_content_to_fill_bounds */);
  window_->layer()->StackAtTop(surface_layer_owner_->layer());
}

viz::SurfaceId ClientSurfaceEmbedder::GetSurfaceId() const {
  const viz::SurfaceId* id = surface_layer_owner_->layer()->GetSurfaceId();
  return id ? *id : viz::SurfaceId();
}

}  // namespace aura
