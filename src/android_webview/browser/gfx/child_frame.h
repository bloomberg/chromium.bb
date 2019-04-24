// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_GFX_CHILD_FRAME_H_
#define ANDROID_WEBVIEW_BROWSER_GFX_CHILD_FRAME_H_

#include <memory>

#include "android_webview/browser/gfx/compositor_id.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/transform.h"

namespace viz {
class CompositorFrame;
}

namespace android_webview {

class ChildFrame {
 public:
  ChildFrame(
      scoped_refptr<content::SynchronousCompositor::FrameFuture> frame_future,
      const CompositorID& compositor_id,
      const gfx::Size& viewport_size_for_tile_priority,
      const gfx::Transform& transform_for_tile_priority,
      bool offscreen_pre_raster);
  ~ChildFrame();

  // Helper to move frame from |frame_future| to |frame|.
  void WaitOnFutureIfNeeded();

  // The frame is either in |frame_future| or |frame|. It's illegal if both
  // are non-null.
  scoped_refptr<content::SynchronousCompositor::FrameFuture> frame_future;
  uint32_t layer_tree_frame_sink_id = 0u;
  std::unique_ptr<viz::CompositorFrame> frame;
  // The id of the compositor this |frame| comes from.
  const CompositorID compositor_id;
  const gfx::Size viewport_size_for_tile_priority;
  const gfx::Transform transform_for_tile_priority;
  const bool offscreen_pre_raster;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChildFrame);
};

using ChildFrameQueue = base::circular_deque<std::unique_ptr<ChildFrame>>;

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_GFX_CHILD_FRAME_H_
