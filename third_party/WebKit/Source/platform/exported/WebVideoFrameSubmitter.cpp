// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebVideoFrameSubmitter.h"

#include <memory>
#include "third_party/WebKit/Source/platform/graphics/VideoFrameSubmitter.h"

namespace cc {
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
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager) {
  return std::make_unique<VideoFrameSubmitter>(
      base::MakeUnique<VideoFrameResourceProvider>(
          std::move(context_provider_callback), shared_bitmap_manager,
          gpu_memory_buffer_manager));
}

}  // namespace blink
