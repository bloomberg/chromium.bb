// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_COMPOSITOR_H_
#define UI_COMPOSITOR_COMPOSITOR_H_
#pragma once

#include "base/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayer.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayerTreeView.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayerTreeViewClient.h"
#include "ui/compositor/compositor_export.h"
#include "ui/gfx/gl/gl_share_group.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"
#include "ui/gfx/transform.h"

class SkBitmap;

namespace gfx {
class GLContext;
class GLSurface;
class GLShareGroup;
class Point;
class Rect;
}

namespace ui {

class Compositor;
class CompositorObserver;
class Layer;

// This class abstracts the creation of the 3D context for the compositor. It is
// a global object.
class COMPOSITOR_EXPORT ContextFactory {
 public:
  virtual ~ContextFactory() {}

  // Gets the global instance.
  static ContextFactory* GetInstance();

  // Sets the global instance. Caller keeps ownership.
  // If this function isn't called (for tests), a "default" factory will be
  // created on the first call of GetInstance.
  static void SetInstance(ContextFactory* instance);

  // Creates a context for given compositor. The factory may keep per-compositor
  // data (e.g. a shared context), that needs to be cleaned up by calling
  // RemoveCompositor when the compositor gets destroyed.
  virtual WebKit::WebGraphicsContext3D* CreateContext(
      Compositor* compositor) = 0;

  // Creates a context for given compositor used for offscreen rendering. See
  // the comments of CreateContext to know how per-compositor data is handled.
  virtual WebKit::WebGraphicsContext3D* CreateOffscreenContext(
      Compositor* compositor) = 0;

  // Destroys per-compositor data.
  virtual void RemoveCompositor(Compositor* compositor) = 0;
};

// The default factory that creates in-process contexts.
class COMPOSITOR_EXPORT DefaultContextFactory : public ContextFactory {
 public:
  DefaultContextFactory();
  virtual ~DefaultContextFactory();

  // ContextFactory implementation
  virtual WebKit::WebGraphicsContext3D* CreateContext(
      Compositor* compositor) OVERRIDE;
  virtual WebKit::WebGraphicsContext3D* CreateOffscreenContext(
      Compositor* compositor) OVERRIDE;
  virtual void RemoveCompositor(Compositor* compositor) OVERRIDE;

  bool Initialize();

  void set_share_group(gfx::GLShareGroup* share_group) {
    share_group_ = share_group;
  }

 private:
  WebKit::WebGraphicsContext3D* CreateContextCommon(
      Compositor* compositor,
      bool offscreen);

  scoped_refptr<gfx::GLShareGroup> share_group_;

  DISALLOW_COPY_AND_ASSIGN(DefaultContextFactory);
};

// Texture provide an abstraction over the external texture that can be passed
// to a layer.
class COMPOSITOR_EXPORT Texture : public base::RefCounted<Texture> {
 public:
  Texture(bool flipped, const gfx::Size& size);

  unsigned int texture_id() const { return texture_id_; }
  void set_texture_id(unsigned int id) { texture_id_ = id; }
  bool flipped() const { return flipped_; }
  gfx::Size size() const { return size_; }

 protected:
  virtual ~Texture();

 private:
  friend class base::RefCounted<Texture>;

  unsigned int texture_id_;
  bool flipped_;
  gfx::Size size_;  // in pixel

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
    : NON_EXPORTED_BASE(public WebKit::WebLayerTreeViewClient) {
 public:
  Compositor(CompositorDelegate* delegate,
             gfx::AcceleratedWidget widget);
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

  // The scale factor of the device that this compositor is
  // compositing layers on.
  float device_scale_factor() const { return device_scale_factor_; }

  // Draws the scene created by the layer tree and any visual effects. If
  // |force_clear| is true, this will cause the compositor to clear before
  // compositing.
  void Draw(bool force_clear);

  // Where possible, draws are scissored to a damage region calculated from
  // changes to layer properties.  This bypasses that and indicates that
  // the whole frame needs to be drawn.
  void ScheduleFullDraw();

  // Reads the region |bounds_in_pixel| of the contents of the last rendered
  // frame into the given bitmap.
  // Returns false if the pixels could not be read.
  bool ReadPixels(SkBitmap* bitmap, const gfx::Rect& bounds_in_pixel);

  // Sets the compositor's device scale factor and size.
  void SetScaleAndSize(float scale, const gfx::Size& size_in_pixel);

  // Returns the size of the widget that is being drawn to in pixel coordinates.
  const gfx::Size& size() const { return size_; }

  // Returns the widget for this compositor.
  gfx::AcceleratedWidget widget() const { return widget_; }

  // Compositor does not own observers. It is the responsibility of the
  // observer to remove itself when it is done observing.
  void AddObserver(CompositorObserver* observer);
  void RemoveObserver(CompositorObserver* observer);
  bool HasObserver(CompositorObserver* observer);

  // Returns whether a draw is pending, that is, if we're between the Draw call
  // and the OnCompositingEnded.
  bool DrawPending() const { return swap_posted_; }

  // Internal functions, called back by command-buffer contexts on swap buffer
  // events.

  // Signals swap has been posted.
  void OnSwapBuffersPosted();

  // Signals swap has completed.
  void OnSwapBuffersComplete();

  // Signals swap has aborted (e.g. lost context).
  void OnSwapBuffersAborted();

  // WebLayerTreeViewClient implementation.
  virtual void updateAnimations(double frameBeginTime);
  virtual void layout();
  virtual void applyScrollAndScale(const WebKit::WebSize& scrollDelta,
                                   float scaleFactor);
  virtual WebKit::WebGraphicsContext3D* createContext3D();
  virtual void didRebindGraphicsContext(bool success);
  virtual void didCommitAndDrawFrame();
  virtual void didCompleteSwapBuffers();
  virtual void scheduleComposite();

 private:
  friend class base::RefCounted<Compositor>;

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

  // This is set to true when the swap buffers has been posted and we're waiting
  // for completion.
  bool swap_posted_;

  // The device scale factor of the monitor that this compositor is compositing
  // layers on.
  float device_scale_factor_;
};

}  // namespace ui

#endif  // UI_COMPOSITOR_COMPOSITOR_H_
