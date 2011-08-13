// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_BUFFER_PROXY_H_
#define PPAPI_PPB_BUFFER_PROXY_H_

#include "base/shared_memory.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/thunk/ppb_buffer_api.h"

struct PPB_Buffer_Dev;

namespace ppapi {
class HostResource;
}

namespace pp {
namespace proxy {

class Buffer : public ppapi::thunk::PPB_Buffer_API,
               public PluginResource {
 public:
  Buffer(const ppapi::HostResource& resource,
         const base::SharedMemoryHandle& shm_handle,
         uint32_t size);
  virtual ~Buffer();

  // ResourceObjectBase overrides.
  virtual ppapi::thunk::PPB_Buffer_API* AsPPB_Buffer_API() OVERRIDE;

  // PPB_Buffer_API implementation.
  virtual PP_Bool Describe(uint32_t* size_in_bytes) OVERRIDE;
  virtual PP_Bool IsMapped() OVERRIDE;
  virtual void* Map() OVERRIDE;
  virtual void Unmap() OVERRIDE;

 private:
  base::SharedMemory shm_;
  uint32_t size_;
  void* mapped_data_;
  int map_count_;

  DISALLOW_COPY_AND_ASSIGN(Buffer);
};

class PPB_Buffer_Proxy : public InterfaceProxy {
 public:
  PPB_Buffer_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_Buffer_Proxy();

  static const Info* GetInfo();

  static PP_Resource CreateProxyResource(PP_Instance instance,
                                         uint32_t size);
  static PP_Resource AddProxyResource(const ppapi::HostResource& resource,
                                      base::SharedMemoryHandle shm_handle,
                                      uint32_t size);

  const PPB_Buffer_Dev* ppb_buffer_target() const {
    return static_cast<const PPB_Buffer_Dev*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgCreate(PP_Instance instance,
                   uint32_t size,
                   ppapi::HostResource* result_resource,
                   base::SharedMemoryHandle* result_shm_handle);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PPB_BUFFER_PROXY_H_
