// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/command_buffer_nacl.h"

#include <sys/mman.h>
#include "gpu/command_buffer/common/logging.h"
#include "native_client/src/shared/ppapi_proxy/plugin_globals.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "ppapi/c/ppb_core.h"

#include "srpcgen/ppb_rpc.h"

using ppapi_proxy::DebugPrintf;

CommandBufferNacl::CommandBufferNacl(PP_Resource graphics_3d,
                                     const PPB_Core* iface_core)
    : graphics_3d_(graphics_3d), iface_core_(iface_core) {
  iface_core_->AddRefResource(graphics_3d_);
}

CommandBufferNacl::~CommandBufferNacl() {
  DebugPrintf("CommandBufferNacl::~CommandBufferNacl()\n");
  iface_core_->ReleaseResource(graphics_3d_);
}

bool CommandBufferNacl::Initialize(int32 size) {
  DebugPrintf("CommandBufferNacl::Initialize\n");
  int32_t success;
  NaClSrpcChannel* channel = ppapi_proxy::GetMainSrpcChannel();
  NaClSrpcError retval =
      PpbGraphics3DRpcClient::PPB_Graphics3DTrusted_InitCommandBuffer(
          channel, graphics_3d_, size, &success);
  DebugPrintf("CommandBufferNaCl::Initialize returned success=%s\n",
      (PP_TRUE == success) ? "TRUE" : "FALSE");
  return NACL_SRPC_RESULT_OK == retval && PP_TRUE == success;
}

gpu::Buffer CommandBufferNacl::GetRingBuffer() {
  DebugPrintf("CommandBufferNacl::GetRingBuffer\n");
  if (!buffer_.ptr) {
    DebugPrintf("CommandBufferNacl::GetRingBuffer: Fetching\n");
    int shm_handle = -1;
    int32_t shm_size = 0;

    NaClSrpcChannel* channel = ppapi_proxy::GetMainSrpcChannel();
    NaClSrpcError retval =
        PpbGraphics3DRpcClient::PPB_Graphics3DTrusted_GetRingBuffer(
            channel, graphics_3d_, &shm_handle, &shm_size);
    if (NACL_SRPC_RESULT_OK != retval) {
      shm_handle = -1;
    }
    buffer_ = BufferFromShm(shm_handle, shm_size);
  }

  return buffer_;
}

gpu::CommandBuffer::State CommandBufferNacl::GetState() {
  DebugPrintf("CommandBufferNacl::GetState\n");
  PP_Graphics3DTrustedState state;
  nacl_abi_size_t state_size = static_cast<nacl_abi_size_t>(sizeof(state));

  NaClSrpcChannel* channel = ppapi_proxy::GetMainSrpcChannel();
  NaClSrpcError retval =
      PpbGraphics3DRpcClient::PPB_Graphics3DTrusted_GetState(
          channel, graphics_3d_, &state_size, reinterpret_cast<char*>(&state));
  if (NACL_SRPC_RESULT_OK != retval
      || state_size != static_cast<nacl_abi_size_t>(sizeof(state))) {
    return ErrorGpuState();
  }

  last_state_ = PpapiToGpuState(state);
  return last_state_;
}

gpu::CommandBuffer::State CommandBufferNacl::GetLastState() {
  return last_state_;
}

void CommandBufferNacl::Flush(int32 put_offset) {
  DebugPrintf("CommandBufferNacl::Flush\n");
  NaClSrpcChannel* channel = ppapi_proxy::GetMainSrpcChannel();
  PpbGraphics3DRpcClient::PPB_Graphics3DTrusted_Flush(
      channel, graphics_3d_, put_offset);
}

gpu::CommandBuffer::State CommandBufferNacl::FlushSync(int32 put_offset,
                                                       int32 last_known_get) {
  DebugPrintf("CommandBufferNacl::FlushSync\n");
  PP_Graphics3DTrustedState state;
  nacl_abi_size_t state_size = static_cast<nacl_abi_size_t>(sizeof(state));

  NaClSrpcChannel* channel = ppapi_proxy::GetMainSrpcChannel();
  NaClSrpcError retval =
      PpbGraphics3DRpcClient::PPB_Graphics3DTrusted_FlushSyncFast(
          channel,
          graphics_3d_,
          put_offset,
          last_known_get,
          &state_size,
          reinterpret_cast<char*>(&state));
  if (NACL_SRPC_RESULT_OK != retval
      || state_size != static_cast<nacl_abi_size_t>(sizeof(state))) {
    return ErrorGpuState();
  }

  last_state_ = PpapiToGpuState(state);
  return last_state_;
}

void CommandBufferNacl::SetGetOffset(int32 get_offset) {
  DebugPrintf("CommandBufferNacl::SetGetOffset\n");
  // Not implemented by proxy.
  GPU_NOTREACHED();
}

int32 CommandBufferNacl::CreateTransferBuffer(size_t size, int32 id_request) {
  DebugPrintf("CommandBufferNacl::CreateTransferBuffer\n");
  int32_t id;

  NaClSrpcChannel* channel = ppapi_proxy::GetMainSrpcChannel();
  NaClSrpcError retval =
      PpbGraphics3DRpcClient::PPB_Graphics3DTrusted_CreateTransferBuffer(
          channel, graphics_3d_, size, id_request, &id);
  if (NACL_SRPC_RESULT_OK != retval)
    return 0;

  return id;
}

void CommandBufferNacl::DestroyTransferBuffer(int32 id) {
  DebugPrintf("CommandBufferNacl::DestroyTransferBuffer\n");
  NaClSrpcChannel* channel = ppapi_proxy::GetMainSrpcChannel();
  PpbGraphics3DRpcClient::PPB_Graphics3DTrusted_DestroyTransferBuffer(
      channel, graphics_3d_, id);
}

gpu::Buffer CommandBufferNacl::GetTransferBuffer(int32 id) {
  DebugPrintf("CommandBufferNacl::GetTransferBuffer\n");
  int shm_handle;
  int32_t shm_size;

  NaClSrpcChannel* channel = ppapi_proxy::GetMainSrpcChannel();
  NaClSrpcError retval =
      PpbGraphics3DRpcClient::PPB_Graphics3DTrusted_GetTransferBuffer(
          channel, graphics_3d_, id, &shm_handle, &shm_size);
  if (NACL_SRPC_RESULT_OK != retval) {
    return BufferFromShm(-1, 0);
  }
  return BufferFromShm(shm_handle, shm_size);
}

void CommandBufferNacl::SetToken(int32 token) {
  DebugPrintf("CommandBufferNacl::SetToken\n");
  // Not implemented by proxy.
  GPU_NOTREACHED();
}

void CommandBufferNacl::SetParseError(
    gpu::error::Error error) {
  DebugPrintf("CommandBufferNacl::SetParseError\n");
  // Not implemented by proxy.
  GPU_NOTREACHED();
}

void CommandBufferNacl::SetContextLostReason(gpu::error::ContextLostReason) {
  // Not implemented by proxy.
  GPU_NOTREACHED();
}

// static
gpu::Buffer CommandBufferNacl::BufferFromShm(int shm_handle,
                                             uint32_t shm_size) {
  gpu::Buffer buffer;
  buffer.ptr = mmap(0,
                    shm_size,
                    PROT_READ | PROT_WRITE,
                    MAP_SHARED,
                    shm_handle,
                    0);
  // TODO(neb): Close the fd now that it's mapped.
  // TODO(neb): Unmap ring & transfer buffers in the destructor.
  if (NULL != buffer.ptr)
    buffer.size = shm_size;
  return buffer;
}

// static
gpu::CommandBuffer::State CommandBufferNacl::ErrorGpuState() {
  gpu::CommandBuffer::State state;
  state.error = gpu::error::kGenericError;
  return state;
}

// static
gpu::CommandBuffer::State CommandBufferNacl::PpapiToGpuState(
    PP_Graphics3DTrustedState s) {
  gpu::CommandBuffer::State state;
  state.num_entries = s.num_entries;
  state.get_offset  = s.get_offset;
  state.put_offset  = s.put_offset;
  state.token       = s.token;
  state.error       = static_cast<gpu::error::Error>(s.error);
  return state;
}
