// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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

namespace pp {
namespace proxy {

class Buffer : public PluginResource {
 public:
  Buffer(PP_Instance instance, int memory_handle, uint32_t size);
  virtual ~Buffer();

  // Resource overrides.
  virtual Buffer* AsBuffer() { return this; }

  uint32_t size() const { return size_; }

  void* Map();
  void Unmap();

 private:
  int memory_handle_;
  uint32_t size_;

  void* mapped_data_;

  DISALLOW_COPY_AND_ASSIGN(Buffer);
};

Buffer::Buffer(PP_Instance instance, int memory_handle, uint32_t size)
    : PluginResource(instance),
      memory_handle_(memory_handle),
      size_(size),
      mapped_data_(NULL) {
}

Buffer::~Buffer() {
  Unmap();
}

void* Buffer::Map() {
  // TODO(brettw) implement this.
  return mapped_data_;
}

void Buffer::Unmap() {
  // TODO(brettw) implement this.
}

namespace {

PP_Resource Create(PP_Instance instance, uint32_t size) {
  PP_Resource result = 0;
  int32_t shm_handle = -1;
  PluginDispatcher::GetForInstance(instance)->Send(
      new PpapiHostMsg_PPBBuffer_Create(
          INTERFACE_ID_PPB_BUFFER, instance, size,
          &result, &shm_handle));
  if (!result)
    return 0;

  linked_ptr<Buffer> object(new Buffer(instance, static_cast<int>(shm_handle),
                                       size));
  PluginResourceTracker::GetInstance()->AddResource(result, object);
  return result;
}

PP_Bool IsBuffer(PP_Resource resource) {
  Buffer* object = PluginResource::GetAs<Buffer>(resource);
  return BoolToPPBool(!!object);
}

PP_Bool Describe(PP_Resource resource, uint32_t* size_in_bytes) {
  Buffer* object = PluginResource::GetAs<Buffer>(resource);
  if (!object) {
    *size_in_bytes = 0;
    return PP_FALSE;
  }
  *size_in_bytes = object->size();
  return PP_TRUE;
}

void* Map(PP_Resource resource) {
  Buffer* object = PluginResource::GetAs<Buffer>(resource);
  if (!object)
    return NULL;
  return object->Map();
}

void Unmap(PP_Resource resource) {
  Buffer* object = PluginResource::GetAs<Buffer>(resource);
  if (object)
    object->Unmap();
}

const PPB_Buffer_Dev ppb_buffer = {
  &Create,
  &IsBuffer,
  &Describe,
  &Map,
  &Unmap,
};

}  // namespace

PPB_Buffer_Proxy::PPB_Buffer_Proxy(Dispatcher* dispatcher,
                                   const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_Buffer_Proxy::~PPB_Buffer_Proxy() {
}

const void* PPB_Buffer_Proxy::GetSourceInterface() const {
  return &ppb_buffer;
}

InterfaceID PPB_Buffer_Proxy::GetInterfaceId() const {
  return INTERFACE_ID_PPB_BUFFER;
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
                                   PP_Resource* result_resource,
                                   int* result_shm_handle) {
  *result_resource = ppb_buffer_target()->Create(instance, size);
  // TODO(brettw) set the shm handle from a trusted interface.
  *result_shm_handle = 0;
}

}  // namespace proxy
}  // namespace pp
