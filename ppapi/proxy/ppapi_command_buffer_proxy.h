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
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/host_resource.h"

namespace IPC {
class Message;
}

namespace ppapi {
namespace proxy {

class SerializedHandle;

class PPAPI_PROXY_EXPORT PpapiCommandBufferProxy : public gpu::CommandBuffer,
                                                   public gpu::GpuControl {
 public:
  PpapiCommandBufferProxy(const HostResource& resource,
                          PluginDispatcher* dispatcher,
                          const gpu::Capabilities& capabilities,
                          const SerializedHandle& shared_state,
                          uint64_t command_buffer_id);
  ~PpapiCommandBufferProxy() override;

  // gpu::CommandBuffer implementation:
  bool Initialize() override;
  State GetLastState() override;
  int32 GetLastToken() override;
  void Flush(int32 put_offset) override;
  void OrderingBarrier(int32 put_offset) override;
  void WaitForTokenInRange(int32 start, int32 end) override;
  void WaitForGetOffsetInRange(int32 start, int32 end) override;
  void SetGetBuffer(int32 transfer_buffer_id) override;
  scoped_refptr<gpu::Buffer> CreateTransferBuffer(size_t size,
                                                  int32* id) override;
  void DestroyTransferBuffer(int32 id) override;

  // gpu::GpuControl implementation:
  gpu::Capabilities GetCapabilities() override;
  int32 CreateImage(ClientBuffer buffer,
                    size_t width,
                    size_t height,
                    unsigned internalformat) override;
  void DestroyImage(int32 id) override;
  int32 CreateGpuMemoryBufferImage(size_t width,
                                   size_t height,
                                   unsigned internalformat,
                                   unsigned usage) override;
  uint32 InsertSyncPoint() override;
  uint32 InsertFutureSyncPoint() override;
  void RetireSyncPoint(uint32 sync_point) override;
  void SignalSyncPoint(uint32 sync_point,
                       const base::Closure& callback) override;
  void SignalQuery(uint32 query, const base::Closure& callback) override;
  void SetSurfaceVisible(bool visible) override;
  uint32 CreateStreamTexture(uint32 texture_id) override;
  void SetLock(base::Lock*) override;
  bool IsGpuChannelLost() override;
  gpu::CommandBufferNamespace GetNamespaceID() const override;
  uint64_t GetCommandBufferID() const override;
  uint64_t GenerateFenceSyncRelease() override;
  bool IsFenceSyncRelease(uint64_t release) override;
  bool IsFenceSyncFlushed(uint64_t release) override;
  bool IsFenceSyncFlushReceived(uint64_t release) override;
  void SignalSyncToken(const gpu::SyncToken& sync_token,
                       const base::Closure& callback) override;
  bool CanWaitUnverifiedSyncToken(const gpu::SyncToken* sync_token) override;

 private:
  bool Send(IPC::Message* msg);
  void UpdateState(const gpu::CommandBuffer::State& state, bool success);

  // Try to read an updated copy of the state from shared memory.
  void TryUpdateState();

  // The shared memory area used to update state.
  gpu::CommandBufferSharedState* shared_state() const;

  void FlushInternal();

  const uint64_t command_buffer_id_;

  gpu::Capabilities capabilities_;
  State last_state_;
  scoped_ptr<base::SharedMemory> shared_state_shm_;

  HostResource resource_;
  PluginDispatcher* dispatcher_;

  base::Closure channel_error_callback_;

  InstanceData::FlushInfo *flush_info_;

  uint64_t next_fence_sync_release_;
  uint64_t pending_fence_sync_release_;
  uint64_t flushed_fence_sync_release_;

  DISALLOW_COPY_AND_ASSIGN(PpapiCommandBufferProxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif // PPAPI_PROXY_COMMAND_BUFFER_PROXY_H_
