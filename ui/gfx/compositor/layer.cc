// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/compositor/layer.h"

#include <algorithm>

#include "base/logging.h"
#include "ui/gfx/compositor/compositor.h"

namespace ui {

Layer::Layer(Compositor* compositor)
    : compositor_(compositor),
      texture_(compositor->CreateTexture()),
      parent_(NULL) {
}

Layer::~Layer() {
  if (parent_)
    parent_->Remove(this);
  for (size_t i = 0; i < children_.size(); ++i)
    children_[i]->parent_ = NULL;
}

void Layer::Add(Layer* child) {
  if (child->parent_)
    child->parent_->Remove(child);
  child->parent_ = this;
  children_.push_back(child);
}

void Layer::Remove(Layer* child) {
  std::vector<Layer*>::iterator i =
      std::find(children_.begin(), children_.end(), child);
  DCHECK(i != children_.end());
  children_.erase(i);
  child->parent_ = NULL;
}

void Layer::SetTexture(ui::Texture* texture) {
  if (texture == NULL)
    texture_ = compositor_->CreateTexture();
  else
    texture_ = texture;
}

void Layer::SetCanvas(const SkCanvas& canvas, const gfx::Point& origin) {
  texture_->SetCanvas(canvas, origin, bounds_.size());
}

void Layer::Draw() {
  ui::TextureDrawParams texture_draw_params;
  for (Layer* layer = this; layer; layer = layer->parent_) {
    texture_draw_params.transform.ConcatTransform(layer->transform_);
    texture_draw_params.transform.ConcatTranslate(
        static_cast<float>(layer->bounds_.x()),
        static_cast<float>(layer->bounds_.y()));
  }
  // Only blend for child layers. The root layer will clobber the cleared bg.
  texture_draw_params.blend = parent_ != NULL;
  texture_->Draw(texture_draw_params);
}

}  // namespace ui
