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
  Buffer(uint64_t memory_handle, int32_t size);
  virtual ~Buffer();

  // Resource overrides.
  virtual Buffer* AsBuffer() { return this; }

  int32_t size() const { return size_; }

  void* Map();
  void Unmap();

 private:
  uint64_t memory_handle_;
  int32_t size_;

  void* mapped_data_;

  DISALLOW_COPY_AND_ASSIGN(Buffer);
};

Buffer::Buffer(uint64_t memory_handle, int32_t size)
    : memory_handle_(memory_handle),
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

PP_Resource Create(PP_Module module_id, int32_t size) {
  PP_Resource result = 0;
  uint64_t shm_handle = -1;
  PluginDispatcher::Get()->Send(
      new PpapiHostMsg_PPBBuffer_Create(
          INTERFACE_ID_PPB_BUFFER, module_id, size,
          &result, &shm_handle));
  if (!result)
    return 0;

  linked_ptr<Buffer> object(new Buffer(shm_handle, size));
  PluginDispatcher::Get()->plugin_resource_tracker()->AddResource(
      result, object);
  return result;
}

PP_Bool IsBuffer(PP_Resource resource) {
  Buffer* object = PluginResource::GetAs<Buffer>(resource);
  return BoolToPPBool(!!object);
}

PP_Bool Describe(PP_Resource resource, int32_t* size_in_bytes) {
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

void PPB_Buffer_Proxy::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(PPB_Buffer_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBBuffer_Create, OnMsgCreate)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw) handle bad messages!
}

void PPB_Buffer_Proxy::OnMsgCreate(PP_Module module,
                                   int32_t size,
                                   PP_Resource* result_resource,
                                   uint64_t* result_shm_handle) {
  *result_resource = ppb_buffer_target()->Create(module, size);
  // TODO(brettw) set the shm handle from a trusted interface.
  *result_shm_handle = 0;
}

}  // namespace proxy
}  // namespace pp
