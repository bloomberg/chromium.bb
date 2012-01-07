// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/compositor.h"

#include "ui/gfx/compositor/compositor_observer.h"
#include "ui/gfx/compositor/layer.h"

namespace ui {

TextureDrawParams::TextureDrawParams()
    : blend(false),
      has_valid_alpha_channel(false),
      opacity(1.0f),
      vertically_flipped(false) {
}

// static
Compositor*(*Compositor::compositor_factory_)(CompositorDelegate*) = NULL;

Compositor::Compositor(CompositorDelegate* delegate, const gfx::Size& size)
    : delegate_(delegate),
      size_(size),
      root_layer_(NULL) {
}

Compositor::~Compositor() {
  if (root_layer_)
    root_layer_->SetCompositor(NULL);
}

void Compositor::ScheduleDraw() {
  delegate_->ScheduleDraw();
}

void Compositor::SetRootLayer(Layer* root_layer) {
  if (root_layer_ == root_layer)
    return;
  if (root_layer_)
    root_layer_->SetCompositor(NULL);
  root_layer_ = root_layer;
  if (root_layer_ && !root_layer_->GetCompositor())
    root_layer_->SetCompositor(this);
  OnRootLayerChanged();
}

void Compositor::Draw(bool force_clear) {
  if (!root_layer_)
    return;

  NotifyStart(force_clear);
  DrawTree();
  NotifyEnd();
}

void Compositor::AddObserver(CompositorObserver* observer) {
  observer_list_.AddObserver(observer);
}

void Compositor::RemoveObserver(CompositorObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

bool Compositor::HasObserver(CompositorObserver* observer) {
  return observer_list_.HasObserver(observer);
}

void Compositor::OnRootLayerChanged() {
  ScheduleDraw();
}

void Compositor::DrawTree() {
  root_layer_->DrawTree();
}

void Compositor::SwizzleRGBAToBGRAAndFlip(unsigned char* pixels,
                                          const gfx::Size& image_size) {
  // Swizzle from RGBA to BGRA
  size_t bitmap_size = 4 * image_size.width() * image_size.height();
  for(size_t i = 0; i < bitmap_size; i += 4)
    std::swap(pixels[i], pixels[i + 2]);

  // Vertical flip to transform from GL co-ords
  size_t row_size = 4 * image_size.width();
  scoped_array<unsigned char> tmp_row(new unsigned char[row_size]);
  for(int row = 0; row < image_size.height() / 2; row++) {
    memcpy(tmp_row.get(),
           &pixels[row * row_size],
           row_size);
    memcpy(&pixels[row * row_size],
           &pixels[bitmap_size - (row + 1) * row_size],
           row_size);
    memcpy(&pixels[bitmap_size - (row + 1) * row_size],
           tmp_row.get(),
           row_size);
  }
}

void Compositor::NotifyStart(bool clear) {
  OnNotifyStart(clear);
}

void Compositor::NotifyEnd() {
  OnNotifyEnd();
  FOR_EACH_OBSERVER(CompositorObserver,
                    observer_list_,
                    OnCompositingEnded(this));
}

}  // namespace ui
