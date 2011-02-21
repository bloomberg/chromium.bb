// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_file_system_proxy.h"

#include "base/message_loop.h"
#include "base/task.h"
#include "ppapi/c/dev/ppb_file_system_dev.h"
#include "ppapi/c/pp_errors.h"
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
class FileSystem : public PluginResource {
 public:
  FileSystem(const HostResource& host_resource, PP_FileSystemType_Dev type);
  virtual ~FileSystem();

  virtual FileSystem* AsFileSystem();

  PP_FileSystemType_Dev type_;
  bool opened_;
  PP_CompletionCallback current_open_callback_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FileSystem);
};

FileSystem::FileSystem(const HostResource& host_resource,
                       PP_FileSystemType_Dev type)
    : PluginResource(host_resource),
      type_(type),
      opened_(false),
      current_open_callback_(PP_MakeCompletionCallback(NULL, NULL)) {
}

// TODO(brettw) this logic is duplicated with some other resource objects
// like FileChooser. It would be nice to look at all of the different resources
// that need callback tracking and design something that they can all re-use.
FileSystem::~FileSystem() {
  // Ensure the callback is always fired.
  if (current_open_callback_.func) {
    // TODO(brettw) the callbacks at this level should be refactored with a
    // more automatic tracking system like we have in the renderer.
    MessageLoop::current()->PostTask(FROM_HERE, NewRunnableFunction(
        current_open_callback_.func, current_open_callback_.user_data,
        static_cast<int32_t>(PP_ERROR_ABORTED)));
  }
}

FileSystem* FileSystem::AsFileSystem() {
  return this;
}

namespace {

PP_Resource Create(PP_Instance instance, PP_FileSystemType_Dev type) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_ERROR_BADARGUMENT;

  HostResource result;
  dispatcher->Send(new PpapiHostMsg_PPBFileSystem_Create(
      INTERFACE_ID_PPB_FILE_SYSTEM, instance, type, &result));
  if (result.is_null())
    return 0;

  linked_ptr<FileSystem> object(new FileSystem(result, type));
  return PluginResourceTracker::GetInstance()->AddResource(object);
}

PP_Bool IsFileSystem(PP_Resource resource) {
  FileSystem* object = PluginResource::GetAs<FileSystem>(resource);
  return BoolToPPBool(!!object);
}

int32_t Open(PP_Resource file_system,
             int64_t expected_size,
             struct PP_CompletionCallback callback) {
  FileSystem* object = PluginResource::GetAs<FileSystem>(file_system);
  if (!object)
    return PP_ERROR_BADRESOURCE;
  if (object->opened_)
    return PP_OK;

  Dispatcher* dispatcher = PluginDispatcher::GetForInstance(object->instance());
  if (!dispatcher)
    return PP_ERROR_BADARGUMENT;

  if (object->current_open_callback_.func)
    return PP_ERROR_INPROGRESS;
  object->current_open_callback_ = callback;

  dispatcher->Send(new PpapiHostMsg_PPBFileSystem_Open(
      INTERFACE_ID_PPB_FILE_SYSTEM, object->host_resource(), expected_size));
  return PP_ERROR_WOULDBLOCK;
}

PP_FileSystemType_Dev GetType(PP_Resource resource) {
  FileSystem* object = PluginResource::GetAs<FileSystem>(resource);
  if (!object)
    return PP_FILESYSTEMTYPE_INVALID;
  return object->type_;
}

const PPB_FileSystem_Dev file_system_interface = {
  &Create,
  &IsFileSystem,
  &Open,
  &GetType
};

InterfaceProxy* CreateFileSystemProxy(Dispatcher* dispatcher,
                                      const void* target_interface) {
  return new PPB_FileSystem_Proxy(dispatcher, target_interface);
}

}  // namespace

PPB_FileSystem_Proxy::PPB_FileSystem_Proxy(Dispatcher* dispatcher,
                                           const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

PPB_FileSystem_Proxy::~PPB_FileSystem_Proxy() {
}

const InterfaceProxy::Info* PPB_FileSystem_Proxy::GetInfo() {
  static const Info info = {
    &file_system_interface,
    PPB_FILESYSTEM_DEV_INTERFACE,
    INTERFACE_ID_PPB_FILE_SYSTEM,
    false,
    &CreateFileSystemProxy,
  };
  return &info;
}

bool PPB_FileSystem_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_FileSystem_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFileSystem_Create, OnMsgCreate)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFileSystem_Open, OnMsgOpen)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBFileSystem_OpenComplete, OnMsgOpenComplete)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void PPB_FileSystem_Proxy::OnMsgCreate(PP_Instance instance,
                                       int type,
                                       HostResource* result) {
  PP_Resource resource = ppb_file_system_target()->Create(
      instance, static_cast<PP_FileSystemType_Dev>(type));
  if (!resource)
    return;  // CreateInfo default constructor initializes to 0.
  result->SetHostResource(instance, resource);
}

void PPB_FileSystem_Proxy::OnMsgOpen(const HostResource& host_resource,
                                     int64_t expected_size) {
  CompletionCallback callback = callback_factory_.NewCallback(
      &PPB_FileSystem_Proxy::OpenCompleteInHost, host_resource);

  int32_t result = ppb_file_system_target()->Open(
      host_resource.host_resource(), expected_size,
      callback.pp_completion_callback());
  if (result != PP_ERROR_WOULDBLOCK)
    callback.Run(result);
}

// Called in the plugin to handle the open callback.
void PPB_FileSystem_Proxy::OnMsgOpenComplete(const HostResource& filesystem,
                                             int32_t result) {
  FileSystem* object = PluginResource::GetAs<FileSystem>(
      PluginResourceTracker::GetInstance()->PluginResourceForHostResource(
          filesystem));
  if (!object || !object->current_open_callback_.func)
    return;

  PP_CompletionCallback callback = object->current_open_callback_;
  object->current_open_callback_ = PP_MakeCompletionCallback(NULL, NULL);
  PP_RunCompletionCallback(&callback, result);
}

void PPB_FileSystem_Proxy::OpenCompleteInHost(
    int32_t result,
    const HostResource& host_resource) {
  dispatcher()->Send(new PpapiMsg_PPBFileSystem_OpenComplete(
      INTERFACE_ID_PPB_FILE_SYSTEM, host_resource, result));
}

}  // namespace proxy
}  // namespace pp
