// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_CONTEXT_3D_PROXY_H_
#define PPAPI_PROXY_PPB_CONTEXT_3D_PROXY_H_

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/shared_memory.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "ppapi/c/pp_graphics_3d.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/proxy_non_thread_safe_ref_count.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_context_3d_api.h"

struct PPB_Context3D_Dev;
struct PPB_Context3DTrusted_Dev;

namespace gpu {
class CommandBuffer;

namespace gles2 {
class GLES2CmdHelper;
class GLES2Implementation;
}  // namespace gles2

}  // namespace gpu

namespace ppapi {
namespace proxy {

class Surface3D;

class Context3D : public Resource, public thunk::PPB_Context3D_API {
 public:
  explicit Context3D(const HostResource& resource);
  virtual ~Context3D();

  // Resource overrides.
  virtual thunk::PPB_Context3D_API* AsPPB_Context3D_API() OVERRIDE;

  gpu::gles2::GLES2Implementation* gles2_impl() const {
    return gles2_impl_.get();
  }

  // PPB_Context3D_API implementation.
  virtual int32_t GetAttrib(int32_t attribute, int32_t* value) OVERRIDE;
  virtual int32_t BindSurfaces(PP_Resource draw, PP_Resource read) OVERRIDE;
  virtual int32_t GetBoundSurfaces(PP_Resource* draw,
                                   PP_Resource* read) OVERRIDE;
  virtual PP_Bool InitializeTrusted(int32_t size) OVERRIDE;
  virtual PP_Bool GetRingBuffer(int* shm_handle,
                                uint32_t* shm_size) OVERRIDE;
  virtual PP_Context3DTrustedState GetState() OVERRIDE;
  virtual PP_Bool Flush(int32_t put_offset) OVERRIDE;
  virtual PP_Context3DTrustedState FlushSync(int32_t put_offset) OVERRIDE;
  virtual int32_t CreateTransferBuffer(uint32_t size) OVERRIDE;
  virtual PP_Bool DestroyTransferBuffer(int32_t id) OVERRIDE;
  virtual PP_Bool GetTransferBuffer(int32_t id,
                                    int* shm_handle,
                                    uint32_t* shm_size) OVERRIDE;
  virtual PP_Context3DTrustedState FlushSyncFast(
      int32_t put_offset,
      int32_t last_known_get) OVERRIDE;
  virtual void* MapTexSubImage2DCHROMIUM(GLenum target,
                                         GLint level,
                                         GLint xoffset,
                                         GLint yoffset,
                                         GLsizei width,
                                         GLsizei height,
                                         GLenum format,
                                         GLenum type,
                                         GLenum access) OVERRIDE;
  virtual void UnmapTexSubImage2DCHROMIUM(const void* mem) OVERRIDE;
  virtual gpu::gles2::GLES2Implementation* GetGLES2Impl() OVERRIDE;

  bool CreateImplementation();

 private:
  Surface3D* draw_;
  Surface3D* read_;

  scoped_ptr<gpu::CommandBuffer> command_buffer_;
  scoped_ptr<gpu::gles2::GLES2CmdHelper> helper_;
  int32 transfer_buffer_id_;
  scoped_ptr<gpu::gles2::GLES2Implementation> gles2_impl_;

  DISALLOW_COPY_AND_ASSIGN(Context3D);
};

class PPB_Context3D_Proxy : public InterfaceProxy {
 public:
  PPB_Context3D_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_Context3D_Proxy();

  static const Info* GetInfo();
  static const Info* GetTextureMappingInfo();

  static PP_Resource Create(PP_Instance instance,
                            PP_Config3D_Dev config,
                            PP_Resource share_context,
                            const int32_t* attrib_list);

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  void OnMsgCreate(PP_Instance instance,
                   PP_Config3D_Dev config,
                   const std::vector<int32_t>& attribs,
                   HostResource* result);
  void OnMsgBindSurfaces(const HostResource& context,
                         const HostResource& draw,
                         const HostResource& read,
                         int32_t* result);
  void OnMsgInitialize(const HostResource& context,
                       int32 size,
                       base::SharedMemoryHandle* ring_buffer);
  void OnMsgGetState(const HostResource& context,
                     gpu::CommandBuffer::State* state);
  void OnMsgFlush(const HostResource& context,
                  int32 put_offset,
                  int32 last_known_get,
                  gpu::CommandBuffer::State* state);
  void OnMsgAsyncFlush(const HostResource& context,
                       int32 put_offset);
  void OnMsgCreateTransferBuffer(const HostResource& context,
                                 int32 size,
                                 int32* id);
  void OnMsgDestroyTransferBuffer(const HostResource& context,
                                  int32 id);
  void OnMsgGetTransferBuffer(const HostResource& context,
                              int32 id,
                              base::SharedMemoryHandle* transfer_buffer,
                              uint32* size);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_CONTEXT_3D_PROXY_H_
