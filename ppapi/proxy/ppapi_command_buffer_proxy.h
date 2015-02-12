// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_COMMAND_BUFFER_PROXY_H_
#define PPAPI_PROXY_COMMAND_BUFFER_PROXY_H_

#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/client/gpu_control.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "gpu/command_buffer/common/command_buffer_shared.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/host_resource.h"

namespace IPC {
class Message;
}

namespace ppapi {
namespace proxy {

class ProxyChannel;
class SerializedHandle;

class PPAPI_PROXY_EXPORT PpapiCommandBufferProxy : public gpu::CommandBuffer,
                                                   public gpu::GpuControl {
 public:
  PpapiCommandBufferProxy(const HostResource& resource,
                          ProxyChannel* channel,
                          const gpu::Capabilities& capabilities,
                          const SerializedHandle& shared_state);
  virtual ~PpapiCommandBufferProxy();

  // gpu::CommandBuffer implementation:
  virtual bool Initialize() override;
  virtual State GetLastState() override;
  virtual int32 GetLastToken() override;
  virtual void Flush(int32 put_offset) override;
  virtual void OrderingBarrier(int32 put_offset) override;
  virtual void WaitForTokenInRange(int32 start, int32 end) override;
  virtual void WaitForGetOffsetInRange(int32 start, int32 end) override;
  virtual void SetGetBuffer(int32 transfer_buffer_id) override;
  virtual scoped_refptr<gpu::Buffer> CreateTransferBuffer(size_t size,
                                                          int32* id) override;
  virtual void DestroyTransferBuffer(int32 id) override;

  // gpu::GpuControl implementation:
  virtual gpu::Capabilities GetCapabilities() override;
  virtual int32 CreateImage(ClientBuffer buffer,
                            size_t width,
                            size_t height,
                            unsigned internalformat) override;
  virtual void DestroyImage(int32 id) override;
  virtual int32 CreateGpuMemoryBufferImage(size_t width,
                                           size_t height,
                                           unsigned internalformat,
                                           unsigned usage) override;
  virtual uint32 InsertSyncPoint() override;
  virtual uint32 InsertFutureSyncPoint() override;
  virtual void RetireSyncPoint(uint32 sync_point) override;
  virtual void SignalSyncPoint(uint32 sync_point,
                               const base::Closure& callback) override;
  virtual void SignalQuery(uint32 query,
                           const base::Closure& callback) override;
  virtual void SetSurfaceVisible(bool visible) override;
  virtual uint32 CreateStreamTexture(uint32 texture_id) override;
  void SetLock(base::Lock*) override;

 private:
  bool Send(IPC::Message* msg);
  void UpdateState(const gpu::CommandBuffer::State& state, bool success);

  // Try to read an updated copy of the state from shared memory.
  void TryUpdateState();

  // The shared memory area used to update state.
  gpu::CommandBufferSharedState* shared_state() const;

  gpu::Capabilities capabilities_;
  State last_state_;
  scoped_ptr<base::SharedMemory> shared_state_shm_;

  HostResource resource_;
  ProxyChannel* channel_;

  base::Closure channel_error_callback_;

  DISALLOW_COPY_AND_ASSIGN(PpapiCommandBufferProxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif // PPAPI_PROXY_COMMAND_BUFFER_PROXY_H_
