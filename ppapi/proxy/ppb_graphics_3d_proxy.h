// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_GRAPHICS_3D_PROXY_H_
#define PPAPI_PROXY_PPB_GRAPHICS_3D_PROXY_H_

#include <vector>

#include "base/shared_memory.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "ppapi/c/pp_graphics_3d.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/proxy_completion_callback_factory.h"
#include "ppapi/shared_impl/ppb_graphics_3d_shared.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/utility/completion_callback_factory.h"

namespace ppapi {

class HostResource;

namespace proxy {

class SerializedHandle;

class Graphics3D : public PPB_Graphics3D_Shared {
 public:
  explicit Graphics3D(const HostResource& resource);
  virtual ~Graphics3D();

  bool Init(gpu::gles2::GLES2Implementation* share_gles2);

  // Graphics3DTrusted API. These are not implemented in the proxy.
  virtual PP_Bool InitCommandBuffer() OVERRIDE;
  virtual PP_Bool SetGetBuffer(int32_t shm_id) OVERRIDE;
  virtual PP_Graphics3DTrustedState GetState() OVERRIDE;
  virtual PP_Bool Flush(int32_t put_offset) OVERRIDE;
  virtual PP_Graphics3DTrustedState FlushSync(int32_t put_offset) OVERRIDE;
  virtual int32_t CreateTransferBuffer(uint32_t size) OVERRIDE;
  virtual PP_Bool DestroyTransferBuffer(int32_t id) OVERRIDE;
  virtual PP_Bool GetTransferBuffer(int32_t id,
                                    int* shm_handle,
                                    uint32_t* shm_size) OVERRIDE;
  virtual PP_Graphics3DTrustedState FlushSyncFast(
      int32_t put_offset,
      int32_t last_known_get) OVERRIDE;

 private:
  class LockingCommandBuffer;

  // PPB_Graphics3D_Shared overrides.
  virtual gpu::CommandBuffer* GetCommandBuffer() OVERRIDE;
  virtual int32 DoSwapBuffers() OVERRIDE;
  virtual void PushAlreadyLocked() OVERRIDE;
  virtual void PopAlreadyLocked() OVERRIDE;

  int num_already_locked_calls_;
  scoped_ptr<gpu::CommandBuffer> command_buffer_;
  scoped_ptr<LockingCommandBuffer> locking_command_buffer_;

  DISALLOW_COPY_AND_ASSIGN(Graphics3D);
};

class PPB_Graphics3D_Proxy : public InterfaceProxy {
 public:
  PPB_Graphics3D_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_Graphics3D_Proxy();

  static PP_Resource CreateProxyResource(
      PP_Instance instance,
      PP_Resource share_context,
      const int32_t* attrib_list);

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  static const ApiID kApiID = API_ID_PPB_GRAPHICS_3D;

 private:
  void OnMsgCreate(PP_Instance instance,
                   HostResource share_context,
                   const std::vector<int32_t>& attribs,
                   HostResource* result);
  void OnMsgInitCommandBuffer(const HostResource& context);
  void OnMsgSetGetBuffer(const HostResource& context,
                         int32 id);
  void OnMsgGetState(const HostResource& context,
                     gpu::CommandBuffer::State* state,
                     bool* success);
  void OnMsgFlush(const HostResource& context,
                  int32 put_offset,
                  int32 last_known_get,
                  gpu::CommandBuffer::State* state,
                  bool* success);
  void OnMsgAsyncFlush(const HostResource& context,
                       int32 put_offset);
  void OnMsgCreateTransferBuffer(const HostResource& context,
                                 uint32 size,
                                 int32* id);
  void OnMsgDestroyTransferBuffer(const HostResource& context,
                                  int32 id);
  void OnMsgGetTransferBuffer(const HostResource& context,
                              int32 id,
                              ppapi::proxy::SerializedHandle* transfer_buffer);
  void OnMsgSwapBuffers(const HostResource& context);
  // Renderer->plugin message handlers.
  void OnMsgSwapBuffersACK(const HostResource& context,
                           int32_t pp_error);

  void SendSwapBuffersACKToPlugin(int32_t result,
                                  const HostResource& context);

  ProxyCompletionCallbackFactory<PPB_Graphics3D_Proxy> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Graphics3D_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_GRAPHICS_3D_PROXY_H_

