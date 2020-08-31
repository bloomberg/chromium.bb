// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_H_
#define CONTENT_PUBLIC_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_H_

#include <stddef.h>

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/synchronization/waitable_event.h"
#include "base/time/time.h"
#include "components/viz/common/frame_timing_details_map.h"
#include "components/viz/common/hit_test/hit_test_region_list.h"
#include "components/viz/common/resources/returned_resource.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "content/common/content_export.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

class SkCanvas;

namespace gfx {
class Point;
class ScrollOffset;
class Transform;
}  // namespace gfx

namespace viz {
class CompositorFrame;
class BeginFrameSource;
}

namespace content {
class SynchronousCompositorClient;
class WebContents;

// Interface for embedders that wish to direct compositing operations
// synchronously under their own control. Only meaningful when the
// kEnableSyncrhonousRendererCompositor flag is specified.
class CONTENT_EXPORT SynchronousCompositor {
 public:
  // Must be called once per WebContents instance. Will create the compositor
  // instance as needed, but only if |client| is non-nullptr.
  static void SetClientForWebContents(WebContents* contents,
                                      SynchronousCompositorClient* client);

  struct Frame {
    Frame();
    ~Frame();

    // Movable type.
    Frame(Frame&& rhs);
    Frame& operator=(Frame&& rhs);

    uint32_t layer_tree_frame_sink_id;
    std::unique_ptr<viz::CompositorFrame> frame;
    base::Optional<viz::HitTestRegionList> hit_test_region_list;

   private:
    DISALLOW_COPY_AND_ASSIGN(Frame);
  };

  class FrameFuture : public base::RefCountedThreadSafe<FrameFuture> {
   public:
    explicit FrameFuture(viz::LocalSurfaceId local_surface_id);
    void SetFrame(std::unique_ptr<Frame> frame);
    std::unique_ptr<Frame> GetFrame();
    const viz::LocalSurfaceId& local_surface_id() { return local_surface_id_; }

   private:
    friend class base::RefCountedThreadSafe<FrameFuture>;
    ~FrameFuture();

    base::WaitableEvent waitable_event_;
    std::unique_ptr<Frame> frame_;
    viz::LocalSurfaceId local_surface_id_;
#if DCHECK_IS_ON()
    bool waited_ = false;
#endif
  };

  // "On demand" hardware draw. Parameters are used by compositor for this draw.
  // |viewport_size| is the current size to improve results during resize.
  // |viewport_rect_for_tile_priority| and |transform_for_tile_priority| are
  // used to customize the tiling decisions of compositor.
  virtual scoped_refptr<FrameFuture> DemandDrawHwAsync(
      const gfx::Size& viewport_size,
      const gfx::Rect& viewport_rect_for_tile_priority,
      const gfx::Transform& transform_for_tile_priority) = 0;

  // For delegated rendering, return resources from parent compositor to this.
  // Note that all resources must be returned before ReleaseHwDraw.
  virtual void ReturnResources(
      uint32_t layer_tree_frame_sink_id,
      const std::vector<viz::ReturnedResource>& resources) = 0;

  virtual void DidPresentCompositorFrames(
      viz::FrameTimingDetailsMap timing_details,
      uint32_t frame_token) = 0;

  // "On demand" SW draw, into the supplied canvas (observing the transform
  // and clip set there-in).
  virtual bool DemandDrawSw(SkCanvas* canvas) = 0;

  // Set the memory limit policy of this compositor.
  virtual void SetMemoryPolicy(size_t bytes_limit) = 0;

  // Called during renderer swap. Should push any relevant up to
  // SynchronousCompositorClient.
  virtual void DidBecomeActive() = 0;

  // Should be called by the embedder after the embedder had modified the
  // scroll offset of the root layer. |root_offset| must be in physical pixel
  // scale if --use-zoom-for-dsf is enabled. Otherwise, it must be in DIP scale.
  virtual void DidChangeRootLayerScrollOffset(
      const gfx::ScrollOffset& root_offset) = 0;

  // Allows embedder to synchronously update the zoom level, ie page scale
  // factor, around the anchor point.
  virtual void SynchronouslyZoomBy(float zoom_delta,
                                   const gfx::Point& anchor) = 0;

  // Called by the embedder to notify that the OnComputeScroll step is happening
  // and if any input animation is active, it should tick now.
  virtual void OnComputeScroll(base::TimeTicks animation_time) = 0;

  // Sets BeginFrameSource to use
  virtual void SetBeginFrameSource(
      viz::BeginFrameSource* begin_frame_source) = 0;

  // Called when client invalidated because it was necessary for drawing sub
  // clients. Used with viz for webview only.
  virtual void DidInvalidate() = 0;

 protected:
  virtual ~SynchronousCompositor() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_ANDROID_SYNCHRONOUS_COMPOSITOR_H_
