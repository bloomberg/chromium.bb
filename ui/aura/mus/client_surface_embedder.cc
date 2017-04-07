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

ClientSurfaceEmbedder::ClientSurfaceEmbedder(
    Window* window,
    const gfx::Insets& client_area_insets)
    : window_(window), client_area_insets_(client_area_insets) {
  surface_layer_ = base::MakeUnique<ui::Layer>(ui::LAYER_TEXTURED);
  surface_layer_->SetMasksToBounds(true);
  // The frame provided by the parent window->layer() needs to show through
  // the surface layer.
  surface_layer_->SetFillsBoundsOpaquely(false);

  window_->layer()->Add(surface_layer_.get());

  // Window's layer may contain content from this client (the embedder), e.g.
  // this is the case with window decorations provided by Window Manager.
  // This content should appear underneath the content of the embedded client.
  window_->layer()->StackAtTop(surface_layer_.get());
}

ClientSurfaceEmbedder::~ClientSurfaceEmbedder() = default;

void ClientSurfaceEmbedder::SetPrimarySurfaceInfo(
    const cc::SurfaceInfo& surface_info) {
  surface_layer_->SetShowPrimarySurface(
      surface_info, make_scoped_refptr(new StubSurfaceReferenceFactory));
  surface_layer_->SetBounds(gfx::Rect(window_->bounds().size()));
}

void ClientSurfaceEmbedder::SetFallbackSurfaceInfo(
    const cc::SurfaceInfo& surface_info) {
  surface_layer_->SetFallbackSurface(surface_info);
  UpdateSizeAndGutters();
}

void ClientSurfaceEmbedder::UpdateSizeAndGutters() {
  surface_layer_->SetBounds(gfx::Rect(window_->bounds().size()));
  // TODO(fsamuel): Fix this for high DPI.
  gfx::Size fallback_surface_size(
      surface_layer_->GetFallbackSurfaceInfo()
          ? surface_layer_->GetFallbackSurfaceInfo()->size_in_pixels()
          : gfx::Size());
  gfx::Rect window_bounds(window_->bounds());
  if (fallback_surface_size.width() < window_bounds.width()) {
    right_gutter_ = base::MakeUnique<ui::Layer>(ui::LAYER_SOLID_COLOR);
    // TODO(fsamuel): Use the embedded client's background color.
    right_gutter_->SetColor(SK_ColorWHITE);
    int width = window_bounds.width() - fallback_surface_size.width();
    // The right gutter also includes the bottom-right corner, if necessary.
    int height = window_bounds.height() - client_area_insets_.height();
    right_gutter_->SetBounds(
        gfx::Rect(client_area_insets_.left() + fallback_surface_size.width(),
                  client_area_insets_.top(), width, height));
    window_->layer()->Add(right_gutter_.get());
  } else {
    right_gutter_.reset();
  }

  // Only create a bottom gutter if a fallback surface is available. Otherwise,
  // the right gutter will fill the whole window until a fallback is available.
  if (!fallback_surface_size.IsEmpty() &&
      fallback_surface_size.height() < window_bounds.height()) {
    bottom_gutter_ = base::MakeUnique<ui::Layer>(ui::LAYER_SOLID_COLOR);
    // TODO(fsamuel): Use the embedded client's background color.
    bottom_gutter_->SetColor(SK_ColorWHITE);
    int width = fallback_surface_size.width();
    int height = window_bounds.height() - fallback_surface_size.height();
    bottom_gutter_->SetBounds(
        gfx::Rect(0, fallback_surface_size.height(), width, height));
    window_->layer()->Add(bottom_gutter_.get());
  } else {
    bottom_gutter_.reset();
  }
  window_->layer()->StackAtTop(surface_layer_.get());
}

}  // namespace aura
