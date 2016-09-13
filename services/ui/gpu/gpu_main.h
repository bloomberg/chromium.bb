// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_GPU_GPU_MAIN_H_
#define SERVICES_UI_GPU_GPU_MAIN_H_

#include "gpu/ipc/service/gpu_init.h"
#include "services/ui/gpu/interfaces/gpu_service_internal.mojom.h"

namespace gpu {
class GpuMemoryBufferFactory;
}

namespace ui {

class GpuServiceInternal;

class GpuMain : public gpu::GpuSandboxHelper {
 public:
  GpuMain();
  ~GpuMain() override;

  void Add(mojom::GpuServiceInternalRequest request);

  GpuServiceInternal* gpu_service() { return gpu_service_internal_.get(); }

 private:
  // gpu::GpuSandboxHelper:
  void PreSandboxStartup() override;
  bool EnsureSandboxInitialized() override;

  std::unique_ptr<GpuServiceInternal> gpu_service_internal_;
  gpu::GpuInit gpu_init_;
  std::unique_ptr<gpu::GpuMemoryBufferFactory> gpu_memory_buffer_factory_;

  DISALLOW_COPY_AND_ASSIGN(GpuMain);
};

}  // namespace ui

#endif  // SERVICES_UI_GPU_GPU_MAIN_H_
