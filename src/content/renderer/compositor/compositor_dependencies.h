// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_COMPOSITOR_COMPOSITOR_DEPENDENCIES_H_
#define CONTENT_RENDERER_COMPOSITOR_COMPOSITOR_DEPENDENCIES_H_

#include <memory>
#include <vector>

#include "base/memory/ref_counted.h"
#include "components/viz/common/display/renderer_settings.h"
#include "content/common/content_export.h"
#include "content/common/render_frame_metadata.mojom.h"

class GURL;

namespace base {
class SingleThreadTaskRunner;
}

namespace cc {
class LayerTreeFrameSink;
class TaskGraphRunner;
class UkmRecorderFactory;
}  // namespace cc

namespace blink {
namespace scheduler {
class WebThreadScheduler;
}
}  // namespace blink

namespace content {
class FrameSwapMessageQueue;

class CONTENT_EXPORT CompositorDependencies {
 public:
  virtual bool IsGpuRasterizationForced() = 0;
  virtual int GetGpuRasterizationMSAASampleCount() = 0;
  virtual bool IsLcdTextEnabled() = 0;
  virtual bool IsZeroCopyEnabled() = 0;
  virtual bool IsPartialRasterEnabled() = 0;
  virtual bool IsGpuMemoryBufferCompositorResourcesEnabled() = 0;
  virtual bool IsElasticOverscrollEnabled() = 0;
  virtual bool IsUseZoomForDSFEnabled() = 0;
  virtual scoped_refptr<base::SingleThreadTaskRunner>
  GetCompositorMainThreadTaskRunner() = 0;
  // Returns null if the compositor is in single-threaded mode (ie. there is no
  // compositor thread).
  virtual scoped_refptr<base::SingleThreadTaskRunner>
  GetCompositorImplThreadTaskRunner() = 0;
  virtual blink::scheduler::WebThreadScheduler* GetWebMainThreadScheduler() = 0;
  virtual cc::TaskGraphRunner* GetTaskGraphRunner() = 0;
  virtual bool IsScrollAnimatorEnabled() = 0;
  virtual std::unique_ptr<cc::UkmRecorderFactory>
  CreateUkmRecorderFactory() = 0;

  using LayerTreeFrameSinkCallback =
      base::OnceCallback<void(std::unique_ptr<cc::LayerTreeFrameSink>)>;
  virtual void RequestNewLayerTreeFrameSink(
      int widget_routing_id,
      scoped_refptr<FrameSwapMessageQueue> frame_swap_message_queue,
      const GURL& url,
      LayerTreeFrameSinkCallback callback,
      mojom::RenderFrameMetadataObserverClientRequest
          render_frame_metadata_observer_client_request,
      mojom::RenderFrameMetadataObserverPtr render_frame_metadata_observer_ptr,
      const char* client_name) = 0;

#ifdef OS_ANDROID
  virtual bool UsingSynchronousCompositing() = 0;
#endif

  virtual ~CompositorDependencies() {}
};

}  // namespace content

#endif  // CONTENT_RENDERER_COMPOSITOR_COMPOSITOR_DEPENDENCIES_H_
