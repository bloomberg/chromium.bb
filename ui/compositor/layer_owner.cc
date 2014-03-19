// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/layer_owner.h"

namespace ui {

LayerOwner::LayerOwner() : layer_(NULL) {
}

LayerOwner::~LayerOwner() {
}

void LayerOwner::SetLayer(Layer* layer) {
  DCHECK(!OwnsLayer());
  layer_owner_.reset(layer);
  layer_ = layer;
  layer_->owner_ = this;
}

Layer* LayerOwner::AcquireLayer() {
  if (layer_owner_)
    layer_owner_->owner_ = NULL;
  return layer_owner_.release();
}

scoped_ptr<Layer> LayerOwner::RecreateLayer() {
  return scoped_ptr<ui::Layer>();
}

void LayerOwner::DestroyLayer() {
  layer_ = NULL;
  layer_owner_.reset();
}

bool LayerOwner::OwnsLayer() const {
  return !!layer_owner_;
}

}  // namespace ui
