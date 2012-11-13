// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_file_system_proxy.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_system.h"
#include "ppapi/proxy/enter_proxy.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_file_system_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

using ppapi::thunk::PPB_FileSystem_API;
using ppapi::thunk::ResourceCreationAPI;

namespace ppapi {
namespace proxy {

namespace {

InterfaceProxy* CreateFileSystemProxy(Dispatcher* dispatcher) {
  return new PPB_FileSystem_Proxy(dispatcher);
}

}  // namespace

// This object maintains most of the state of the ref in the plugin for fast
// querying. It's all set in the constructor from the "create info" sent from
// the host.
class FileSystem : public Resource, public PPB_FileSystem_API {
 public:
  FileSystem(const HostResource& host_resource, PP_FileSystemType type);
  virtual ~FileSystem();

  // Resource override.
  virtual PPB_FileSystem_API* AsPPB_FileSystem_API() OVERRIDE;

  // PPB_FileSystem_APi implementation.
  virtual int32_t Open(int64_t expected_size,
                       scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual PP_FileSystemType GetType() OVERRIDE;

  // Called when the host has responded to our open request.
  void OpenComplete(int32_t result);

 private:
  PP_FileSystemType type_;
  bool called_open_;
  scoped_refptr<TrackedCallback> current_open_callback_;

  DISALLOW_COPY_AND_ASSIGN(FileSystem);
};

FileSystem::FileSystem(const HostResource& host_resource,
                       PP_FileSystemType type)
    : Resource(OBJECT_IS_PROXY, host_resource),
      type_(type),
      called_open_(false) {
}

FileSystem::~FileSystem() {
}

PPB_FileSystem_API* FileSystem::AsPPB_FileSystem_API() {
  return this;
}

int32_t FileSystem::Open(int64_t expected_size,
                         scoped_refptr<TrackedCallback> callback) {
  if (TrackedCallback::IsPending(current_open_callback_))
    return PP_ERROR_INPROGRESS;
  if (called_open_)
    return PP_ERROR_FAILED;

  current_open_callback_ = callback;
  called_open_ = true;
  PluginDispatcher::GetForResource(this)->Send(
      new PpapiHostMsg_PPBFileSystem_Open(
          API_ID_PPB_FILE_SYSTEM, host_resource(), expected_size));
  return PP_OK_COMPLETIONPENDING;
}

PP_FileSystemType FileSystem::GetType() {
  return type_;
}

void FileSystem::OpenComplete(int32_t result) {
  current_open_callback_->Run(result);
}

PPB_FileSystem_Proxy::PPB_FileSystem_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

PPB_FileSystem_Proxy::~PPB_FileSystem_Proxy() {
}

const InterfaceProxy::Info* PPB_FileSystem_Proxy::GetInfo() {
  static const Info info = {
    thunk::GetPPB_FileSystem_1_0_Thunk(),
    PPB_FILESYSTEM_INTERFACE_1_0,
    API_ID_PPB_FILE_SYSTEM,
    false,
    &CreateFileSystemProxy,
  };
  return &info;
}

// static
PP_Resource PPB_FileSystem_Proxy::CreateProxyResource(
    PP_Instance instance,
    PP_FileSystemType type) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_ERROR_BADARGUMENT;

  HostResource result;
  dispatcher->Send(new PpapiHostMsg_PPBFileSystem_Create(
      API_ID_PPB_FILE_SYSTEM, instance, type, &result));
  if (result.is_null())
    return 0;
  return (new FileSystem(result, type))->GetReference();
}

bool PPB_FileSystem_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_FileSystem_Proxy, msg)
#if !defined(OS_NACL)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFileSystem_Create, OnMsgCreate)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFileSystem_Open, OnMsgOpen)
#endif
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBFileSystem_OpenComplete, OnMsgOpenComplete)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

#if !defined(OS_NACL)
void PPB_FileSystem_Proxy::OnMsgCreate(PP_Instance instance,
                                       int type,
                                       HostResource* result) {
  thunk::EnterResourceCreation enter(instance);
  if (enter.failed())
    return;
  PP_Resource resource = enter.functions()->CreateFileSystem(
      instance, static_cast<PP_FileSystemType>(type));
  if (!resource)
    return;  // CreateInfo default constructor initializes to 0.
  result->SetHostResource(instance, resource);
}

void PPB_FileSystem_Proxy::OnMsgOpen(const HostResource& host_resource,
                                     int64_t expected_size) {
  EnterHostFromHostResourceForceCallback<PPB_FileSystem_API> enter(
      host_resource, callback_factory_,
      &PPB_FileSystem_Proxy::OpenCompleteInHost, host_resource);
  if (enter.succeeded())
    enter.SetResult(enter.object()->Open(expected_size, enter.callback()));
}
#endif  // !defined(OS_NACL)

// Called in the plugin to handle the open callback.
void PPB_FileSystem_Proxy::OnMsgOpenComplete(const HostResource& host_resource,
                                             int32_t result) {
  EnterPluginFromHostResource<PPB_FileSystem_API> enter(host_resource);
  if (enter.succeeded())
    static_cast<FileSystem*>(enter.object())->OpenComplete(result);
}

#if !defined(OS_NACL)
void PPB_FileSystem_Proxy::OpenCompleteInHost(
    int32_t result,
    const HostResource& host_resource) {
  dispatcher()->Send(new PpapiMsg_PPBFileSystem_OpenComplete(
      API_ID_PPB_FILE_SYSTEM, host_resource, result));
}
#endif  // !defined(OS_NACL)

}  // namespace proxy
}  // namespace ppapi
