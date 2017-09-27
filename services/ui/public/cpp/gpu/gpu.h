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
#include "components/viz/common/gpu/context_provider.h"
#include "gpu/ipc/client/gpu_channel_host.h"
#include "services/ui/public/cpp/gpu/client_gpu_memory_buffer_manager.h"
#include "services/ui/public/interfaces/gpu.mojom.h"

namespace service_manager {
class Connector;
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
      const std::string& service_name,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner = nullptr);

  scoped_refptr<viz::ContextProvider> CreateContextProvider(
      scoped_refptr<gpu::GpuChannelHost> gpu_channel);

  void CreateJpegDecodeAccelerator(
      media::mojom::GpuJpegDecodeAcceleratorRequest jda_request);
  void CreateVideoEncodeAcceleratorProvider(
      media::mojom::VideoEncodeAcceleratorProviderRequest vea_provider_request);

  // gpu::GpuChannelEstablishFactory:
  void EstablishGpuChannel(
      const gpu::GpuChannelEstablishedCallback& callback) override;
  scoped_refptr<gpu::GpuChannelHost> EstablishGpuChannelSync() override;
  gpu::GpuMemoryBufferManager* GetGpuMemoryBufferManager() override;

 private:
  friend class GpuTest;

  using GpuPtrFactory = base::RepeatingCallback<mojom::GpuPtr(void)>;
  Gpu(GpuPtrFactory factory,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  scoped_refptr<gpu::GpuChannelHost> GetGpuChannel();
  void OnEstablishedGpuChannel(int client_id,
                               mojo::ScopedMessagePipeHandle channel_handle,
                               const gpu::GPUInfo& gpu_info,
                               const gpu::GpuFeatureInfo& gpu_feature_info);

  // gpu::GpuChannelHostFactory overrides:
  bool IsMainThread() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetIOThreadTaskRunner() override;
  std::unique_ptr<base::SharedMemory> AllocateSharedMemory(
      size_t size) override;

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  GpuPtrFactory factory_;
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
