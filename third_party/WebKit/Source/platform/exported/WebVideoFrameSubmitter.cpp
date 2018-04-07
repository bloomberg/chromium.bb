// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/web_video_frame_submitter.h"

#include <memory>
#include <utility>

#include "third_party/blink/renderer/platform/graphics/video_frame_submitter.h"

namespace cc {
class LayerTreeSettings;
class VideoFrameProvider;
}  // namespace cc

namespace gpu {
class GpuMemoryBufferManager;
}

namespace viz {
class ContextProvider;
}

namespace blink {

std::unique_ptr<WebVideoFrameSubmitter> WebVideoFrameSubmitter::Create(
    WebContextProviderCallback context_provider_callback,
    viz::SharedBitmapManager* shared_bitmap_manager,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    const cc::LayerTreeSettings& settings) {
  return std::make_unique<VideoFrameSubmitter>(
      std::make_unique<VideoFrameResourceProvider>(
          std::move(context_provider_callback), shared_bitmap_manager,
          gpu_memory_buffer_manager, settings));
}

}  // namespace blink
