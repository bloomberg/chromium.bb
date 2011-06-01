// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_buffer_proxy.h"

#include <vector>

#include "base/logging.h"
#include "build/build_config.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/dev/ppb_buffer_dev.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/thunk/ppb_buffer_api.h"
#include "ppapi/thunk/thunk.h"

namespace pp {
namespace proxy {

namespace {

InterfaceProxy* CreateBufferProxy(Dispatcher* dispatcher,
                                  const void* target_interface) {
  return new PPB_Buffer_Proxy(dispatcher, target_interface);
}

}  // namespace

class Buffer : public ppapi::thunk::PPB_Buffer_API,
               public PluginResource {
 public:
  Buffer(const HostResource& resource,
         int memory_handle,
         uint32_t size);
  virtual ~Buffer();

  // Resource overrides.
  virtual Buffer* AsBuffer() OVERRIDE;

  // ResourceObjectBase overries.
  virtual ppapi::thunk::PPB_Buffer_API* AsBuffer_API() OVERRIDE;

  // PPB_Buffer_API implementation.
  virtual PP_Bool Describe(uint32_t* size_in_bytes) OVERRIDE;
  virtual PP_Bool IsMapped() OVERRIDE;
  virtual void* Map() OVERRIDE;
  virtual void Unmap() OVERRIDE;

 private:
  int memory_handle_;
  uint32_t size_;

  void* mapped_data_;

  DISALLOW_COPY_AND_ASSIGN(Buffer);
};

Buffer::Buffer(const HostResource& resource,
               int memory_handle,
               uint32_t size)
    : PluginResource(resource),
      memory_handle_(memory_handle),
      size_(size),
      mapped_data_(NULL) {
}

Buffer::~Buffer() {
  Unmap();
}

Buffer* Buffer::AsBuffer() {
  return this;
}

ppapi::thunk::PPB_Buffer_API* Buffer::AsBuffer_API() {
  return this;
}

PP_Bool Buffer::Describe(uint32_t* size_in_bytes) {
  *size_in_bytes = size_;
  return PP_TRUE;
}

PP_Bool Buffer::IsMapped() {
  return PP_FromBool(!!mapped_data_);
}

void* Buffer::Map() {
  // TODO(brettw) implement this.
  return mapped_data_;
}

void Buffer::Unmap() {
  // TODO(brettw) implement this.
}

PPB_Buffer_Proxy::PPB_Buffer_Proxy(Dispatcher* dispatcher,
                                   const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_Buffer_Proxy::~PPB_Buffer_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_Buffer_Proxy::GetInfo() {
  static const Info info = {
    ppapi::thunk::GetPPB_Buffer_Thunk(),
    PPB_BUFFER_DEV_INTERFACE,
    INTERFACE_ID_PPB_BUFFER,
    false,
    &CreateBufferProxy,
  };
  return &info;
}

// static
PP_Resource PPB_Buffer_Proxy::CreateProxyResource(PP_Instance instance,
                                                  uint32_t size) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;

  HostResource result;
  int32_t shm_handle = -1;
  dispatcher->Send(new PpapiHostMsg_PPBBuffer_Create(
      INTERFACE_ID_PPB_BUFFER, instance, size,
      &result, &shm_handle));
  if (result.is_null())
    return 0;

  linked_ptr<Buffer> object(new Buffer(result,
                                       static_cast<int>(shm_handle), size));
  return PluginResourceTracker::GetInstance()->AddResource(object);
}

bool PPB_Buffer_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Buffer_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBBuffer_Create, OnMsgCreate)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw) handle bad messages!
  return handled;
}

void PPB_Buffer_Proxy::OnMsgCreate(PP_Instance instance,
                                   uint32_t size,
                                   HostResource* result_resource,
                                   int* result_shm_handle) {
  result_resource->SetHostResource(
      instance,
      ppb_buffer_target()->Create(instance, size));
  // TODO(brettw) set the shm handle from a trusted interface.
  *result_shm_handle = 0;
}

}  // namespace proxy
}  // namespace pp
