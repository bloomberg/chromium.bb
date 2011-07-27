// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COMPOSITOR_LAYER_H_
#define UI_GFX_COMPOSITOR_LAYER_H_
#pragma once

#include <vector>

#include "base/memory/ref_counted.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"

class SkCanvas;

namespace ui {

class Compositor;
class Texture;

// Layer manages a texture, transform and a set of child Layers. Any View that
// has enabled layers ends up creating a Layer to manage the texture.
//
// NOTE: unlike Views, each Layer does *not* own its children views. If you
// delete a Layer and it has children, the parent of each child layer is set to
// NULL, but the children are not deleted.
class Layer {
 public:
  explicit Layer(Compositor* compositor);
  ~Layer();

  // Adds a new Layer to this Layer.
  void Add(Layer* child);

  // Removes a Layer from this Layer.
  void Remove(Layer* child);

  // Returns the child Layers.
  const std::vector<Layer*>& children() { return children_; }

  // The parent.
  const Layer* parent() const { return parent_; }
  Layer* parent() { return parent_; }

  // The transform, relative to the parent.
  void set_transform(const ui::Transform& transform) { transform_ = transform; }
  const ui::Transform& transform() const { return transform_; }

  // The bounds, relative to the parent.
  void set_bounds(const gfx::Rect& bounds) { bounds_ = bounds; }
  const gfx::Rect& bounds() const { return bounds_; }

  // The compositor.
  const Compositor* compositor() const { return compositor_; }
  Compositor* compositor() { return compositor_; }

  // Passing NULL will cause the layer to get a texture from its compositor.
  void SetTexture(ui::Texture* texture);
  const ui::Texture* texture() const { return texture_.get(); }

  // Resets the canvas of the texture.
  void SetCanvas(const SkCanvas& canvas, const gfx::Point& origin);

  // Draws the layer.
  void Draw();

 private:
  Compositor* compositor_;

  scoped_refptr<ui::Texture> texture_;

  Layer* parent_;

  std::vector<Layer*> children_;

  ui::Transform transform_;

  gfx::Rect bounds_;

  DISALLOW_COPY_AND_ASSIGN(Layer);
};

}  // namespace ui

#endif  // UI_GFX_COMPOSITOR_LAYER_H_
