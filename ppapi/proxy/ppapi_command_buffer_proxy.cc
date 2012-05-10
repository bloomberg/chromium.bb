// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppapi_command_buffer_proxy.h"

#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/proxy_channel.h"
#include "ppapi/shared_impl/api_id.h"
#include "ppapi/shared_impl/host_resource.h"

namespace ppapi {
namespace proxy {

PpapiCommandBufferProxy::PpapiCommandBufferProxy(
    const ppapi::HostResource& resource,
    ProxyChannel* channel)
    : resource_(resource),
      channel_(channel) {
}

PpapiCommandBufferProxy::~PpapiCommandBufferProxy() {
  // Delete all the locally cached shared memory objects, closing the handle
  // in this process.
  for (TransferBufferMap::iterator it = transfer_buffers_.begin();
       it != transfer_buffers_.end(); ++it) {
    delete it->second.shared_memory;
    it->second.shared_memory = NULL;
  }
}

void PpapiCommandBufferProxy::ReportChannelError() {
  if (!channel_error_callback_.is_null()) {
    channel_error_callback_.Run();
    channel_error_callback_.Reset();
  }
}

int PpapiCommandBufferProxy::GetRouteID() const {
  NOTIMPLEMENTED();
  return 0;
}

bool PpapiCommandBufferProxy::Echo(const base::Closure& callback) {
  return false;
}

bool PpapiCommandBufferProxy::SetSurfaceVisible(bool visible) {
  NOTIMPLEMENTED();
  return true;
}

bool PpapiCommandBufferProxy::DiscardBackbuffer() {
  NOTIMPLEMENTED();
  return true;
}

bool PpapiCommandBufferProxy::EnsureBackbuffer() {
  NOTIMPLEMENTED();
  return true;
}

void PpapiCommandBufferProxy::SetMemoryAllocationChangedCallback(
      const base::Callback<void(const GpuMemoryAllocationForRenderer&)>&
          callback) {
  NOTIMPLEMENTED();
}

bool PpapiCommandBufferProxy::SetParent(
    CommandBufferProxy* parent_command_buffer,
    uint32 parent_texture_id) {
  // TODO(fsamuel): Need a proper implementation of this to support offscreen
  // contexts in the guest renderer (WebGL, canvas, etc).
  NOTIMPLEMENTED();
  return false;
}

void PpapiCommandBufferProxy::SetChannelErrorCallback(
    const base::Closure& callback) {
  channel_error_callback_ = callback;
}

void PpapiCommandBufferProxy::SetNotifyRepaintTask(
    const base::Closure& callback) {
  NOTIMPLEMENTED();
}

void PpapiCommandBufferProxy::SetOnConsoleMessageCallback(
    const GpuConsoleMessageCallback& callback) {
  NOTIMPLEMENTED();
}

bool PpapiCommandBufferProxy::Initialize() {
  return Send(new PpapiHostMsg_PPBGraphics3D_InitCommandBuffer(
      ppapi::API_ID_PPB_GRAPHICS_3D, resource_));
}

gpu::CommandBuffer::State PpapiCommandBufferProxy::GetState() {
  // Send will flag state with lost context if IPC fails.
  if (last_state_.error == gpu::error::kNoError) {
    gpu::CommandBuffer::State state;
    if (Send(new PpapiHostMsg_PPBGraphics3D_GetState(
             ppapi::API_ID_PPB_GRAPHICS_3D, resource_, &state))) {
      UpdateState(state);
    }
  }

  return last_state_;
}

gpu::CommandBuffer::State PpapiCommandBufferProxy::GetLastState() {
  return last_state_;
}

void PpapiCommandBufferProxy::Flush(int32 put_offset) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  IPC::Message* message = new PpapiHostMsg_PPBGraphics3D_AsyncFlush(
      ppapi::API_ID_PPB_GRAPHICS_3D, resource_, put_offset);

  // Do not let a synchronous flush hold up this message. If this handler is
  // deferred until after the synchronous flush completes, it will overwrite the
  // cached last_state_ with out-of-date data.
  message->set_unblock(true);
  Send(message);
}

gpu::CommandBuffer::State PpapiCommandBufferProxy::FlushSync(int32 put_offset,
                                                   int32 last_known_get) {
  if (last_known_get == last_state_.get_offset) {
    // Send will flag state with lost context if IPC fails.
    if (last_state_.error == gpu::error::kNoError) {
      gpu::CommandBuffer::State state;
      if (Send(new PpapiHostMsg_PPBGraphics3D_Flush(
               ppapi::API_ID_PPB_GRAPHICS_3D, resource_, put_offset,
              last_known_get, &state))) {
        UpdateState(state);
      }
    }
  } else {
    Flush(put_offset);
  }
  return last_state_;
}

void PpapiCommandBufferProxy::SetGetBuffer(int32 transfer_buffer_id) {
  if (last_state_.error == gpu::error::kNoError) {
    Send(new PpapiHostMsg_PPBGraphics3D_SetGetBuffer(
         ppapi::API_ID_PPB_GRAPHICS_3D, resource_, transfer_buffer_id));
  }
}

void PpapiCommandBufferProxy::SetGetOffset(int32 get_offset) {
  // Not implemented in proxy.
  NOTREACHED();
}

int32 PpapiCommandBufferProxy::CreateTransferBuffer(
    size_t size,
    int32 id_request) {
  if (last_state_.error == gpu::error::kNoError) {
    int32 id;
    if (Send(new PpapiHostMsg_PPBGraphics3D_CreateTransferBuffer(
             ppapi::API_ID_PPB_GRAPHICS_3D, resource_, size, &id))) {
      return id;
    }
  }
  return -1;
}

int32 PpapiCommandBufferProxy::RegisterTransferBuffer(
    base::SharedMemory* shared_memory,
    size_t size,
    int32 id_request) {
  // Not implemented in proxy.
  NOTREACHED();
  return -1;
}

void PpapiCommandBufferProxy::DestroyTransferBuffer(int32 id) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  // Remove the transfer buffer from the client side4 cache.
  TransferBufferMap::iterator it = transfer_buffers_.find(id);
  DCHECK(it != transfer_buffers_.end());

  // Delete the shared memory object, closing the handle in this process.
  delete it->second.shared_memory;

  transfer_buffers_.erase(it);

  Send(new PpapiHostMsg_PPBGraphics3D_DestroyTransferBuffer(
      ppapi::API_ID_PPB_GRAPHICS_3D, resource_, id));
}

gpu::Buffer PpapiCommandBufferProxy::GetTransferBuffer(int32 id) {
  if (last_state_.error != gpu::error::kNoError)
    return gpu::Buffer();

  // Check local cache to see if there is already a client side shared memory
  // object for this id.
  TransferBufferMap::iterator it = transfer_buffers_.find(id);
  if (it != transfer_buffers_.end()) {
    return it->second;
  }

  // Assuming we are in the renderer process, the service is responsible for
  // duplicating the handle. This might not be true for NaCl.
  base::SharedMemoryHandle handle;
  uint32 size;
  if (!Send(new PpapiHostMsg_PPBGraphics3D_GetTransferBuffer(
            ppapi::API_ID_PPB_GRAPHICS_3D, resource_, id, &handle, &size))) {
    return gpu::Buffer();
  }

  // Cache the transfer buffer shared memory object client side.
  scoped_ptr<base::SharedMemory> shared_memory(
      new base::SharedMemory(handle, false));

  // Map the shared memory on demand.
  if (!shared_memory->memory()) {
    if (!shared_memory->Map(size)) {
      return gpu::Buffer();
    }
  }

  gpu::Buffer buffer;
  buffer.ptr = shared_memory->memory();
  buffer.size = size;
  buffer.shared_memory = shared_memory.release();
  transfer_buffers_[id] = buffer;

  return buffer;
}

void PpapiCommandBufferProxy::SetToken(int32 token) {
  NOTREACHED();
}

void PpapiCommandBufferProxy::SetParseError(gpu::error::Error error) {
  NOTREACHED();
}

void PpapiCommandBufferProxy::SetContextLostReason(
    gpu::error::ContextLostReason reason) {
  NOTREACHED();
}

bool PpapiCommandBufferProxy::Send(IPC::Message* msg) {
  DCHECK(last_state_.error == gpu::error::kNoError);

  if (channel_->Send(msg))
    return true;

  last_state_.error = gpu::error::kLostContext;
  return false;
}

void PpapiCommandBufferProxy::UpdateState(
    const gpu::CommandBuffer::State& state) {
  // Handle wraparound. It works as long as we don't have more than 2B state
  // updates in flight across which reordering occurs.
  if (state.generation - last_state_.generation < 0x80000000U)
    last_state_ = state;
}

}  // namespace proxy
}  // namespace ppapi

