// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_COMMAND_BUFFER_NACL_H
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_COMMAND_BUFFER_NACL_H

#include "gpu/command_buffer/common/command_buffer.h"
#include "ppapi/c/dev/pp_graphics_3d_dev.h"
#include "ppapi/c/dev/ppb_graphics_3d_trusted_dev.h"
#include "ppapi/c/pp_resource.h"

struct PPB_Core;

// A CommandBuffer proxy implementation that uses trusted PPAPI interface to
// access a CommandBuffer.

class CommandBufferNacl : public gpu::CommandBuffer {
 public:
  // This class will addref the graphics 3d resource using the core interface.
  CommandBufferNacl(PP_Resource graphics_3d, const PPB_Core* iface_core);
  virtual ~CommandBufferNacl();

  // CommandBuffer implementation.
  virtual bool Initialize(int32 size);
  virtual bool Initialize(base::SharedMemory* buffer, int32 size) {
    // TODO(neb): support for nacl if neccessary
    return false;
  }
  virtual gpu::Buffer GetRingBuffer();
  virtual State GetState();
  virtual State GetLastState();
  virtual void Flush(int32 put_offset);
  virtual State FlushSync(int32 put_offset, int32 last_known_get);
  virtual void SetGetOffset(int32 get_offset);
  virtual int32 CreateTransferBuffer(size_t size, int32 id_request);
  virtual int32 RegisterTransferBuffer(base::SharedMemory* buffer,
                                       size_t size,
                                       int32 id_request) {
    // TODO(neb): support for nacl if neccessary
    return -1;
  }
  virtual void DestroyTransferBuffer(int32 id);
  virtual gpu::Buffer GetTransferBuffer(int32 handle);
  virtual void SetToken(int32 token);
  virtual void SetParseError(gpu::error::Error error);
  virtual void SetContextLostReason(gpu::error::ContextLostReason);

 private:
  PP_Resource graphics_3d_;
  const PPB_Core* iface_core_;
  gpu::Buffer buffer_;
  State last_state_;

  static gpu::CommandBuffer::State PpapiToGpuState(PP_Graphics3DTrustedState s);
  static gpu::CommandBuffer::State ErrorGpuState();
  static gpu::Buffer BufferFromShm(int shm_handle, uint32_t shm_size);
};

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_COMMAND_BUFFER_NACL_H
