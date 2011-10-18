// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_file_ref_proxy.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_ref.h"
#include "ppapi/c/private/ppb_proxy_private.h"
#include "ppapi/proxy/enter_proxy.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/shared_impl/file_ref_impl.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

using ppapi::thunk::EnterFunctionNoLock;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_FileRef_API;
using ppapi::thunk::ResourceCreationAPI;

namespace ppapi {
namespace proxy {

class FileRef : public FileRefImpl {
 public:
  explicit FileRef(const PPB_FileRef_CreateInfo& info);
  virtual ~FileRef();

  // PPB_FileRef_API implementation (not provided by FileRefImpl).
  virtual PP_Resource GetParent() OVERRIDE;
  virtual int32_t MakeDirectory(PP_Bool make_ancestors,
                                PP_CompletionCallback callback) OVERRIDE;
  virtual int32_t Touch(PP_Time last_access_time,
                        PP_Time last_modified_time,
                        PP_CompletionCallback callback) OVERRIDE;
  virtual int32_t Delete(PP_CompletionCallback callback) OVERRIDE;
  virtual int32_t Rename(PP_Resource new_file_ref,
                         PP_CompletionCallback callback) OVERRIDE;

 private:
  PluginDispatcher* GetDispatcher() const {
    return PluginDispatcher::GetForResource(this);
  }

  DISALLOW_IMPLICIT_CONSTRUCTORS(FileRef);
};

FileRef::FileRef(const PPB_FileRef_CreateInfo& info)
    : FileRefImpl(FileRefImpl::InitAsProxy(), info) {
}

FileRef::~FileRef() {
}

PP_Resource FileRef::GetParent() {
  PPB_FileRef_CreateInfo create_info;
  GetDispatcher()->Send(new PpapiHostMsg_PPBFileRef_GetParent(
      INTERFACE_ID_PPB_FILE_REF, host_resource(), &create_info));
  return PPB_FileRef_Proxy::DeserializeFileRef(create_info);
}

int32_t FileRef::MakeDirectory(PP_Bool make_ancestors,
                               PP_CompletionCallback callback) {
  GetDispatcher()->Send(new PpapiHostMsg_PPBFileRef_MakeDirectory(
      INTERFACE_ID_PPB_FILE_REF, host_resource(), make_ancestors,
      GetDispatcher()->callback_tracker().SendCallback(callback)));
  return PP_OK_COMPLETIONPENDING;
}

int32_t FileRef::Touch(PP_Time last_access_time,
                       PP_Time last_modified_time,
                       PP_CompletionCallback callback) {
  GetDispatcher()->Send(new PpapiHostMsg_PPBFileRef_Touch(
      INTERFACE_ID_PPB_FILE_REF, host_resource(),
      last_access_time, last_modified_time,
      GetDispatcher()->callback_tracker().SendCallback(callback)));
  return PP_OK_COMPLETIONPENDING;
}

int32_t FileRef::Delete(PP_CompletionCallback callback) {
  GetDispatcher()->Send(new PpapiHostMsg_PPBFileRef_Delete(
      INTERFACE_ID_PPB_FILE_REF, host_resource(),
      GetDispatcher()->callback_tracker().SendCallback(callback)));
  return PP_OK_COMPLETIONPENDING;
}

int32_t FileRef::Rename(PP_Resource new_file_ref,
                        PP_CompletionCallback callback) {
  Resource* new_file_ref_object =
      PluginResourceTracker::GetInstance()->GetResource(new_file_ref);
  if (!new_file_ref_object ||
      new_file_ref_object->host_resource().instance() != pp_instance())
    return PP_ERROR_BADRESOURCE;

  GetDispatcher()->Send(new PpapiHostMsg_PPBFileRef_Rename(
      INTERFACE_ID_PPB_FILE_REF, host_resource(),
      new_file_ref_object->host_resource(),
      GetDispatcher()->callback_tracker().SendCallback(callback)));
  return PP_OK_COMPLETIONPENDING;
}

PPB_FileRef_Proxy::PPB_FileRef_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
}

PPB_FileRef_Proxy::~PPB_FileRef_Proxy() {
}

// static
PP_Resource PPB_FileRef_Proxy::CreateProxyResource(PP_Resource file_system,
                                                   const char* path) {
  Resource* file_system_object =
      PluginResourceTracker::GetInstance()->GetResource(file_system);
  if (!file_system_object)
    return 0;

  PPB_FileRef_CreateInfo create_info;
  PluginDispatcher::GetForResource(file_system_object)->Send(
      new PpapiHostMsg_PPBFileRef_Create(
          INTERFACE_ID_PPB_FILE_REF, file_system_object->host_resource(),
          path, &create_info));
  return PPB_FileRef_Proxy::DeserializeFileRef(create_info);
}

bool PPB_FileRef_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_FileRef_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFileRef_Create, OnMsgCreate)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFileRef_GetParent, OnMsgGetParent)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFileRef_MakeDirectory,
                        OnMsgMakeDirectory)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFileRef_Touch, OnMsgTouch)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFileRef_Delete, OnMsgDelete)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFileRef_Rename, OnMsgRename)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPB_FileRef_Proxy::SerializeFileRef(PP_Resource file_ref,
                                         PPB_FileRef_CreateInfo* result) {
  EnterResourceNoLock<PPB_FileRef_API> enter(file_ref, false);
  if (enter.succeeded())
    *result = enter.object()->GetCreateInfo();
}

// static
PP_Resource PPB_FileRef_Proxy::DeserializeFileRef(
    const PPB_FileRef_CreateInfo& serialized) {
  if (serialized.resource.is_null())
    return 0;  // Resource invalid.
  return (new FileRef(serialized))->GetReference();
}

void PPB_FileRef_Proxy::OnMsgCreate(const HostResource& file_system,
                                    const std::string& path,
                                    PPB_FileRef_CreateInfo* result) {
  thunk::EnterResourceCreation enter(file_system.instance());
  if (enter.failed())
    return;
  PP_Resource resource = enter.functions()->CreateFileRef(
      file_system.host_resource(), path.c_str());
  if (!resource)
    return;  // CreateInfo default constructor initializes to 0.
  SerializeFileRef(resource, result);
}

void PPB_FileRef_Proxy::OnMsgGetParent(const HostResource& host_resource,
                                       PPB_FileRef_CreateInfo* result) {
  EnterHostFromHostResource<PPB_FileRef_API> enter(host_resource);
  if (enter.succeeded())
    SerializeFileRef(enter.object()->GetParent(), result);
}

void PPB_FileRef_Proxy::OnMsgMakeDirectory(const HostResource& host_resource,
                                           PP_Bool make_ancestors,
                                           uint32_t serialized_callback) {
  EnterHostFromHostResource<PPB_FileRef_API> enter(host_resource);
  if (enter.failed())
    return;
  PP_CompletionCallback callback = ReceiveCallback(serialized_callback);
  int32_t result = enter.object()->MakeDirectory(make_ancestors, callback);
  if (result != PP_OK_COMPLETIONPENDING)
    PP_RunCompletionCallback(&callback, result);
}

void PPB_FileRef_Proxy::OnMsgTouch(const HostResource& host_resource,
                                   PP_Time last_access,
                                   PP_Time last_modified,
                                   uint32_t serialized_callback) {
  EnterHostFromHostResource<PPB_FileRef_API> enter(host_resource);
  if (enter.failed())
    return;
  PP_CompletionCallback callback = ReceiveCallback(serialized_callback);
  int32_t result = enter.object()->Touch(last_access, last_modified, callback);
  if (result != PP_OK_COMPLETIONPENDING)
    PP_RunCompletionCallback(&callback, result);
}

void PPB_FileRef_Proxy::OnMsgDelete(const HostResource& host_resource,
                                    uint32_t serialized_callback) {
  EnterHostFromHostResource<PPB_FileRef_API> enter(host_resource);
  if (enter.failed())
    return;
  PP_CompletionCallback callback = ReceiveCallback(serialized_callback);
  int32_t result = enter.object()->Delete(callback);
  if (result != PP_OK_COMPLETIONPENDING)
    PP_RunCompletionCallback(&callback, result);
}

void PPB_FileRef_Proxy::OnMsgRename(const HostResource& file_ref,
                                    const HostResource& new_file_ref,
                                    uint32_t serialized_callback) {
  EnterHostFromHostResource<PPB_FileRef_API> enter(file_ref);
  if (enter.failed())
    return;
  PP_CompletionCallback callback = ReceiveCallback(serialized_callback);
  int32_t result = enter.object()->Rename(new_file_ref.host_resource(),
                                          callback);
  if (result != PP_OK_COMPLETIONPENDING)
    PP_RunCompletionCallback(&callback, result);
}

}  // namespace proxy
}  // namespace ppapi
