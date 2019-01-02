// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_HOST_GPU_CLIENT_DELEGATE_H_
#define COMPONENTS_VIZ_HOST_GPU_CLIENT_DELEGATE_H_

#include "base/callback_forward.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace gpu {
struct GPUInfo;
struct GpuFeatureInfo;
}  // namespace gpu

namespace viz {
namespace mojom {
class GpuService;
}

class HostGpuMemoryBufferManager;

// Delegate interface that GpuClientImpl uses to get the current GpuService
// instance and establish GPU channel for a client. These functions are
// guaranteed to be called on the thread corresponding to GpuClientImpl's task
// runner.
class GpuClientDelegate {
 public:
  enum class EstablishGpuChannelStatus {
    kGpuAccessDenied,  // GPU access was not allowed.
    kGpuHostInvalid,   // Request failed because the gpu host became invalid
                       // while processing the request (e.g. the gpu process may
                       // have been killed). The caller should normally make
                       // another request to establish a new channel.
    kSuccess,
  };

  using EstablishGpuChannelCallback =
      base::OnceCallback<void(mojo::ScopedMessagePipeHandle channel_handle,
                              const gpu::GPUInfo&,
                              const gpu::GpuFeatureInfo&,
                              EstablishGpuChannelStatus status)>;

  virtual ~GpuClientDelegate() {}

  // Returns the current instance of GPU service. If GPU service is not running,
  // tries to launch it. If the launch is unsuccessful, returns nullptr.
  virtual mojom::GpuService* EnsureGpuService() = 0;

  // Sends a request to establish a GPU channel for a client to the GPU service.
  // If GPU service is not running, tries to launch it and send the request.
  virtual void EstablishGpuChannel(int client_id,
                                   uint64_t client_tracing_id,
                                   EstablishGpuChannelCallback callback) = 0;

  // Returns the current HostGpuMemoryBufferManager instance.
  virtual HostGpuMemoryBufferManager* GetGpuMemoryBufferManager() = 0;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_HOST_GPU_CLIENT_DELEGATE_H_
