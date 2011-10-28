// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COMPOSITOR_LAYER_PROPERTY_SETTER_H_
#define UI_GFX_COMPOSITOR_LAYER_PROPERTY_SETTER_H_
#pragma once

#include "views/views_export.h"

namespace gfx {
class Rect;
}

namespace ui {
class Layer;
class Transform;
}

namespace views {

// When a property of layer needs to be changed it is set by way of
// LayerPropertySetter. This enables LayerPropertySetter to animate property
// changes.
class VIEWS_EXPORT LayerPropertySetter {
 public:
  // Creates a LayerPropertySetter that immediately sets the values on the
  // layer. Ownership returns to caller.
  static LayerPropertySetter* CreateDefaultSetter();

  virtual ~LayerPropertySetter() {}

  // Invoked when the LayerPropertySetter is to be used for a particular Layer.
  // This is invoked whenever a new Layer is created. A LayerPropertySetter is
  // only associated with one Layer at a time.
  virtual void Installed(ui::Layer* layer) = 0;

  // Invoked when the Layer is destroyed.
  virtual void Uninstalled(ui::Layer* layer) = 0;

  // Sets the transform on the Layer.
  virtual void SetTransform(ui::Layer* layer,
                            const ui::Transform& transform) = 0;

  // Sets the bounds of the layer.
  virtual void SetBounds(ui::Layer* layer, const gfx::Rect& bounds) = 0;
};

}  // namespace views

#endif  // UI_GFX_COMPOSITOR_LAYER_PROPERTY_SETTER_H_
