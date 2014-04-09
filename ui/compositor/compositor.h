// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_COMPOSITOR_H_
#define UI_COMPOSITOR_COMPOSITOR_H_

#include <string>

#include "base/containers/hash_tables.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "cc/trees/layer_tree_host_client.h"
#include "cc/trees/layer_tree_host_single_thread_client.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/compositor_export.h"
#include "ui/compositor/compositor_observer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/size.h"
#include "ui/gfx/vector2d.h"

class SkBitmap;

namespace base {
class MessageLoopProxy;
class RunLoop;
}

namespace cc {
class ContextProvider;
class Layer;
class LayerTreeDebugState;
class LayerTreeHost;
class SharedBitmapManager;
}

namespace gfx {
class Rect;
class Size;
}

namespace gpu {
struct Mailbox;
}

namespace ui {

class Compositor;
class CompositorVSyncManager;
class Layer;
class Reflector;
class Texture;
struct LatencyInfo;

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

  // Creates an output surface for the given compositor. The factory may keep
  // per-compositor data (e.g. a shared context), that needs to be cleaned up
  // by calling RemoveCompositor when the compositor gets destroyed.
  virtual scoped_ptr<cc::OutputSurface> CreateOutputSurface(
      Compositor* compositor, bool software_fallback) = 0;

  // Creates a reflector that copies the content of the |mirrored_compositor|
  // onto |mirroing_layer|.
  virtual scoped_refptr<Reflector> CreateReflector(
      Compositor* mirrored_compositor,
      Layer* mirroring_layer) = 0;
  // Removes the reflector, which stops the mirroring.
  virtual void RemoveReflector(scoped_refptr<Reflector> reflector) = 0;

  // Returns a reference to the offscreen context provider used by the
  // compositor. This provider is bound and used on whichever thread the
  // compositor is rendering from.
  virtual scoped_refptr<cc::ContextProvider>
      OffscreenCompositorContextProvider() = 0;

  // Return a reference to a shared offscreen context provider usable from the
  // main thread. This may be the same as OffscreenCompositorContextProvider()
  // depending on the compositor's threading configuration. This provider will
  // be bound to the main thread.
  virtual scoped_refptr<cc::ContextProvider>
      SharedMainThreadContextProvider() = 0;

  // Destroys per-compositor data.
  virtual void RemoveCompositor(Compositor* compositor) = 0;

  // When true, the factory uses test contexts that do not do real GL
  // operations.
  virtual bool DoesCreateTestContexts() = 0;
};

// This class represents a lock on the compositor, that can be used to prevent
// commits to the compositor tree while we're waiting for an asynchronous
// event. The typical use case is when waiting for a renderer to produce a frame
// at the right size. The caller keeps a reference on this object, and drops the
// reference once it desires to release the lock.
// Note however that the lock is cancelled after a short timeout to ensure
// responsiveness of the UI, so the compositor tree should be kept in a
// "reasonable" state while the lock is held.
// Don't instantiate this class directly, use Compositor::GetCompositorLock.
class COMPOSITOR_EXPORT CompositorLock
    : public base::RefCounted<CompositorLock>,
      public base::SupportsWeakPtr<CompositorLock> {
 private:
  friend class base::RefCounted<CompositorLock>;
  friend class Compositor;

  explicit CompositorLock(Compositor* compositor);
  ~CompositorLock();

  void CancelLock();

  Compositor* compositor_;
  DISALLOW_COPY_AND_ASSIGN(CompositorLock);
};

// Compositor object to take care of GPU painting.
// A Browser compositor object is responsible for generating the final
// displayable form of pixels comprising a single widget's contents. It draws an
// appropriately transformed texture for each transformed view in the widget's
// view hierarchy.
class COMPOSITOR_EXPORT Compositor
    : NON_EXPORTED_BASE(public cc::LayerTreeHostClient),
      NON_EXPORTED_BASE(public cc::LayerTreeHostSingleThreadClient) {
 public:
  explicit Compositor(gfx::AcceleratedWidget widget);
  virtual ~Compositor();

  static void Initialize();
  static bool WasInitializedWithThread();
  static scoped_refptr<base::MessageLoopProxy> GetCompositorMessageLoop();
  static void Terminate();
  static void SetSharedBitmapManager(cc::SharedBitmapManager* manager);

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

  // Called when we need the compositor to preserve the alpha channel in the
  // output for situations when we want to render transparently atop something
  // else, e.g. Aero glass.
  void SetHostHasTransparentBackground(bool host_has_transparent_background);

  // The scale factor of the device that this compositor is
  // compositing layers on.
  float device_scale_factor() const { return device_scale_factor_; }

  // Draws the scene created by the layer tree and any visual effects.
  void Draw();

  // Where possible, draws are scissored to a damage region calculated from
  // changes to layer properties.  This bypasses that and indicates that
  // the whole frame needs to be drawn.
  void ScheduleFullRedraw();

  // Schedule redraw and append damage_rect to the damage region calculated
  // from changes to layer properties.
  void ScheduleRedrawRect(const gfx::Rect& damage_rect);

  void SetLatencyInfo(const LatencyInfo& latency_info);

  // Sets the compositor's device scale factor and size.
  void SetScaleAndSize(float scale, const gfx::Size& size_in_pixel);

  // Returns the size of the widget that is being drawn to in pixel coordinates.
  const gfx::Size& size() const { return size_; }

  // Sets the background color used for areas that aren't covered by
  // the |root_layer|.
  void SetBackgroundColor(SkColor color);

  // Returns the widget for this compositor.
  gfx::AcceleratedWidget widget() const { return widget_; }

  // Returns the vsync manager for this compositor.
  scoped_refptr<CompositorVSyncManager> vsync_manager() const;

  // Compositor does not own observers. It is the responsibility of the
  // observer to remove itself when it is done observing.
  void AddObserver(CompositorObserver* observer);
  void RemoveObserver(CompositorObserver* observer);
  bool HasObserver(CompositorObserver* observer);

  // Creates a compositor lock. Returns NULL if it is not possible to lock at
  // this time (i.e. we're waiting to complete a previous unlock).
  scoped_refptr<CompositorLock> GetCompositorLock();

  // Internal functions, called back by command-buffer contexts on swap buffer
  // events.

  // Signals swap has been posted.
  void OnSwapBuffersPosted();

  // Signals swap has completed.
  void OnSwapBuffersComplete();

  // Signals swap has aborted (e.g. lost context).
  void OnSwapBuffersAborted();

  // LayerTreeHostClient implementation.
  virtual void WillBeginMainFrame(int frame_id) OVERRIDE {}
  virtual void DidBeginMainFrame() OVERRIDE {}
  virtual void Animate(base::TimeTicks frame_begin_time) OVERRIDE {}
  virtual void Layout() OVERRIDE;
  virtual void ApplyScrollAndScale(const gfx::Vector2d& scroll_delta,
                                   float page_scale) OVERRIDE {}
  virtual scoped_ptr<cc::OutputSurface> CreateOutputSurface(bool fallback)
      OVERRIDE;
  virtual void DidInitializeOutputSurface(bool success) OVERRIDE {}
  virtual void WillCommit() OVERRIDE {}
  virtual void DidCommit() OVERRIDE;
  virtual void DidCommitAndDrawFrame() OVERRIDE;
  virtual void DidCompleteSwapBuffers() OVERRIDE;
  virtual scoped_refptr<cc::ContextProvider>
      OffscreenContextProvider() OVERRIDE;

  // cc::LayerTreeHostSingleThreadClient implementation.
  virtual void ScheduleComposite() OVERRIDE;
  virtual void ScheduleAnimation() OVERRIDE;
  virtual void DidPostSwapBuffers() OVERRIDE;
  virtual void DidAbortSwapBuffers() OVERRIDE;

  int last_started_frame() { return last_started_frame_; }
  int last_ended_frame() { return last_ended_frame_; }

  bool IsLocked() { return compositor_lock_ != NULL; }

  const cc::LayerTreeDebugState& GetLayerTreeDebugState() const;
  void SetLayerTreeDebugState(const cc::LayerTreeDebugState& debug_state);

 private:
  friend class base::RefCounted<Compositor>;
  friend class CompositorLock;

  // Called by CompositorLock.
  void UnlockCompositor();

  // Called to release any pending CompositorLock
  void CancelCompositorLock();

  // Notifies the compositor that compositing is complete.
  void NotifyEnd();

  gfx::Size size_;

  // The root of the Layer tree drawn by this compositor.
  Layer* root_layer_;

  ObserverList<CompositorObserver> observer_list_;

  gfx::AcceleratedWidget widget_;
  scoped_refptr<cc::Layer> root_web_layer_;
  scoped_ptr<cc::LayerTreeHost> host_;

  // The manager of vsync parameters for this compositor.
  scoped_refptr<CompositorVSyncManager> vsync_manager_;

  // The device scale factor of the monitor that this compositor is compositing
  // layers on.
  float device_scale_factor_;

  int last_started_frame_;
  int last_ended_frame_;

  bool next_draw_is_resize_;

  bool disable_schedule_composite_;

  CompositorLock* compositor_lock_;

  // Prevent more than one draw from being scheduled.
  bool defer_draw_scheduling_;

  // Used to prevent Draw()s while a composite is in progress.
  bool waiting_on_compositing_end_;
  bool draw_on_compositing_end_;
  enum SwapState { SWAP_NONE, SWAP_POSTED, SWAP_COMPLETED };
  SwapState swap_state_;

  base::WeakPtrFactory<Compositor> schedule_draw_factory_;

  DISALLOW_COPY_AND_ASSIGN(Compositor);
};

}  // namespace ui

#endif  // UI_COMPOSITOR_COMPOSITOR_H_
