// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_CONTEXT_3D_PROXY_H_
#define PPAPI_PPB_CONTEXT_3D_PROXY_H_

#include <vector>

#include "base/shared_memory.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "ppapi/c/dev/pp_graphics_3d_dev.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/proxy_non_thread_safe_ref_count.h"

struct PPB_Context3D_Dev;
struct PPB_Context3DTrusted_Dev;

namespace gpu {
class CommandBuffer;

namespace gles2 {
class GLES2CmdHelper;
class GLES2Implementation;
}  // namespace gles2

}  // namespace gpu

namespace pp {
namespace proxy {

class Surface3D;

class Context3D : public PluginResource {
 public:
  explicit Context3D(const HostResource& resource);
  virtual ~Context3D();

  // Resource overrides.
  virtual Context3D* AsContext3D() { return this; }

  bool CreateImplementation();

  Surface3D* get_draw_surface() const {
    return draw_;
  }

  Surface3D* get_read_surface() const {
    return read_;
  }

  void BindSurfaces(Surface3D* draw, Surface3D* read);

  gpu::gles2::GLES2Implementation* gles2_impl() const {
    return gles2_impl_.get();
  }

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

  const PPB_Context3D_Dev* ppb_context_3d_target() const {
    return reinterpret_cast<const PPB_Context3D_Dev*>(target_interface());
  }
  const PPB_Context3DTrusted_Dev* ppb_context_3d_trusted() const;

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  void OnMsgCreate(PP_Instance instance,
                   PP_Config3D_Dev config,
                   std::vector<int32_t> attribs,
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
}  // namespace pp

#endif  // PPAPI_PPB_CONTEXT_3D_PROXY_H_
