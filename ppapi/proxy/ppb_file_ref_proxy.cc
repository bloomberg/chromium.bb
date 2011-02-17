// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_file_ref_proxy.h"

#include "ppapi/c/dev/ppb_file_ref_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_proxy_private.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_var.h"

namespace pp {
namespace proxy {

// This object maintains most of the state of the ref in the plugin for fast
// querying. It's all set in the constructor from the "create info" sent from
// the host.
class FileRef : public PluginResource {
 public:
  FileRef(const PPBFileRef_CreateInfo& info);
  virtual ~FileRef();

  virtual FileRef* AsFileRef();

  PP_FileSystemType_Dev file_system_type() const { return file_system_type_; }
  const PP_Var& path() const { return path_; }
  const PP_Var& name() const { return name_; }

 private:
  PP_FileSystemType_Dev file_system_type_;
  PP_Var path_;
  PP_Var name_;

  DISALLOW_COPY_AND_ASSIGN(FileRef);
};

FileRef::FileRef(const PPBFileRef_CreateInfo& info)
    : PluginResource(info.resource) {
  Dispatcher* dispatcher = PluginDispatcher::GetForInstance(instance());

  file_system_type_ = static_cast<PP_FileSystemType_Dev>(info.file_system_type);

  name_ = ReceiveSerializedVarReturnValue(info.name).Return(dispatcher);
  path_ = ReceiveSerializedVarReturnValue(info.path).Return(dispatcher);
}

FileRef::~FileRef() {
  PluginVarTracker::GetInstance()->Release(path_);
  PluginVarTracker::GetInstance()->Release(name_);
}

FileRef* FileRef::AsFileRef() {
  return this;
}

namespace {

bool FileRefAndDispatcherForResource(PP_Resource resource,
                                     FileRef** file_ref,
                                     Dispatcher** dispatcher) {
  *file_ref = PluginResource::GetAs<FileRef>(resource);
  if (!file_ref)
    return false;
  *dispatcher = PluginDispatcher::GetForInstance((*file_ref)->instance());
  return !!(*dispatcher);
}

PP_Resource Create(PP_Resource file_system, const char* path) {
  PluginResource* file_system_object =
      PluginResourceTracker::GetInstance()->GetResourceObject(file_system);
  if (!file_system_object)
    return 0;

  Dispatcher* dispatcher =
      PluginDispatcher::GetForInstance(file_system_object->instance());
  if (!dispatcher)
    return 0;

  PPBFileRef_CreateInfo create_info;
  dispatcher->Send(new PpapiHostMsg_PPBFileRef_Create(
      INTERFACE_ID_PPB_FILE_REF, file_system_object->host_resource(),
      path, &create_info));
  return PPB_FileRef_Proxy::DeserializeFileRef(create_info);
}

PP_Bool IsFileRef(PP_Resource resource) {
  FileRef* object = PluginResource::GetAs<FileRef>(resource);
  return BoolToPPBool(!!object);
}

PP_FileSystemType_Dev GetFileSystemType(PP_Resource file_ref) {
  FileRef* object = PluginResource::GetAs<FileRef>(file_ref);
  if (!object)
    return PP_FILESYSTEMTYPE_EXTERNAL;
  return object->file_system_type();
}

PP_Var GetName(PP_Resource file_ref) {
  FileRef* object = PluginResource::GetAs<FileRef>(file_ref);
  if (!object)
    return PP_MakeUndefined();

  PluginVarTracker::GetInstance()->AddRef(object->name());
  return object->name();
}

PP_Var GetPath(PP_Resource file_ref) {
  FileRef* object = PluginResource::GetAs<FileRef>(file_ref);
  if (!object)
    return PP_MakeUndefined();

  PluginVarTracker::GetInstance()->AddRef(object->path());
  return object->path();
}

PP_Resource GetParent(PP_Resource file_ref) {
  FileRef* object;
  Dispatcher* dispatcher;
  if (!FileRefAndDispatcherForResource(file_ref, &object, &dispatcher))
    return 0;

  PPBFileRef_CreateInfo create_info;
  dispatcher->Send(new PpapiHostMsg_PPBFileRef_GetParent(
      INTERFACE_ID_PPB_FILE_REF, object->host_resource(), &create_info));
  return PPB_FileRef_Proxy::DeserializeFileRef(create_info);
}

int32_t MakeDirectory(PP_Resource directory_ref,
                      PP_Bool make_ancestors,
                      struct PP_CompletionCallback callback) {
  FileRef* object;
  Dispatcher* dispatcher;
  if (!FileRefAndDispatcherForResource(directory_ref, &object, &dispatcher))
    return PP_ERROR_BADRESOURCE;

  dispatcher->Send(new PpapiHostMsg_PPBFileRef_MakeDirectory(
      INTERFACE_ID_PPB_FILE_REF, object->host_resource(), make_ancestors,
      dispatcher->callback_tracker().SendCallback(callback)));
  return PP_ERROR_WOULDBLOCK;
}

int32_t Touch(PP_Resource file_ref,
              PP_Time last_access_time,
              PP_Time last_modified_time,
              struct PP_CompletionCallback callback) {
  FileRef* object;
  Dispatcher* dispatcher;
  if (!FileRefAndDispatcherForResource(file_ref, &object, &dispatcher))
    return PP_ERROR_BADRESOURCE;

  dispatcher->Send(new PpapiHostMsg_PPBFileRef_Touch(
      INTERFACE_ID_PPB_FILE_REF, object->host_resource(),
      last_access_time, last_modified_time,
      dispatcher->callback_tracker().SendCallback(callback)));
  return PP_ERROR_WOULDBLOCK;
}

int32_t Delete(PP_Resource file_ref,
               struct PP_CompletionCallback callback) {
  FileRef* object;
  Dispatcher* dispatcher;
  if (!FileRefAndDispatcherForResource(file_ref, &object, &dispatcher))
    return PP_ERROR_BADRESOURCE;

  dispatcher->Send(new PpapiHostMsg_PPBFileRef_Delete(
      INTERFACE_ID_PPB_FILE_REF, object->host_resource(),
      dispatcher->callback_tracker().SendCallback(callback)));
  return PP_ERROR_WOULDBLOCK;
}

int32_t Rename(PP_Resource file_ref,
               PP_Resource new_file_ref,
               struct PP_CompletionCallback callback) {
  FileRef* obj1;
  Dispatcher* dispatcher1;
  if (!FileRefAndDispatcherForResource(file_ref, &obj1, &dispatcher1))
    return PP_ERROR_BADRESOURCE;

  FileRef* obj2;
  Dispatcher* dispatcher2;
  if (!FileRefAndDispatcherForResource(new_file_ref, &obj2, &dispatcher2))
    return PP_ERROR_BADRESOURCE;

  if (obj1->instance() != obj2->instance())
    return PP_ERROR_BADRESOURCE;

  dispatcher1->Send(new PpapiHostMsg_PPBFileRef_Rename(
      INTERFACE_ID_PPB_FILE_REF, obj1->host_resource(),
      obj2->host_resource(),
      dispatcher1->callback_tracker().SendCallback(callback)));
  return PP_ERROR_WOULDBLOCK;
}

const PPB_FileRef_Dev file_ref_interface = {
  &Create,
  &IsFileRef,
  &GetFileSystemType,
  &GetName,
  &GetPath,
  &GetParent,
  &MakeDirectory,
  &Touch,
  &Delete,
  &Rename
};

InterfaceProxy* CreateFileRefProxy(Dispatcher* dispatcher,
                                   const void* target_interface) {
  return new PPB_FileRef_Proxy(dispatcher, target_interface);
}

}  // namespace

PPB_FileRef_Proxy::PPB_FileRef_Proxy(Dispatcher* dispatcher,
                                     const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_FileRef_Proxy::~PPB_FileRef_Proxy() {
}

const InterfaceProxy::Info* PPB_FileRef_Proxy::GetInfo() {
  static const Info info = {
    &file_ref_interface,
    PPB_FILEREF_DEV_INTERFACE,
    INTERFACE_ID_PPB_FILE_REF,
    false,
    &CreateFileRefProxy,
  };
  return &info;
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
  // We need the instance out of the resource for serializing back to the
  // plugin. This code can only run in the host.
  if (dispatcher()->IsPlugin()) {
    NOTREACHED();
    return;
  }
  HostDispatcher* host_dispatcher = static_cast<HostDispatcher*>(dispatcher());
  PP_Instance instance =
      host_dispatcher->GetPPBProxy()->GetInstanceForResource(file_ref);

  result->resource.SetHostResource(instance, file_ref);
  result->file_system_type =
      static_cast<int>(ppb_file_ref_target()->GetFileSystemType(file_ref));
  result->path = SerializedVarReturnValue::Convert(
      dispatcher(),
      ppb_file_ref_target()->GetPath(file_ref));
  result->name = SerializedVarReturnValue::Convert(
      dispatcher(),
      ppb_file_ref_target()->GetName(file_ref));
}

// static
PP_Resource PPB_FileRef_Proxy::DeserializeFileRef(
    const PPBFileRef_CreateInfo& serialized) {
  if (serialized.resource.is_null())
    return 0;  // Resource invalid.

  linked_ptr<FileRef> object(new FileRef(serialized));
  return PluginResourceTracker::GetInstance()->AddResource(object);
}

void PPB_FileRef_Proxy::OnMsgCreate(const HostResource& file_system,
                                    const std::string& path,
                                    PPBFileRef_CreateInfo* result) {

  PP_Resource resource = ppb_file_ref_target()->Create(
      file_system.host_resource(), path.c_str());
  if (!resource)
    return;  // CreateInfo default constructor initializes to 0.
  SerializeFileRef(resource, result);
}

void PPB_FileRef_Proxy::OnMsgGetParent(const HostResource& host_resource,
                                       PPBFileRef_CreateInfo* result) {
  PP_Resource resource = ppb_file_ref_target()->GetParent(
      host_resource.host_resource());
  if (!resource)
    return;  // CreateInfo default constructor initializes to 0.
  SerializeFileRef(resource, result);
}

void PPB_FileRef_Proxy::OnMsgMakeDirectory(const HostResource& host_resource,
                                           PP_Bool make_ancestors,
                                           uint32_t serialized_callback) {
  PP_CompletionCallback callback = ReceiveCallback(serialized_callback);
  int32_t result =
      ppb_file_ref_target()->MakeDirectory(host_resource.host_resource(),
                                           make_ancestors, callback);
  if (result != PP_ERROR_WOULDBLOCK)
    PP_RunCompletionCallback(&callback, result);
}

void PPB_FileRef_Proxy::OnMsgTouch(const HostResource& host_resource,
                                   PP_Time last_access,
                                   PP_Time last_modified,
                                   uint32_t serialized_callback) {
  PP_CompletionCallback callback = ReceiveCallback(serialized_callback);
  int32_t result =
      ppb_file_ref_target()->Touch(host_resource.host_resource(),
                                   last_access, last_modified, callback);
  if (result != PP_ERROR_WOULDBLOCK)
    PP_RunCompletionCallback(&callback, result);
}

void PPB_FileRef_Proxy::OnMsgDelete(const HostResource& host_resource,
                                    uint32_t serialized_callback) {
  PP_CompletionCallback callback = ReceiveCallback(serialized_callback);
  int32_t result =
      ppb_file_ref_target()->Delete(host_resource.host_resource(), callback);
  if (result != PP_ERROR_WOULDBLOCK)
    PP_RunCompletionCallback(&callback, result);
}

void PPB_FileRef_Proxy::OnMsgRename(const HostResource& file_ref,
                                    const HostResource& new_file_ref,
                                    uint32_t serialized_callback) {
  PP_CompletionCallback callback = ReceiveCallback(serialized_callback);
  int32_t result =
      ppb_file_ref_target()->Rename(file_ref.host_resource(),
                                    new_file_ref.host_resource(),
                                    callback);
  if (result != PP_ERROR_WOULDBLOCK)
    PP_RunCompletionCallback(&callback, result);
}

}  // namespace proxy
}  // namespace pp
