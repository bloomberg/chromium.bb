// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COMPOSITOR_COMPOSITOR_H_
#define UI_GFX_COMPOSITOR_COMPOSITOR_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "ui/gfx/compositor/compositor_export.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"

class SkBitmap;
class SkCanvas;
namespace gfx {
class Point;
class Rect;
class ScopedMakeCurrent;
}

namespace ui {

class CompositorObserver;
class Layer;

class SharedResources {
 public:
  virtual ~SharedResources() {}

  // Creates an instance of ScopedMakeCurrent.
  // Note: Caller is responsible for managing lifetime of returned pointer.
  virtual gfx::ScopedMakeCurrent* GetScopedMakeCurrent() = 0;

  virtual void* GetDisplay() = 0;
};

struct TextureDrawParams {
  TextureDrawParams();

  // The transform to be applied to the texture.
  ui::Transform transform;

  // If this is true, then the texture is blended with the pixels behind it.
  // Otherwise, the drawn pixels clobber the old pixels.
  bool blend;

  // If this is false, the alpha values for this texture should not be trusted.
  bool has_valid_alpha_channel;

  // This multiplier is applied to all pixels before blending. The intent is to
  // allow alpha to be animated (for effects such as cross fades).
  float opacity;

  // Sometimes the texture is vertically flipped. In this case we have to
  // draw the texture differently.
  bool vertically_flipped;

  // The size of the surface that the texture is drawn to.
  gfx::Size compositor_size;

  // Copy and assignment are allowed.
};

// Textures are created by a Compositor for managing an accelerated view.
// Any time a View with a texture needs to redraw itself it invokes SetCanvas().
// When the view is ready to be drawn Draw() is invoked.
//
// Texture is really a proxy to the gpu. Texture does not itself keep a copy of
// the bitmap.
//
// Views own the Texture.
class COMPOSITOR_EXPORT Texture : public base::RefCounted<Texture> {
 public:
  // Sets the canvas of this texture. The origin is at |origin|.
  // |overall_size| gives the total size of texture.
  virtual void SetCanvas(const SkCanvas& canvas,
                         const gfx::Point& origin,
                         const gfx::Size& overall_size) = 0;

  // Draws the portion of the texture contained within clip_bounds
  virtual void Draw(const ui::TextureDrawParams& params,
                    const gfx::Rect& clip_bounds_in_texture) = 0;

 protected:
  virtual ~Texture() {}

 private:
  friend class base::RefCounted<Texture>;
};

// An interface to allow the compositor to communicate with its owner.
class COMPOSITOR_EXPORT CompositorDelegate {
 public:
  // Requests the owner to schedule a redraw of the layer tree.
  virtual void ScheduleDraw() = 0;

 protected:
  virtual ~CompositorDelegate() {}
};

// Compositor object to take care of GPU painting.
// A Browser compositor object is responsible for generating the final
// displayable form of pixels comprising a single widget's contents. It draws an
// appropriately transformed texture for each transformed view in the widget's
// view hierarchy.
class COMPOSITOR_EXPORT Compositor : public base::RefCounted<Compositor> {
 public:
  // Create a compositor from the provided handle.
  static Compositor* Create(CompositorDelegate* delegate,
                            gfx::AcceleratedWidget widget,
                            const gfx::Size& size);

  // Creates a new texture. The caller owns the returned object.
  virtual Texture* CreateTexture() = 0;

  // Blurs the specific region in the compositor.
  virtual void Blur(const gfx::Rect& bounds) = 0;

  // Schedules a redraw of the layer tree associated with this compositor.
  virtual void ScheduleDraw();

  // Sets the root of the layer tree drawn by this Compositor. The root layer
  // must have no parent. The compositor's root layer is reset if the root layer
  // is destroyed. NULL can be passed to reset the root layer, in which case the
  // compositor will stop drawing anything.
  // The Compositor does not own the root layer.
  const Layer* root_layer() const { return root_layer_; }
  Layer* root_layer() { return root_layer_; }
  void SetRootLayer(Layer* root_layer);

  // Draws the scene created by the layer tree and any visual effects. If
  // |force_clear| is true, this will cause the compositor to clear before
  // compositing.
  void Draw(bool force_clear);

  // Reads the region |bounds| of the contents of the last rendered frame
  // into the given bitmap.
  // Returns false if the pixels could not be read.
  virtual bool ReadPixels(SkBitmap* bitmap, const gfx::Rect& bounds) = 0;

  // Notifies the compositor that the size of the widget that it is
  // drawing to has changed.
  void WidgetSizeChanged(const gfx::Size& size) {
    if (size.IsEmpty())
      return;
    size_ = size;
    OnWidgetSizeChanged();
  }

  // Returns the size of the widget that is being drawn to.
  const gfx::Size& size() { return size_; }

  // Compositor does not own observers. It is the responsibility of the
  // observer to remove itself when it is done observing.
  void AddObserver(CompositorObserver* observer);
  void RemoveObserver(CompositorObserver* observer);
  bool HasObserver(CompositorObserver* observer);

  static void set_compositor_factory_for_testing(
      ui::Compositor*(*factory)(ui::CompositorDelegate* owner)) {
    compositor_factory_ = factory;
  }

  static ui::Compositor* (*compositor_factory())(
      ui::CompositorDelegate* owner) {
    return compositor_factory_;
  }

 protected:
  Compositor(CompositorDelegate* delegate, const gfx::Size& size);
  virtual ~Compositor();

  // Notifies the compositor that compositing is about to start.
  virtual void OnNotifyStart(bool clear) = 0;

  // Notifies the compositor that compositing is complete.
  virtual void OnNotifyEnd() = 0;

  virtual void OnWidgetSizeChanged() = 0;
  virtual void OnRootLayerChanged();
  virtual void DrawTree();

  CompositorDelegate* delegate() { return delegate_; }

  // When reading back pixel data we often get RGBA rather than BGRA pixels and
  // and the image often needs to be flipped vertically.
  static void SwizzleRGBAToBGRAAndFlip(unsigned char* pixels,
                                       const gfx::Size& image_size);

 private:
  // Notifies the compositor that compositing is about to start. See Draw() for
  // notes about |force_clear|.
  void NotifyStart(bool force_clear);

  // Notifies the compositor that compositing is complete.
  void NotifyEnd();

  CompositorDelegate* delegate_;
  gfx::Size size_;

  // The root of the Layer tree drawn by this compositor.
  Layer* root_layer_;

  ObserverList<CompositorObserver> observer_list_;

  // Factory used to create Compositors. Settable by tests.
  // The delegate can be NULL if you don't wish to catch the ScheduleDraw()
  // calls to it.
  static ui::Compositor*(*compositor_factory_)(
      ui::CompositorDelegate* delegate);

  friend class base::RefCounted<Compositor>;
};

}  // namespace ui

#endif  // UI_GFX_COMPOSITOR_COMPOSITOR_H_
