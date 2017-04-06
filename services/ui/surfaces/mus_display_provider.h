// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_SURFACES_MUS_DISPLAY_PROVIDER_H_
#define SERVICES_UI_SURFACES_MUS_DISPLAY_PROVIDER_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "cc/surfaces/frame_sink_id.h"
#include "components/viz/frame_sinks/display_provider.h"
#include "gpu/command_buffer/client/gpu_memory_buffer_manager.h"
#include "gpu/ipc/common/surface_handle.h"
#include "gpu/ipc/in_process_command_buffer.h"

namespace gpu {
class ImageFactory;
}

namespace ui {

// Mus implementation of DisplayProvider. This will be created in mus-gpu.
class MusDisplayProvider : public NON_EXPORTED_BASE(viz::DisplayProvider) {
 public:
  MusDisplayProvider(
      scoped_refptr<gpu::InProcessCommandBuffer::Service> gpu_service,
      std::unique_ptr<gpu::GpuMemoryBufferManager> gpu_memory_buffer_manager,
      gpu::ImageFactory* image_factory);
  ~MusDisplayProvider() override;

  // display_compositor::DisplayProvider:
  std::unique_ptr<cc::Display> CreateDisplay(
      const cc::FrameSinkId& frame_sink_id,
      gpu::SurfaceHandle surface_handle,
      std::unique_ptr<cc::BeginFrameSource>* begin_frame_source) override;

 private:
  scoped_refptr<gpu::InProcessCommandBuffer::Service> gpu_service_;
  std::unique_ptr<gpu::GpuMemoryBufferManager> gpu_memory_buffer_manager_;
  gpu::ImageFactory* image_factory_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(MusDisplayProvider);
};

}  // namespace ui

#endif  //  SERVICES_UI_SURFACES_MUS_DISPLAY_PROVIDER_H_
