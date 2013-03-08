// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_COMMAND_BUFFER_PROXY_H_
#define PPAPI_PROXY_COMMAND_BUFFER_PROXY_H_

#include "base/callback.h"
#include "base/hash_tables.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "ppapi/proxy/ppapi_proxy_export.h"
#include "ppapi/shared_impl/host_resource.h"
#include "gpu/ipc/command_buffer_proxy.h"

namespace IPC {
class Message;
}

namespace ppapi {
namespace proxy {

class ProxyChannel;

class PPAPI_PROXY_EXPORT PpapiCommandBufferProxy : public CommandBufferProxy {
 public:
  PpapiCommandBufferProxy(const HostResource& resource,
                          ProxyChannel* channel);
  virtual ~PpapiCommandBufferProxy();

  void ReportChannelError();

  // CommandBufferProxy implementation:
  virtual int GetRouteID() const OVERRIDE;
  virtual bool Echo(const base::Closure& callback) OVERRIDE;
  virtual bool SetParent(CommandBufferProxy* parent_command_buffer,
                         uint32 parent_texture_id) OVERRIDE;
  virtual void SetChannelErrorCallback(const base::Closure& callback) OVERRIDE;

  // gpu::CommandBuffer implementation:
  virtual bool Initialize();
  virtual State GetState();
  virtual State GetLastState();
  virtual int32 GetLastToken();
  virtual void Flush(int32 put_offset);
  virtual State FlushSync(int32 put_offset, int32 last_known_get);
  virtual void SetGetBuffer(int32 transfer_buffer_id);
  virtual void SetGetOffset(int32 get_offset);
  virtual gpu::Buffer CreateTransferBuffer(size_t size, int32* id);
  virtual void DestroyTransferBuffer(int32 id);
  virtual gpu::Buffer GetTransferBuffer(int32 id);
  virtual void SetToken(int32 token);
  virtual void SetParseError(gpu::error::Error error);
  virtual void SetContextLostReason(gpu::error::ContextLostReason reason);
  virtual uint32 InsertSyncPoint();

 private:
  bool Send(IPC::Message* msg);
  void UpdateState(const gpu::CommandBuffer::State& state, bool success);

  typedef base::hash_map<int32, gpu::Buffer> TransferBufferMap;
  TransferBufferMap transfer_buffers_;

  State last_state_;

  HostResource resource_;
  ProxyChannel* channel_;

  base::Closure channel_error_callback_;

  DISALLOW_COPY_AND_ASSIGN(PpapiCommandBufferProxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif // PPAPI_PROXY_COMMAND_BUFFER_PROXY_H_
