// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_COMPOSITOR_OBSERVER_H_
#define UI_COMPOSITOR_COMPOSITOR_OBSERVER_H_

#include "base/time/time.h"
#include "ui/compositor/compositor_export.h"

namespace viz {
class LocalSurfaceIdAllocation;
}

namespace ui {

class Compositor;

// A compositor observer is notified when compositing completes.
class COMPOSITOR_EXPORT CompositorObserver {
 public:
  virtual ~CompositorObserver() = default;

  // A commit proxies information from the main thread to the compositor
  // thread. It typically happens when some state changes that will require a
  // composite. In the multi-threaded case, many commits may happen between
  // two successive composites. In the single-threaded, a single commit
  // between two composites (just before the composite as part of the
  // composite cycle). If the compositor is locked, it will not send this
  // this signal.
  virtual void OnCompositingDidCommit(Compositor* compositor) {}

  // Called when compositing started: it has taken all the layer changes into
  // account and has issued the graphics commands.
  virtual void OnCompositingStarted(Compositor* compositor,
                                    base::TimeTicks start_time) {}

  // Called when compositing completes: the present to screen has completed.
  virtual void OnCompositingEnded(Compositor* compositor) {}

  // Called when a child of the compositor is resizing.
  virtual void OnCompositingChildResizing(Compositor* compositor) {}

  // Called at the top of the compositor's destructor, to give observers a
  // chance to remove themselves.
  virtual void OnCompositingShuttingDown(Compositor* compositor) {}

  // Called (asynchronously) when the compositor generates a new
  // LocalSurfaceIdAllocation. For example, if
  // LayerTreeHost::RequestNewLocalSurfaceId() is called, then this function
  // is called once the compositor generates the new LocalSurfaceIdAllocation.
  virtual void DidGenerateLocalSurfaceIdAllocation(
      Compositor* compositor,
      const viz::LocalSurfaceIdAllocation& allocation) {}
};

}  // namespace ui

#endif  // UI_COMPOSITOR_COMPOSITOR_OBSERVER_H_
