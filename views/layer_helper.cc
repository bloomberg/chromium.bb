// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/layer_helper.h"

#include "views/layer_property_setter.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/transform.h"

namespace views {

namespace internal {

LayerHelper::LayerHelper()
    : fills_bounds_opaquely_(false),
      paint_to_layer_(false),
      property_setter_explicitly_set_(false),
      needs_paint_all_(true) {
}

LayerHelper::~LayerHelper() {
  if (property_setter_.get() && layer_.get())
    property_setter_->Uninstalled(layer_.get());
  property_setter_.reset();
  layer_.reset();
}

void LayerHelper::SetTransform(const ui::Transform& transform) {
  transform_.reset(transform.HasChange() ? new ui::Transform(transform) : NULL);
}

void LayerHelper::SetLayer(ui::Layer* layer) {
  if (property_setter_.get() && this->layer())
    property_setter_->Uninstalled(this->layer());
  layer_.reset(layer);
  if (layer) {
    if (!property_setter_.get())
      property_setter_.reset(LayerPropertySetter::CreateDefaultSetter());
    property_setter_->Installed(this->layer());
  } else if (!property_setter_explicitly_set_) {
    property_setter_.reset(NULL);
  }
}

void LayerHelper::SetPropertySetter(LayerPropertySetter* setter) {
  if (property_setter_.get() && layer())
    property_setter_->Uninstalled(layer());
  property_setter_.reset(setter);
  if (layer()) {
    if (!setter)
      property_setter_.reset(LayerPropertySetter::CreateDefaultSetter());
    property_setter_->Installed(layer());
  }
}

bool LayerHelper::ShouldPaintToLayer() const {
  return paint_to_layer_ || (transform_.get() && transform_->HasChange());
}

} // namespace internal

}  // namespace views
