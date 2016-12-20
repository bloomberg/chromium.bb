// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_PUBLIC_CPP_GPU_GPU_H_
#define SERVICES_UI_PUBLIC_CPP_GPU_GPU_H_

#include <stdint.h>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "cc/output/context_provider.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "services/ui/public/cpp/gpu/client_gpu_memory_buffer_manager.h"
#include "services/ui/public/interfaces/gpu.mojom.h"

namespace service_manager {
class Connector;
class InterfaceProvider;
}

namespace ui {

class Gpu : public gpu::GpuChannelHostFactory,
            public gpu::GpuChannelEstablishFactory {
 public:
  ~Gpu() override;

  gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager() const {
    return gpu_memory_buffer_manager_.get();
  }

  // The Gpu has to be initialized in the main thread before establishing
  // the gpu channel. If no |task_runner| is provided, then a new thread is
  // created and used.
  static std::unique_ptr<Gpu> Create(
      service_manager::Connector* connector,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner = nullptr);
  static std::unique_ptr<Gpu> Create(
      service_manager::InterfaceProvider*,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner = nullptr);

  scoped_refptr<cc::ContextProvider> CreateContextProvider(
      scoped_refptr<gpu::GpuChannelHost> gpu_channel);

  // gpu::GpuChannelEstablishFactory:
  void EstablishGpuChannel(
      const gpu::GpuChannelEstablishedCallback& callback) override;
  scoped_refptr<gpu::GpuChannelHost> EstablishGpuChannelSync() override;
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;

 private:
  friend struct base::DefaultSingletonTraits<Gpu>;

  Gpu(service_manager::Connector* connector,
      service_manager::InterfaceProvider* provider,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  scoped_refptr<gpu::GpuChannelHost> GetGpuChannel();
  void OnEstablishedGpuChannel(int client_id,
                               mojo::ScopedMessagePipeHandle channel_handle,
                               const gpu::GPUInfo& gpu_info);

  // gpu::GpuChannelHostFactory overrides:
  bool IsMainThread() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetIOThreadTaskRunner() override;
  std::unique_ptr<base::SharedMemory> AllocateSharedMemory(
      size_t size) override;

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  service_manager::Connector* connector_;
  service_manager::InterfaceProvider* interface_provider_;
  base::WaitableEvent shutdown_event_;
  std::unique_ptr<base::Thread> io_thread_;
  std::unique_ptr<ClientGpuMemoryBufferManager> gpu_memory_buffer_manager_;

  ui::mojom::GpuPtr gpu_;
  scoped_refptr<gpu::GpuChannelHost> gpu_channel_;
  std::vector<gpu::GpuChannelEstablishedCallback> establish_callbacks_;

  DISALLOW_COPY_AND_ASSIGN(Gpu);
};

}  // namespace ui

#endif  // SERVICES_UI_PUBLIC_CPP_GPU_GPU_H_
