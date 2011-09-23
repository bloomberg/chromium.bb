// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COMPOSITOR_LAYER_H_
#define UI_GFX_COMPOSITOR_LAYER_H_
#pragma once

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/compositor/layer_delegate.h"

class SkCanvas;

namespace ui {

class Compositor;
class Texture;

// Layer manages a texture, transform and a set of child Layers. Any View that
// has enabled layers ends up creating a Layer to manage the texture.
// A Layer can also be created without a texture, in which case it renders
// nothing and is simply used as a node in a hierarchy of layers.
//
// NOTE: unlike Views, each Layer does *not* own its children views. If you
// delete a Layer and it has children, the parent of each child layer is set to
// NULL, but the children are not deleted.
class COMPOSITOR_EXPORT Layer {
 public:
  enum TextureParam {
    LAYER_HAS_NO_TEXTURE = 0,
    LAYER_HAS_TEXTURE = 1
  };

  explicit Layer(Compositor* compositor);
  Layer(Compositor* compositor, TextureParam texture_param);
  ~Layer();

  LayerDelegate* delegate() { return delegate_; }
  void set_delegate(LayerDelegate* delegate) { delegate_ = delegate; }

  // Adds a new Layer to this Layer.
  void Add(Layer* child);

  // Removes a Layer from this Layer.
  void Remove(Layer* child);

  // Returns the child Layers.
  const std::vector<Layer*>& children() { return children_; }

  // The parent.
  const Layer* parent() const { return parent_; }
  Layer* parent() { return parent_; }

  // Returns true if this Layer contains |other| somewhere in its children.
  bool Contains(const Layer* other) const;

  // The transform, relative to the parent.
  void SetTransform(const ui::Transform& transform);
  const ui::Transform& transform() const { return transform_; }

  // The bounds, relative to the parent.
  void SetBounds(const gfx::Rect& bounds);
  const gfx::Rect& bounds() const { return bounds_; }

  // Sets |visible_|. The Layer is drawn by Draw() only when visible_ is true.
  bool visible() const { return visible_; }
  void set_visible(bool visible) { visible_ = visible; }

  // Converts a point from the coordinates of |source| to the coordinates of
  // |target|. Necessarily, |source| and |target| must inhabit the same Layer
  // tree.
  static void ConvertPointToLayer(const Layer* source,
                                  const Layer* target,
                                  gfx::Point* point);

  // See description in View for details
  void SetFillsBoundsOpaquely(bool fills_bounds_opaquely);
  bool fills_bounds_opaquely() const { return fills_bounds_opaquely_; }

  const gfx::Rect& hole_rect() const {  return hole_rect_; }

  // The compositor.
  const Compositor* compositor() const { return compositor_; }
  Compositor* compositor() { return compositor_; }

  const ui::Texture* texture() const { return texture_.get(); }

  // |texture| cannot be NULL, and this function cannot be called more than
  // once.
  // TODO(beng): This can be removed from the API when we are in a
  //             single-compositor world.
  void SetExternalTexture(ui::Texture* texture);

  // Resets the canvas of the texture.
  void SetCanvas(const SkCanvas& canvas, const gfx::Point& origin);

  // Adds |invalid_rect| to the Layer's pending invalid rect, and schedules a
  // repaint if the Layer has an associated LayerDelegate that can handle the
  // repaint.
  void SchedulePaint(const gfx::Rect& invalid_rect);

  // Draws the layer with hole if hole is non empty.
  // hole looks like:
  //
  //  layer____________________________
  //  |xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx|
  //  |xxxxxxxxxxxxx top xxxxxxxxxxxxxx|
  //  |________________________________|
  //  |xxxxx|                    |xxxxx|
  //  |xxxxx|      Hole Rect     |xxxxx|
  //  |left | (not composited)   |right|
  //  |_____|____________________|_____|
  //  |xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx|
  //  |xxxxxxxxxx bottom xxxxxxxxxxxxxx|
  //  |________________________________|
  //
  // Legend:
  //   composited area: x
  void Draw();

  // Draws a tree of Layers, by calling Draw() on each in the hierarchy starting
  // with the receiver.
  void DrawTree();

  // Sometimes the Layer is being updated by something other than SetCanvas
  // (e.g. the GPU process on TOUCH_UI).
  bool layer_updated_externally() const { return layer_updated_externally_; }

  float opacity() const { return opacity_; }
  void SetOpacity(float alpha);

 private:
  // TODO(vollick): Eventually, if a non-leaf node has an opacity of less than
  // 1.0, we'll render to a separate texture, and then apply the alpha.
  // Currently, we multiply our opacity by all our ancestor's opacities and
  // use the combined result, but this is only temporary.
  float GetCombinedOpacity() const;

  // calls Texture::Draw only if the region to be drawn is non empty
  void DrawRegion(const ui::TextureDrawParams& params,
                  const gfx::Rect& region_to_draw);

  // Called during the Draw() pass to freshen the Layer's contents from the
  // delegate.
  void UpdateLayerCanvas();

  // A hole in a layer is an area in the layer that does not get drawn
  // because this area is covered up with another layer which is known to be
  // opaque.
  // This method computes the dimension of the hole (if there is one)
  // based on whether one of its child nodes is always opaque.
  // Note: For simpicity's sake, currently a hole is only created if the child
  // view has no transfrom with respect to its parent.
  void RecomputeHole();

  bool ConvertPointForAncestor(const Layer* ancestor, gfx::Point* point) const;
  bool ConvertPointFromAncestor(const Layer* ancestor, gfx::Point* point) const;

  bool GetTransformRelativeTo(const Layer* ancestor,
                              Transform* transform) const;

  // The only externally updated layers are ones that get their pixels from
  // WebKit and WebKit does not produce valid alpha values. All other layers
  // should have valid alpha.
  bool has_valid_alpha_channel() const { return !layer_updated_externally_; }

  Compositor* compositor_;

  scoped_refptr<ui::Texture> texture_;

  Layer* parent_;

  std::vector<Layer*> children_;

  ui::Transform transform_;

  gfx::Rect bounds_;

  bool visible_;

  bool fills_bounds_opaquely_;

  gfx::Rect hole_rect_;

  gfx::Rect invalid_rect_;

  // If true the layer is always up to date.
  bool layer_updated_externally_;

  float opacity_;

  LayerDelegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(Layer);
};

}  // namespace ui

#endif  // UI_GFX_COMPOSITOR_LAYER_H_
