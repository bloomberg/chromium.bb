// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COMPOSITOR_LAYER_H_
#define UI_GFX_COMPOSITOR_LAYER_H_
#pragma once

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebContentLayerClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebLayer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebLayerClient.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/compositor/layer_animation_delegate.h"
#include "ui/gfx/compositor/layer_delegate.h"

class SkCanvas;

namespace ui {

class Compositor;
class LayerAnimator;
class Texture;

// Layer manages a texture, transform and a set of child Layers. Any View that
// has enabled layers ends up creating a Layer to manage the texture.
// A Layer can also be created without a texture, in which case it renders
// nothing and is simply used as a node in a hierarchy of layers.
//
// NOTE: unlike Views, each Layer does *not* own its children views. If you
// delete a Layer and it has children, the parent of each child layer is set to
// NULL, but the children are not deleted.
class COMPOSITOR_EXPORT Layer :
    public LayerAnimationDelegate,
    NON_EXPORTED_BASE(public WebKit::WebLayerClient),
    NON_EXPORTED_BASE(public WebKit::WebContentLayerClient) {
 public:
  enum LayerType {
    LAYER_HAS_NO_TEXTURE = 0,
    LAYER_HAS_TEXTURE = 1
  };

  Layer();
  explicit Layer(LayerType type);
  virtual ~Layer();

  // Retrieves the Layer's compositor. The Layer will walk up its parent chain
  // to locate it. Returns NULL if the Layer is not attached to a compositor.
  Compositor* GetCompositor();

  // Called by the compositor when the Layer is set as its root Layer. This can
  // only ever be called on the root layer.
  void SetCompositor(Compositor* compositor);

  LayerDelegate* delegate() { return delegate_; }
  void set_delegate(LayerDelegate* delegate) { delegate_ = delegate; }

  // Adds a new Layer to this Layer.
  void Add(Layer* child);

  // Removes a Layer from this Layer.
  void Remove(Layer* child);

  // Moves a child to the end of the child list.
  void MoveToFront(Layer* child);

  // Moves |child| to be above |other|. Does nothing if |other| is already above
  // |child|.
  void MoveAbove(Layer* child, Layer* other);

  // Returns the child Layers.
  const std::vector<Layer*>& children() const { return children_; }

  // The parent.
  const Layer* parent() const { return parent_; }
  Layer* parent() { return parent_; }

  // Returns true if this Layer contains |other| somewhere in its children.
  bool Contains(const Layer* other) const;

  // The layer's animator is responsible for causing automatic animations when
  // properties are set. It also manages a queue of pending animations and
  // handles blending of animations. The layer takes ownership of the animator.
  void SetAnimator(LayerAnimator* animator);

  // Returns the layer's animator. Creates a default animator of one has not
  // been set. Will not return NULL.
  LayerAnimator* GetAnimator();

  // The transform, relative to the parent.
  void SetTransform(const Transform& transform);
  const Transform& transform() const { return transform_; }

  // Return the target transform if animator is running, or the current
  // transform otherwise.
  Transform GetTargetTransform() const;

  // The bounds, relative to the parent.
  void SetBounds(const gfx::Rect& bounds);
  const gfx::Rect& bounds() const { return bounds_; }

  // Return the target bounds if animator is running, or the current bounds
  // otherwise.
  gfx::Rect GetTargetBounds() const;

  // The opacity of the layer. The opacity is applied to each pixel of the
  // texture (resulting alpha = opacity * alpha).
  float opacity() const { return opacity_; }
  void SetOpacity(float opacity);

  // Return the target opacity if animator is running, or the current opacity
  // otherwise.
  float GetTargetOpacity() const;

  // Sets the visibility of the Layer. A Layer may be visible but not
  // drawn. This happens if any ancestor of a Layer is not visible.
  void SetVisible(bool visible);
  bool visible() const { return visible_; }

  // Returns true if this Layer is drawn. A Layer is drawn only if all ancestors
  // are visible.
  bool IsDrawn() const;

  // Returns true if this layer can have a texture (has_texture_ is true)
  // and is not completely obscured by a child.
  bool ShouldDraw() const;

  // Converts a point from the coordinates of |source| to the coordinates of
  // |target|. Necessarily, |source| and |target| must inhabit the same Layer
  // tree.
  static void ConvertPointToLayer(const Layer* source,
                                  const Layer* target,
                                  gfx::Point* point);

  // See description in View for details
  void SetFillsBoundsOpaquely(bool fills_bounds_opaquely);
  bool fills_bounds_opaquely() const { return fills_bounds_opaquely_; }

  // Returns the invalid rectangle. That is, the region of the layer that needs
  // to be repainted. This is exposed for testing and isn't generally useful.
  const gfx::Rect& invalid_rect() const { return invalid_rect_; }

  const gfx::Rect& hole_rect() const {  return hole_rect_; }

  const ui::Texture* texture() const { return texture_.get(); }

  // |texture| cannot be NULL, and this function cannot be called more than
  // once.
  // TODO(beng): This can be removed from the API when we are in a
  //             single-compositor world.
  void SetExternalTexture(ui::Texture* texture);

  // Resets the canvas of the texture.
  void SetCanvas(const SkCanvas& canvas, const gfx::Point& origin);

  // Adds |invalid_rect| to the Layer's pending invalid rect and calls
  // ScheduleDraw().
  void SchedulePaint(const gfx::Rect& invalid_rect);

  // Schedules a redraw of the layer tree at the compositor.
  void ScheduleDraw();

  // Does drawing for the layer.
  void Draw();

  // Draws a tree of Layers, by calling Draw() on each in the hierarchy starting
  // with the receiver.
  void DrawTree();

  // Sometimes the Layer is being updated by something other than SetCanvas
  // (e.g. the GPU process on TOUCH_UI).
  bool layer_updated_externally() const { return layer_updated_externally_; }

  // WebLayerClient
  virtual void notifyNeedsComposite();

  // WebContentLayerClient
  virtual void paintContents(WebKit::WebCanvas*, const WebKit::WebRect& clip);

#if defined(USE_WEBKIT_COMPOSITOR)
  WebKit::WebLayer web_layer() { return web_layer_; }
#endif

 private:
  struct LayerProperties {
   public:
    ui::Layer* layer;
    ui::Transform transform_relative_to_root;
  };

  // TODO(vollick): Eventually, if a non-leaf node has an opacity of less than
  // 1.0, we'll render to a separate texture, and then apply the alpha.
  // Currently, we multiply our opacity by all our ancestor's opacities and
  // use the combined result, but this is only temporary.
  float GetCombinedOpacity() const;

  // Called during the Draw() pass to freshen the Layer's contents from the
  // delegate.
  void UpdateLayerCanvas();

  // Called to indicate that a layer's properties have changed and that the
  // holes for the layers must be recomputed.
  void SetNeedsToRecomputeHole();

  // Resets |hole_rect_| to the empty rect for all layers below and
  // including this one.
  void ClearHoleRects();

  // Does a preorder traversal of layers starting with this layer. Omits layers
  // which cannot punch a hole in another layer such as non visible layers
  // and layers which don't fill their bounds opaquely.
  void GetLayerProperties(const ui::Transform& current_transform,
                          std::vector<LayerProperties>* traverasal);

  // A hole in a layer is an area in the layer that does not get drawn
  // because this area is covered up with another layer which is known to be
  // opaque.
  // This method computes the dimension of the hole (if there is one)
  // based on whether one of its child nodes is always opaque.
  // Note: This method should only be called from the root.
  void RecomputeHole();

  void set_hole_rect(const gfx::Rect& hole_rect) {
    hole_rect_ = hole_rect;
  }

  // Determines the regions that don't intersect |rect| and places the
  // result in |sides|.
  //
  //  rect_____________________________
  //  |xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx|
  //  |xxxxxxxxxxxxx top xxxxxxxxxxxxxx|
  //  |________________________________|
  //  |xxxxx|                    |xxxxx|
  //  |xxxxx|region_to_punch_out |xxxxx|
  //  |left |                    |right|
  //  |_____|____________________|_____|
  //  |xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx|
  //  |xxxxxxxxxx bottom xxxxxxxxxxxxxx|
  //  |________________________________|
  static void PunchHole(const gfx::Rect& rect,
                        const gfx::Rect& region_to_punch_out,
                        std::vector<gfx::Rect>* sides);

  // Drops texture just for this layer.
  void DropTexture();

  // Drop all textures for layers below and including this one. Called when
  // the layer is removed from a hierarchy. Textures will be re-generated if
  // the layer is subsequently re-attached and needs to be drawn.
  void DropTextures();

  bool ConvertPointForAncestor(const Layer* ancestor, gfx::Point* point) const;
  bool ConvertPointFromAncestor(const Layer* ancestor, gfx::Point* point) const;

  bool GetTransformRelativeTo(const Layer* ancestor,
                              Transform* transform) const;

  // The only externally updated layers are ones that get their pixels from
  // WebKit and WebKit does not produce valid alpha values. All other layers
  // should have valid alpha.
  bool has_valid_alpha_channel() const { return !layer_updated_externally_; }

  // Following are invoked from the animation or if no animation exists to
  // update the values immediately.
  void SetBoundsImmediately(const gfx::Rect& bounds);
  void SetTransformImmediately(const ui::Transform& transform);
  void SetOpacityImmediately(float opacity);

  // Implementation of LayerAnimatorDelegate
  virtual void SetBoundsFromAnimation(const gfx::Rect& bounds) OVERRIDE;
  virtual void SetTransformFromAnimation(const Transform& transform) OVERRIDE;
  virtual void SetOpacityFromAnimation(float opacity) OVERRIDE;
  virtual void ScheduleDrawForAnimation() OVERRIDE;
  virtual const gfx::Rect& GetBoundsForAnimation() const OVERRIDE;
  virtual const Transform& GetTransformForAnimation() const OVERRIDE;
  virtual float GetOpacityForAnimation() const OVERRIDE;

#if defined(USE_WEBKIT_COMPOSITOR)
  void CreateWebLayer();
  void RecomputeTransform();
  void RecomputeDrawsContentAndUVRect();
#endif

  const LayerType type_;

  Compositor* compositor_;

  scoped_refptr<ui::Texture> texture_;

  Layer* parent_;

  std::vector<Layer*> children_;

  ui::Transform transform_;

  gfx::Rect bounds_;

  // Visibility of this layer. See SetVisible/IsDrawn for more details.
  bool visible_;

  bool fills_bounds_opaquely_;

  gfx::Rect hole_rect_;

  bool recompute_hole_;

  gfx::Rect invalid_rect_;

  // If true the layer is always up to date.
  bool layer_updated_externally_;

  float opacity_;

  LayerDelegate* delegate_;

  scoped_ptr<LayerAnimator> animator_;

#if defined(USE_WEBKIT_COMPOSITOR)
  WebKit::WebLayer web_layer_;
  bool web_layer_is_accelerated_;
#endif

  DISALLOW_COPY_AND_ASSIGN(Layer);
};

}  // namespace ui

#endif  // UI_GFX_COMPOSITOR_LAYER_H_
