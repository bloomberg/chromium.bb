// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WEB_TEST_DEPENDENCIES_H_
#define CONTENT_RENDERER_WEB_TEST_DEPENDENCIES_H_

#include <stdint.h>
#include <memory>

#include "base/memory/ref_counted.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "content/common/content_export.h"

namespace cc {
class CopyOutputRequest;
class LayerTreeFrameSink;
class SwapPromise;
}  // namespace cc

namespace gpu {
class GpuChannelHost;
class GpuMemoryBufferManager;
}  // namespace gpu

namespace viz {
class ContextProvider;
class RasterContextProvider;
}  // namespace viz

namespace content {
class CompositorDependencies;

// This class allows injection of WebTest-specific behaviour to the
// RenderThreadImpl.
class CONTENT_EXPORT WebTestDependencies {
 public:
  virtual ~WebTestDependencies();

  // Returns true if the web tests should use the display compositor pixel
  // dumps.
  virtual bool UseDisplayCompositorPixelDump() const = 0;

  virtual std::unique_ptr<cc::LayerTreeFrameSink> CreateLayerTreeFrameSink(
      int32_t routing_id,
      scoped_refptr<gpu::GpuChannelHost> gpu_channel,
      scoped_refptr<viz::ContextProvider> compositor_context_provider,
      scoped_refptr<viz::RasterContextProvider> worker_context_provider,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      CompositorDependencies* deps) = 0;

  // Returns a SwapPromise which should be queued for the next compositor frame.
  virtual std::unique_ptr<cc::SwapPromise> RequestCopyOfOutput(
      int32_t routing_id,
      std::unique_ptr<viz::CopyOutputRequest> request) = 0;
};

}  // namespace content

#endif  // CONTENT_RENDERER_WEB_TEST_DEPENDENCIES_H_
