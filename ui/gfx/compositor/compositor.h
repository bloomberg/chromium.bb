// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COMPOSITOR_COMPOSITOR_H_
#define UI_GFX_COMPOSITOR_COMPOSITOR_H_
#pragma once

#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebLayer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebLayerTreeView.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebLayerTreeViewClient.h"
#include "ui/gfx/compositor/compositor_export.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"


class SkBitmap;
class SkCanvas;
namespace gfx {
class GLContext;
class GLSurface;
class GLShareGroup;
class Point;
class Rect;
class ScopedMakeCurrent;
}

namespace ui {

class CompositorObserver;
class Layer;

class COMPOSITOR_EXPORT SharedResources {
 public:
  static SharedResources* GetInstance();

  // Creates an instance of ScopedMakeCurrent.
  // Note: Caller is responsible for managing lifetime of returned pointer.
  gfx::ScopedMakeCurrent* GetScopedMakeCurrent();

  void* GetDisplay();
  gfx::GLShareGroup* GetShareGroup();

 private:
  friend struct DefaultSingletonTraits<SharedResources>;

  SharedResources();
  ~SharedResources();

  bool Initialize();
  void Destroy();

  bool initialized_;

  scoped_refptr<gfx::GLContext> context_;
  scoped_refptr<gfx::GLSurface> surface_;

  DISALLOW_COPY_AND_ASSIGN(SharedResources);
};

// Texture provide an abstraction over the external texture that can be passed
// to a layer.
class COMPOSITOR_EXPORT Texture : public base::RefCounted<Texture> {
 public:
  Texture();
  virtual ~Texture();

  unsigned int texture_id() const { return texture_id_; }
  bool flipped() const { return flipped_; }
  gfx::Size size() const { return size_; }

 protected:
  unsigned int texture_id_;
  bool flipped_;
  gfx::Size size_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Texture);
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
class COMPOSITOR_EXPORT Compositor
    : public base::RefCounted<Compositor>,
      NON_EXPORTED_BASE(public WebKit::WebLayerTreeViewClient) {
 public:
  Compositor(CompositorDelegate* delegate,
             gfx::AcceleratedWidget widget,
             const gfx::Size& size);
  virtual ~Compositor();

  static void Initialize(bool useThread);
  static void Terminate();

  // Schedules a redraw of the layer tree associated with this compositor.
  void ScheduleDraw();

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
  bool ReadPixels(SkBitmap* bitmap, const gfx::Rect& bounds);

  // Notifies the compositor that the size of the widget that it is
  // drawing to has changed.
  void WidgetSizeChanged(const gfx::Size& size);

  // Returns the size of the widget that is being drawn to.
  const gfx::Size& size() { return size_; }

  // Compositor does not own observers. It is the responsibility of the
  // observer to remove itself when it is done observing.
  void AddObserver(CompositorObserver* observer);
  void RemoveObserver(CompositorObserver* observer);
  bool HasObserver(CompositorObserver* observer);

  // WebLayerTreeViewClient implementation.
  virtual void updateAnimations(double frameBeginTime);
  virtual void layout();
  virtual void applyScrollAndScale(const WebKit::WebSize& scrollDelta,
                                   float scaleFactor);
  virtual WebKit::WebGraphicsContext3D* createContext3D();
  virtual void didCompleteSwapBuffers();
  virtual void didRebindGraphicsContext(bool success);
  virtual void scheduleComposite();

 private:
  // When reading back pixel data we often get RGBA rather than BGRA pixels and
  // and the image often needs to be flipped vertically.
  static void SwizzleRGBAToBGRAAndFlip(unsigned char* pixels,
                                       const gfx::Size& image_size);

  // Notifies the compositor that compositing is complete.
  void NotifyEnd();

  CompositorDelegate* delegate_;
  gfx::Size size_;

  // The root of the Layer tree drawn by this compositor.
  Layer* root_layer_;

  ObserverList<CompositorObserver> observer_list_;

  gfx::AcceleratedWidget widget_;
  WebKit::WebLayer root_web_layer_;
  WebKit::WebLayerTreeView host_;

  friend class base::RefCounted<Compositor>;
};

}  // namespace ui

#endif  // UI_GFX_COMPOSITOR_COMPOSITOR_H_
