// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_file_ref_proxy.h"

#include <map>

#include "base/bind.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_file_ref.h"
#include "ppapi/c/private/ppb_file_ref_private.h"
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
  virtual PP_Var GetAbsolutePath() OVERRIDE;

  // Executes the pending callback with the given ID. See pending_callbacks_.
  void ExecuteCallback(int callback_id, int32_t result);

 private:
  PluginDispatcher* GetDispatcher() const {
    return PluginDispatcher::GetForResource(this);
  }

  // Adds a callback to the list and returns its ID. Returns 0 if the callback
  // is invalid.
  int SendCallback(PP_CompletionCallback callback);

  // This class can have any number of out-standing requests with completion
  // callbacks, in contrast to most resources which have one possible pending
  // callback pending (like a Flush callback).
  //
  // To keep track of them, assign integer IDs to the callbacks, which is how
  // the callback will be identified when it's passed to the host and then
  // back here.
  int next_callback_id_;
  typedef std::map<int, PP_CompletionCallback> PendingCallbackMap;
  PendingCallbackMap pending_callbacks_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(FileRef);
};

FileRef::FileRef(const PPB_FileRef_CreateInfo& info)
    : FileRefImpl(FileRefImpl::InitAsProxy(), info),
      next_callback_id_(0) {
}

FileRef::~FileRef() {
  // Abort all pending callbacks. Do this by posting a task to avoid reentering
  // the plugin's Release() call that probably deleted this object.
  for (PendingCallbackMap::iterator i = pending_callbacks_.begin();
       i != pending_callbacks_.end(); ++i) {
    MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
        i->second.func, i->second.user_data,
        static_cast<int32_t>(PP_ERROR_ABORTED)));
  }
}

PP_Resource FileRef::GetParent() {
  PPB_FileRef_CreateInfo create_info;
  GetDispatcher()->Send(new PpapiHostMsg_PPBFileRef_GetParent(
      API_ID_PPB_FILE_REF, host_resource(), &create_info));
  return PPB_FileRef_Proxy::DeserializeFileRef(create_info);
}

int32_t FileRef::MakeDirectory(PP_Bool make_ancestors,
                               PP_CompletionCallback callback) {
  int callback_id = SendCallback(callback);
  if (!callback_id)
    return PP_ERROR_BADARGUMENT;

  GetDispatcher()->Send(new PpapiHostMsg_PPBFileRef_MakeDirectory(
      API_ID_PPB_FILE_REF, host_resource(), make_ancestors, callback_id));
  return PP_OK_COMPLETIONPENDING;
}

int32_t FileRef::Touch(PP_Time last_access_time,
                       PP_Time last_modified_time,
                       PP_CompletionCallback callback) {
  int callback_id = SendCallback(callback);
  if (!callback_id)
    return PP_ERROR_BADARGUMENT;

  GetDispatcher()->Send(new PpapiHostMsg_PPBFileRef_Touch(
      API_ID_PPB_FILE_REF, host_resource(),
      last_access_time, last_modified_time, callback_id));
  return PP_OK_COMPLETIONPENDING;
}

int32_t FileRef::Delete(PP_CompletionCallback callback) {
  int callback_id = SendCallback(callback);
  if (!callback_id)
    return PP_ERROR_BADARGUMENT;

  GetDispatcher()->Send(new PpapiHostMsg_PPBFileRef_Delete(
      API_ID_PPB_FILE_REF, host_resource(), callback_id));
  return PP_OK_COMPLETIONPENDING;
}

int32_t FileRef::Rename(PP_Resource new_file_ref,
                        PP_CompletionCallback callback) {
  int callback_id = SendCallback(callback);
  if (!callback_id)
    return PP_ERROR_BADARGUMENT;

  Resource* new_file_ref_object =
      PpapiGlobals::Get()->GetResourceTracker()->GetResource(new_file_ref);
  if (!new_file_ref_object ||
      new_file_ref_object->host_resource().instance() != pp_instance())
    return PP_ERROR_BADRESOURCE;

  GetDispatcher()->Send(new PpapiHostMsg_PPBFileRef_Rename(
      API_ID_PPB_FILE_REF, host_resource(),
      new_file_ref_object->host_resource(), callback_id));
  return PP_OK_COMPLETIONPENDING;
}

PP_Var FileRef::GetAbsolutePath() {
  ReceiveSerializedVarReturnValue result;
  GetDispatcher()->Send(new PpapiHostMsg_PPBFileRef_GetAbsolutePath(
      API_ID_PPB_FILE_REF, host_resource(), &result));
  return result.Return(GetDispatcher());
}

void FileRef::ExecuteCallback(int callback_id, int32_t result) {
  PendingCallbackMap::iterator found = pending_callbacks_.find(callback_id);
  if (found == pending_callbacks_.end()) {
    // This will happen when the plugin deletes its resource with a pending
    // callback. The callback will be locally issued with an ABORTED call while
    // the operation may still be pending in the renderer.
    return;
  }

  // Executing the callback may mutate the callback list.
  PP_CompletionCallback callback = found->second;
  pending_callbacks_.erase(found);
  PP_RunCompletionCallback(&callback, result);
}

int FileRef::SendCallback(PP_CompletionCallback callback) {
  if (!callback.func)
    return 0;

  // In extreme cases the IDs may wrap around, so avoid duplicates.
  while (pending_callbacks_.find(next_callback_id_) != pending_callbacks_.end())
    next_callback_id_++;

  pending_callbacks_[next_callback_id_] = callback;
  return next_callback_id_++;
}

namespace {

InterfaceProxy* CreateFileRefProxy(Dispatcher* dispatcher) {
  return new PPB_FileRef_Proxy(dispatcher);
}

}  // namespace

PPB_FileRef_Proxy::PPB_FileRef_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

PPB_FileRef_Proxy::~PPB_FileRef_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_FileRef_Proxy::GetPrivateInfo() {
  static const Info info = {
    thunk::GetPPB_FileRefPrivate_Thunk(),
    PPB_FILEREFPRIVATE_INTERFACE,
    API_ID_NONE,  // URL_LOADER is the canonical one.
    false,
    &CreateFileRefProxy
  };
  return &info;
}

// static
PP_Resource PPB_FileRef_Proxy::CreateProxyResource(PP_Resource file_system,
                                                   const char* path) {
  Resource* file_system_object =
      PpapiGlobals::Get()->GetResourceTracker()->GetResource(file_system);
  if (!file_system_object)
    return 0;

  PPB_FileRef_CreateInfo create_info;
  PluginDispatcher::GetForResource(file_system_object)->Send(
      new PpapiHostMsg_PPBFileRef_Create(
          API_ID_PPB_FILE_REF, file_system_object->host_resource(),
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
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFileRef_GetAbsolutePath,
                        OnMsgGetAbsolutePath)

    IPC_MESSAGE_HANDLER(PpapiMsg_PPBFileRef_CallbackComplete,
                        OnMsgCallbackComplete)
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
                                           int callback_id) {
  EnterHostFromHostResourceForceCallback<PPB_FileRef_API> enter(
      host_resource, callback_factory_,
      &PPB_FileRef_Proxy::OnCallbackCompleteInHost, host_resource, callback_id);
  if (enter.succeeded()) {
    enter.SetResult(enter.object()->MakeDirectory(make_ancestors,
                                                  enter.callback()));
  }
}

void PPB_FileRef_Proxy::OnMsgTouch(const HostResource& host_resource,
                                   PP_Time last_access,
                                   PP_Time last_modified,
                                   int callback_id) {
  EnterHostFromHostResourceForceCallback<PPB_FileRef_API> enter(
      host_resource, callback_factory_,
      &PPB_FileRef_Proxy::OnCallbackCompleteInHost, host_resource, callback_id);
  if (enter.succeeded()) {
    enter.SetResult(enter.object()->Touch(last_access, last_modified,
                                          enter.callback()));
  }
}

void PPB_FileRef_Proxy::OnMsgDelete(const HostResource& host_resource,
                                    int callback_id) {
  EnterHostFromHostResourceForceCallback<PPB_FileRef_API> enter(
      host_resource, callback_factory_,
      &PPB_FileRef_Proxy::OnCallbackCompleteInHost, host_resource, callback_id);
  if (enter.succeeded())
    enter.SetResult(enter.object()->Delete(enter.callback()));
}

void PPB_FileRef_Proxy::OnMsgRename(const HostResource& file_ref,
                                    const HostResource& new_file_ref,
                                    int callback_id) {
  EnterHostFromHostResourceForceCallback<PPB_FileRef_API> enter(
      file_ref, callback_factory_,
      &PPB_FileRef_Proxy::OnCallbackCompleteInHost, file_ref, callback_id);
  if (enter.succeeded()) {
    enter.SetResult(enter.object()->Rename(new_file_ref.host_resource(),
                                           enter.callback()));
  }
}

void PPB_FileRef_Proxy::OnMsgGetAbsolutePath(const HostResource& host_resource,
                                             SerializedVarReturnValue result) {
  EnterHostFromHostResource<PPB_FileRef_API> enter(host_resource);
  if (enter.succeeded())
    result.Return(dispatcher(), enter.object()->GetAbsolutePath());
}

void PPB_FileRef_Proxy::OnMsgCallbackComplete(
    const HostResource& host_resource,
    int callback_id,
    int32_t result) {
  // Forward the callback info to the plugin resource.
  EnterPluginFromHostResource<PPB_FileRef_API> enter(host_resource);
  if (enter.succeeded())
    static_cast<FileRef*>(enter.object())->ExecuteCallback(callback_id, result);
}

void PPB_FileRef_Proxy::OnCallbackCompleteInHost(
    int32_t result,
    const HostResource& host_resource,
    int callback_id) {
  // Execute OnMsgCallbackComplete in the plugin process.
  Send(new PpapiMsg_PPBFileRef_CallbackComplete(
      API_ID_PPB_FILE_REF, host_resource, callback_id, result));
}

}  // namespace proxy
}  // namespace ppapi
