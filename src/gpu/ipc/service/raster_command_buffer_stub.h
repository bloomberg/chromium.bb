// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_IPC_SERVICE_RASTER_COMMAND_BUFFER_STUB_H_
#define GPU_IPC_SERVICE_RASTER_COMMAND_BUFFER_STUB_H_

#include "gpu/ipc/service/command_buffer_stub.h"

namespace gpu {

class GPU_IPC_SERVICE_EXPORT RasterCommandBufferStub
    : public CommandBufferStub {
 public:
  RasterCommandBufferStub(GpuChannel* channel,
                          const GPUCreateCommandBufferConfig& init_params,
                          CommandBufferId command_buffer_id,
                          SequenceId sequence_id,
                          int32_t stream_id,
                          int32_t route_id);
  ~RasterCommandBufferStub() override;

  // This must leave the GL context associated with the newly-created
  // CommandBufferStub current, so the GpuChannel can initialize
  // the gpu::Capabilities.
  gpu::ContextResult Initialize(
      CommandBufferStub* share_group,
      const GPUCreateCommandBufferConfig& init_params,
      base::UnsafeSharedMemoryRegion shared_state_shm) override;
  MemoryTracker* GetMemoryTracker() const override;

 private:
  bool HandleMessage(const IPC::Message& message) override;
  void OnSwapBuffers(uint64_t swap_id, uint32_t flags) override;
  void SetActiveURL(GURL url) override;

  std::unique_ptr<MemoryTracker> memory_tracker_;

  DISALLOW_COPY_AND_ASSIGN(RasterCommandBufferStub);
};

}  // namespace gpu

#endif  // GPU_IPC_SERVICE_RASTER_COMMAND_BUFFER_STUB_H_
