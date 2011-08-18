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
#include "ppapi/thunk/ppb_file_ref_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

using ppapi::HostResource;
using ppapi::Resource;
using ppapi::thunk::EnterFunctionNoLock;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_FileRef_API;
using ppapi::thunk::ResourceCreationAPI;

namespace pp {
namespace proxy {

namespace {

InterfaceProxy* CreateFileRefProxy(Dispatcher* dispatcher,
                                   const void* target_interface) {
  return new PPB_FileRef_Proxy(dispatcher, target_interface);
}

}  // namespace

class FileRef : public Resource, public PPB_FileRef_API {
 public:
  explicit FileRef(const PPBFileRef_CreateInfo& info);
  virtual ~FileRef();

  // Resource overrides.
  virtual PPB_FileRef_API* AsPPB_FileRef_API() OVERRIDE;

  // PPB_FileRef_API implementation.
  virtual PP_FileSystemType GetFileSystemType() const OVERRIDE;
  virtual PP_Var GetName() const OVERRIDE;
  virtual PP_Var GetPath() const OVERRIDE;
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

  PP_FileSystemType file_system_type_;
  PP_Var path_;
  PP_Var name_;

  DISALLOW_COPY_AND_ASSIGN(FileRef);
};

FileRef::FileRef(const PPBFileRef_CreateInfo& info)
    : Resource(info.resource) {
  Dispatcher* dispatcher = PluginDispatcher::GetForResource(this);

  file_system_type_ = static_cast<PP_FileSystemType>(info.file_system_type);

  name_ = ReceiveSerializedVarReturnValue(info.name).Return(dispatcher);
  path_ = ReceiveSerializedVarReturnValue(info.path).Return(dispatcher);
}

FileRef::~FileRef() {
  PluginVarTracker& var_tracker =
      PluginResourceTracker::GetInstance()->var_tracker();
  var_tracker.ReleaseVar(path_);
  var_tracker.ReleaseVar(name_);
}

PPB_FileRef_API* FileRef::AsPPB_FileRef_API() {
  return this;
}

PP_FileSystemType FileRef::GetFileSystemType() const {
  return file_system_type_;
}

PP_Var FileRef::GetName() const {
  PluginResourceTracker::GetInstance()->var_tracker().AddRefVar(name_);
  return name_;
}

PP_Var FileRef::GetPath() const {
  PluginResourceTracker::GetInstance()->var_tracker().AddRefVar(path_);
  return path_;
}

PP_Resource FileRef::GetParent() {
  PPBFileRef_CreateInfo create_info;
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

PPB_FileRef_Proxy::PPB_FileRef_Proxy(Dispatcher* dispatcher,
                                     const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_FileRef_Proxy::~PPB_FileRef_Proxy() {
}

const InterfaceProxy::Info* PPB_FileRef_Proxy::GetInfo() {
  static const Info info = {
    ::ppapi::thunk::GetPPB_FileRef_Thunk(),
    PPB_FILEREF_INTERFACE,
    INTERFACE_ID_PPB_FILE_REF,
    false,
    &CreateFileRefProxy,
  };
  return &info;
}

// static
PP_Resource PPB_FileRef_Proxy::CreateProxyResource(PP_Resource file_system,
                                                   const char* path) {
  Resource* file_system_object =
      PluginResourceTracker::GetInstance()->GetResource(file_system);
  if (!file_system_object)
    return 0;

  PPBFileRef_CreateInfo create_info;
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
                                         PPBFileRef_CreateInfo* result) {
  EnterResourceNoLock<PPB_FileRef_API> enter(file_ref, false);
  if (enter.failed()) {
    NOTREACHED();
    return;
  }

  // We need the instance out of the resource for serializing back to the
  // plugin. This code can only run in the host.
  if (dispatcher()->IsPlugin()) {
    NOTREACHED();
    return;
  }
  HostDispatcher* host_dispatcher = static_cast<HostDispatcher*>(dispatcher());
  PP_Instance instance =
      host_dispatcher->ppb_proxy()->GetInstanceForResource(file_ref);

  result->resource.SetHostResource(instance, file_ref);
  result->file_system_type =
      static_cast<int>(enter.object()->GetFileSystemType());
  result->path = SerializedVarReturnValue::Convert(dispatcher(),
                                                   enter.object()->GetPath());
  result->name = SerializedVarReturnValue::Convert(dispatcher(),
                                                   enter.object()->GetName());
}

// static
PP_Resource PPB_FileRef_Proxy::DeserializeFileRef(
    const PPBFileRef_CreateInfo& serialized) {
  if (serialized.resource.is_null())
    return 0;  // Resource invalid.
  return (new FileRef(serialized))->GetReference();
}

void PPB_FileRef_Proxy::OnMsgCreate(const HostResource& file_system,
                                    const std::string& path,
                                    PPBFileRef_CreateInfo* result) {
  EnterFunctionNoLock<ResourceCreationAPI> enter(file_system.instance(), true);
  if (enter.failed())
    return;
  PP_Resource resource = enter.functions()->CreateFileRef(
      file_system.host_resource(), path.c_str());
  if (!resource)
    return;  // CreateInfo default constructor initializes to 0.
  SerializeFileRef(resource, result);
}

void PPB_FileRef_Proxy::OnMsgGetParent(const HostResource& host_resource,
                                       PPBFileRef_CreateInfo* result) {
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
}  // namespace pp
