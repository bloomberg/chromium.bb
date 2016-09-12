// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/gpu/gpu_main.h"

#include "base/command_line.h"
#include "gpu/ipc/common/gpu_memory_buffer_support.h"
#include "gpu/ipc/service/gpu_memory_buffer_factory.h"
#include "gpu/ipc/service/gpu_watchdog_thread.h"
#include "services/ui/gpu/gpu_service_internal.h"

namespace ui {

GpuMain::GpuMain() {
  gpu_init_.set_sandbox_helper(this);
  bool success = gpu_init_.InitializeAndStartSandbox(
      *base::CommandLine::ForCurrentProcess());
  if (success) {
    if (gpu::GetNativeGpuMemoryBufferType() != gfx::EMPTY_BUFFER) {
      gpu_memory_buffer_factory_ =
          gpu::GpuMemoryBufferFactory::CreateNativeType();
    }
    gpu_service_internal_.reset(new GpuServiceInternal(
        gpu_init_.gpu_info(), gpu_init_.watchdog_thread(),
        gpu_memory_buffer_factory_.get()));
  }
}

GpuMain::~GpuMain() {
  gpu_init_.watchdog_thread()->Stop();
}

void GpuMain::Add(mojom::GpuServiceInternalRequest request) {
  if (gpu_service_internal_)
    gpu_service_internal_->Add(std::move(request));
}

void GpuMain::PreSandboxStartup() {
  // TODO(sad): https://crbug.com/645602
}

bool GpuMain::EnsureSandboxInitialized() {
  // TODO(sad): https://crbug.com/645602
  return true;
}

}  // namespace ui
