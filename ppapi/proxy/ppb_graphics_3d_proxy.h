// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_GRAPHICS_3D_PROXY_H_
#define PPAPI_PROXY_PPB_GRAPHICS_3D_PROXY_H_

#include <vector>

#include "base/shared_memory.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "ppapi/c/dev/pp_graphics_3d_dev.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/proxy_non_thread_safe_ref_count.h"
#include "ppapi/shared_impl/graphics_3d_impl.h"
#include "ppapi/shared_impl/resource.h"

namespace ppapi {

class HostResource;

namespace proxy {

class Graphics3D : public Resource, public Graphics3DImpl {
 public:
  explicit Graphics3D(const HostResource& resource);
  virtual ~Graphics3D();

  bool Init();

  // Resource overrides.
  virtual thunk::PPB_Graphics3D_API* AsPPB_Graphics3D_API() OVERRIDE {
    return this;
  }

  // Graphics3DTrusted API. These are not implemented in the proxy.
  virtual PP_Bool InitCommandBuffer(int32_t size) OVERRIDE;
  virtual PP_Bool GetRingBuffer(int* shm_handle, uint32_t* shm_size) OVERRIDE;
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

 protected:
  // Graphics3DImpl overrides.
  virtual gpu::CommandBuffer* GetCommandBuffer() OVERRIDE;
  virtual int32 DoSwapBuffers() OVERRIDE;

 private:
  scoped_ptr<gpu::CommandBuffer> command_buffer_;

  DISALLOW_COPY_AND_ASSIGN(Graphics3D);
};

class PPB_Graphics3D_Proxy : public InterfaceProxy {
 public:
  PPB_Graphics3D_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_Graphics3D_Proxy();

  static const Info* GetInfo();

  static PP_Resource CreateProxyResource(PP_Instance instance,
                                         PP_Resource share_context,
                                         const int32_t* attrib_list);

  const PPB_Graphics3D_Dev* ppb_graphics_3d_target() const {
    return static_cast<const PPB_Graphics3D_Dev*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  void OnMsgCreate(PP_Instance instance,
                   const std::vector<int32_t>& attribs,
                   HostResource* result);
  void OnMsgInitCommandBuffer(const HostResource& context,
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
  void OnMsgSwapBuffers(const HostResource& context);
  // Renderer->plugin message handlers.
  void OnMsgSwapBuffersACK(const HostResource& context,
                           int32_t pp_error);

  void SendSwapBuffersACKToPlugin(int32_t result,
                                  const HostResource& context);

  pp::CompletionCallbackFactory<PPB_Graphics3D_Proxy,
                                ProxyNonThreadSafeRefCount> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Graphics3D_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_GRAPHICS_3D_PROXY_H_

