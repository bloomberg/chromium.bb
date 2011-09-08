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

class SkCanvas;
namespace gfx {
class Point;
class Rect;
}

namespace ui {

class CompositorObserver;

struct TextureDrawParams {
  TextureDrawParams() : transform(), blend(false), compositor_size() {}

  // The transform to be applied to the texture.
  ui::Transform transform;

  // If this is true, then the texture is blended with the pixels behind it.
  // Otherwise, the drawn pixels clobber the old pixels.
  bool blend;

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
  // Requests the owner to schedule a paint.
  virtual void ScheduleCompositorPaint() = 0;
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

  // Notifies the compositor that compositing is about to start.
  void NotifyStart();

  // Notifies the compositor that compositing is complete.
  void NotifyEnd();

  // Blurs the specific region in the compositor.
  virtual void Blur(const gfx::Rect& bounds) = 0;

  // Schedules a paint on the widget this Compositor was created for.
  void SchedulePaint() {
    delegate_->ScheduleCompositorPaint();
  }

  // Notifies the compositor that the size of the widget that it is
  // drawing to has changed.
  void WidgetSizeChanged(const gfx::Size& size) {
    size_ = size;
    OnWidgetSizeChanged();
  }

  // Returns the size of the widget that is being drawn to.
  const gfx::Size& size() { return size_; }

  // Layers do not own observers. It is the responsibility of the observer to
  // remove itself when it is done observing.
  void AddObserver(CompositorObserver* observer);
  void RemoveObserver(CompositorObserver* observer);

 protected:
  Compositor(CompositorDelegate* delegate, const gfx::Size& size)
      : delegate_(delegate),
        size_(size) {}
  virtual ~Compositor() {}

  // Notifies the compositor that compositing is about to start.
  virtual void OnNotifyStart() = 0;

  // Notifies the compositor that compositing is complete.
  virtual void OnNotifyEnd() = 0;

  virtual void OnWidgetSizeChanged() = 0;

  CompositorDelegate* delegate() { return delegate_; }

 private:
  CompositorDelegate* delegate_;
  gfx::Size size_;

  ObserverList<CompositorObserver> observer_list_;

  friend class base::RefCounted<Compositor>;
};

}  // namespace ui

#endif  // UI_GFX_COMPOSITOR_COMPOSITOR_H_
