// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/client_surface_embedder.h"

#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_owner.h"
#include "ui/gfx/geometry/dip_util.h"

namespace aura {
namespace {

// Returns the target visibility respecting the window hierarchy. It returns
// true only when the target visibility of the window and all its ancestors are
// true. This can check if the window is going to be drawn without the effect
// of LayerAnimator.
bool GetTargetVisibility(aura::Window* window) {
  if (!window)
    return false;
  while (window && window->TargetVisibility())
    window = window->parent();
  return window == nullptr;
}

}  // namespace

ClientSurfaceEmbedder::ClientSurfaceEmbedder(Window* window)
    : window_(window),
      surface_layer_owner_(std::make_unique<ui::LayerOwner>(
          std::make_unique<ui::Layer>(ui::LAYER_SOLID_COLOR))) {
  surface_layer_owner_->layer()->set_name("ClientSurfaceEmbedder");
  surface_layer_owner_->layer()->SetMasksToBounds(true);
  // The frame provided by the parent window->layer() needs to show through
  // the surface layer.
  surface_layer_owner_->layer()->SetFillsBoundsOpaquely(false);
  surface_layer_owner_->layer()->SetVisible(GetTargetVisibility(window_));

  window_->layer()->Add(surface_layer_owner_->layer());

  // Window's layer may contain content from this client (the embedder), e.g.
  // this is the case with window decorations provided by Window Manager.
  // This content should appear underneath the content of the embedded client.
  window_->layer()->StackAtTop(surface_layer_owner_->layer());
  window_->AddObserver(this);
}

ClientSurfaceEmbedder::~ClientSurfaceEmbedder() {
  window_->RemoveObserver(this);
}

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

void ClientSurfaceEmbedder::OnWindowVisibilityChanged(Window* window,
                                                      bool visible) {
  if (window->Contains(window_))
    surface_layer_owner_->layer()->SetVisible(GetTargetVisibility(window_));
}

}  // namespace aura
