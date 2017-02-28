// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/client_surface_embedder.h"

#include "cc/surfaces/surface_reference_factory.h"
#include "ui/aura/window.h"

namespace aura {
namespace {

// TODO(mfomitchev, samans): Remove these stub classes once the SurfaceReference
// work is complete.
class StubSurfaceReferenceFactory : public cc::SurfaceReferenceFactory {
 public:
  StubSurfaceReferenceFactory() = default;

  // cc::SurfaceReferenceFactory:
  base::Closure CreateReference(
      cc::SurfaceReferenceOwner* owner,
      const cc::SurfaceId& surface_id) const override {
    return base::Closure();
  }

 protected:
  ~StubSurfaceReferenceFactory() override = default;

  DISALLOW_COPY_AND_ASSIGN(StubSurfaceReferenceFactory);
};
}  // namespace

ClientSurfaceEmbedder::ClientSurfaceEmbedder(Window* window) : window_(window) {
  surface_layer_ = base::MakeUnique<ui::Layer>(ui::LAYER_TEXTURED);
  surface_layer_->SetVisible(true);
  // The frame provided by the parent window->layer() needs to show through
  // the surface layer.
  surface_layer_->SetFillsBoundsOpaquely(false);

  clip_layer_ = base::MakeUnique<ui::Layer>(ui::LAYER_NOT_DRAWN);
  clip_layer_->SetFillsBoundsOpaquely(false);

  clip_layer_->Add(surface_layer_.get());
  window_->layer()->Add(clip_layer_.get());

  // Window's layer may contain content from this client (the embedder), e.g.
  // this is the case with window decorations provided by Window Manager.
  // This content should appear underneath the content of the embedded client.
  window_->layer()->StackAtTop(clip_layer_.get());

  // We can't set this on window's layer, because that would clip the window
  // shadow.
  clip_layer_->SetMasksToBounds(true);
}

ClientSurfaceEmbedder::~ClientSurfaceEmbedder() = default;

void ClientSurfaceEmbedder::UpdateSurface(const cc::SurfaceInfo& surface_info) {
  // TODO(mfomitchev): Currently the frame size may not match the window size.
  // In the future the surface id will be created by Ash (and used with the
  // surface layer) when the window resize happens, which will ensure that the
  // surface size matches the window size (unless a timeout occurs).
  gfx::Size frame_size = surface_info.size_in_pixels();
  surface_layer_->SetBounds(
      gfx::Rect(0, 0, frame_size.width(), frame_size.height()));
  // Clip to window bounds.
  clip_layer_->SetBounds(
      gfx::Rect(0, 0, window_->bounds().width(), window_->bounds().height()));

  surface_layer_->SetShowPrimarySurface(
      surface_info, make_scoped_refptr(new StubSurfaceReferenceFactory));
}

}  // namespace aura
