// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_url_loader_proxy.h"

#include <vector>

#include "base/logging.h"
#include "build/build_config.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/dev/ppb_url_loader_dev.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_messages.h"

#if defined(OS_LINUX)
#include <sys/shm.h>
#endif

namespace pp {
namespace proxy {

class URLLoader : public PluginResource {
 public:
  URLLoader();
  virtual ~URLLoader();

  // Resource overrides.
  virtual URLLoader* AsURLLoader() { return this; }

  int64_t bytes_sent_;
  int64_t total_bytes_to_be_sent_;
  int64_t bytes_received_;
  int64_t total_bytes_to_be_received_;

 private:

  DISALLOW_COPY_AND_ASSIGN(URLLoader);
};

URLLoader::URLLoader()
    : bytes_sent_(0),
      total_bytes_to_be_sent_(0),
      bytes_received_(0),
      total_bytes_to_be_received_(0) {
}

URLLoader::~URLLoader() {
}

namespace {

PP_Resource Create(PP_Instance instance_id) {
  PluginDispatcher* dispatcher = PluginDispatcher::Get();
  PP_Resource result = 0;
  dispatcher->Send(new PpapiHostMsg_PPBURLLoader_Create(
      INTERFACE_ID_PPB_URL_LOADER, instance_id, &result));
  if (result) {
    linked_ptr<URLLoader> object(new URLLoader);
    dispatcher->plugin_resource_tracker()->AddResource(result, object);
  }
  return result;
}

bool IsURLLoader(PP_Resource resource) {
  URLLoader* object = PluginResource::GetAs<URLLoader>(resource);
  return !!object;
}

int32_t Open(PP_Resource loader_id,
             PP_Resource request_id,
             PP_CompletionCallback callback) {
  Dispatcher* dispatcher = PluginDispatcher::Get();
  dispatcher->Send(new PpapiHostMsg_PPBURLLoader_Open(
      INTERFACE_ID_PPB_URL_LOADER, loader_id, request_id,
      dispatcher->callback_tracker().SendCallback(callback)));
  return PP_ERROR_WOULDBLOCK;
}

int32_t FollowRedirect(PP_Resource loader_id,
                       PP_CompletionCallback callback) {
  Dispatcher* dispatcher = PluginDispatcher::Get();
  dispatcher->Send(new PpapiHostMsg_PPBURLLoader_FollowRedirect(
      INTERFACE_ID_PPB_URL_LOADER, loader_id,
      dispatcher->callback_tracker().SendCallback(callback)));
  return PP_ERROR_WOULDBLOCK;
}

bool GetUploadProgress(PP_Resource loader_id,
                       int64_t* bytes_sent,
                       int64_t* total_bytes_to_be_sent) {
  // TODO(brettw) implement this as a non-blocking call where we get notified
  // of updates and just send the local copy.
  bool result = false;
  PluginDispatcher::Get()->Send(
      new PpapiHostMsg_PPBURLLoader_GetUploadProgress(
          INTERFACE_ID_PPB_URL_LOADER, loader_id, bytes_sent,
          total_bytes_to_be_sent, &result));
  return result;
}

bool GetDownloadProgress(PP_Resource loader_id,
                         int64_t* bytes_received,
                         int64_t* total_bytes_to_be_received) {
  // TODO(brettw) implement this as a non-blocking call where we get notified
  // of updates and just send the local copy.
  bool result = false;
  PluginDispatcher::Get()->Send(
      new PpapiHostMsg_PPBURLLoader_GetDownloadProgress(
        INTERFACE_ID_PPB_URL_LOADER, loader_id, bytes_received,
        total_bytes_to_be_received, &result));
  return result;
}

PP_Resource GetResponseInfo(PP_Resource loader_id) {
  // TODO(brettw) this needs to have proper refcounting handling for the
  // result!
  /*
  PP_Resource result;
  Dispatcher::Get()->Send(new PpapiHostMsg_PPBURLLoader_GetResponseInfo(
      INTERFACE_ID_PPB_URL_LOADER, loader_id, &result));
  */
  return 0;
}

int32_t ReadResponseBody(PP_Resource loader_id,
                         char* buffer,
                         int32_t bytes_to_read,
                         PP_CompletionCallback callback) {
                         /* TODO
  Dispatcher::Get()->Send(new PpapiHostMsg_PPBURLLoader_ReadResponseBody(
      INTERFACE_ID_PPB_URL_LOADER, loader_id, ));
      */
  return PP_ERROR_WOULDBLOCK;
}

int32_t FinishStreamingToFile(PP_Resource loader_id,
                              PP_CompletionCallback callback) {
  Dispatcher* dispatcher = PluginDispatcher::Get();
  dispatcher->Send(new PpapiHostMsg_PPBURLLoader_FinishStreamingToFile(
      INTERFACE_ID_PPB_URL_LOADER, loader_id,
      dispatcher->callback_tracker().SendCallback(callback)));
  return PP_ERROR_WOULDBLOCK;
}

void Close(PP_Resource loader_id) {
  PluginDispatcher::Get()->Send(new PpapiHostMsg_PPBURLLoader_Close(
      INTERFACE_ID_PPB_URL_LOADER, loader_id));
}

const PPB_URLLoader_Dev ppb_urlloader = {
  &Create,
  &IsURLLoader,
  &Open,
  &FollowRedirect,
  &GetUploadProgress,
  &GetDownloadProgress,
  &GetResponseInfo,
  &ReadResponseBody,
  &FinishStreamingToFile,
  &Close
};

}  // namespace

PPB_URLLoader_Proxy::PPB_URLLoader_Proxy(Dispatcher* dispatcher,
                                         const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_URLLoader_Proxy::~PPB_URLLoader_Proxy() {
}

const void* PPB_URLLoader_Proxy::GetSourceInterface() const {
  return &ppb_urlloader;
}

InterfaceID PPB_URLLoader_Proxy::GetInterfaceId() const {
  return INTERFACE_ID_PPB_URL_LOADER;
}

void PPB_URLLoader_Proxy::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(PPB_URLLoader_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBURLLoader_Create,
                        OnMsgCreate)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBURLLoader_Open,
                        OnMsgOpen)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBURLLoader_FollowRedirect,
                        OnMsgFollowRedirect)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBURLLoader_GetUploadProgress,
                        OnMsgGetUploadProgress)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBURLLoader_GetDownloadProgress,
                        OnMsgGetDownloadProgress)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBURLLoader_GetResponseInfo,
                        OnMsgGetResponseInfo)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBURLLoader_ReadResponseBody,
                        OnMsgReadResponseBody)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBURLLoader_FinishStreamingToFile,
                        OnMsgFinishStreamingToFile)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBURLLoader_Close,
                        OnMsgClose)

    IPC_MESSAGE_HANDLER(PpapiMsg_PPBURLLoader_UpdateProgress,
                        OnMsgUpdateProgress)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw) handle bad messages!
}

void PPB_URLLoader_Proxy::OnMsgCreate(PP_Instance instance,
                                      PP_Resource* result) {
  *result = ppb_url_loader_target()->Create(instance);
}

void PPB_URLLoader_Proxy::OnMsgOpen(PP_Resource loader,
                                    PP_Resource request_info,
                                    uint32_t serialized_callback) {
  // TODO(brettw): need to notify in case the result is not "would block".
  ppb_url_loader_target()->Open(loader, request_info,
                                ReceiveCallback(serialized_callback));
}

void PPB_URLLoader_Proxy::OnMsgFollowRedirect(
    PP_Resource loader,
    uint32_t serialized_callback) {
  // TODO(brettw): need to notify in case the result is not "would block".
  ppb_url_loader_target()->FollowRedirect(loader,
                                          ReceiveCallback(serialized_callback));
}

void PPB_URLLoader_Proxy::OnMsgGetUploadProgress(
    PP_Resource loader,
    int64* bytes_sent,
    int64* total_bytes_to_be_sent,
    bool* result) {
  *result = ppb_url_loader_target()->GetUploadProgress(
      loader, bytes_sent, total_bytes_to_be_sent);
}

void PPB_URLLoader_Proxy::OnMsgGetDownloadProgress(
    PP_Resource loader,
    int64* bytes_received,
    int64* total_bytes_to_be_received,
    bool* result) {
  *result = ppb_url_loader_target()->GetDownloadProgress(
      loader, bytes_received, total_bytes_to_be_received);
}

void PPB_URLLoader_Proxy::OnMsgGetResponseInfo(PP_Resource loader,
                                               PP_Resource* result) {
  /* TODO(brettw) this will depend on the plugin-side FIXME above.
  *result = ppb_url_loader_target()->GetResponseInfo(loader);
  */
}

void PPB_URLLoader_Proxy::OnMsgReadResponseBody(
    PP_Resource loader,
    int32_t bytes_to_read,
    uint32_t serialized_callback) {
  // TODO(brettw): need to notify in case the result is not "would block".
  /* TODO(brettw) figure out how to share data.
  ppb_url_loader_target()->ReadRseponseBody(loader, bytes_to_read,
      ReceiveCallback(serialized_callback));
  */
}

void PPB_URLLoader_Proxy::OnMsgFinishStreamingToFile(
    PP_Resource loader,
    uint32_t serialized_callback) {
  // TODO(brettw): need to notify in case the result is not "would block".
  ppb_url_loader_target()->FinishStreamingToFile(loader,
      ReceiveCallback(serialized_callback));
}

void PPB_URLLoader_Proxy::OnMsgClose(PP_Resource loader) {
  ppb_url_loader_target()->Close(loader);
}

// TODO(brettw) nobody calls this function. We should have some way to register
// for these updates so when WebKit tells the pepper url loader code that
// something changed, we'll get a callback.
void PPB_URLLoader_Proxy::OnMsgUpdateProgress(
    PP_Resource resource,
    int64_t bytes_sent,
    int64_t total_bytes_to_be_sent,
    int64_t bytes_received,
    int64_t total_bytes_to_be_received) {
  URLLoader* object = PluginResource::GetAs<URLLoader>(resource);
  if (!object)
    return;

  object->bytes_sent_ = bytes_sent;
  object->total_bytes_to_be_sent_ = total_bytes_to_be_sent;
  object->bytes_received_ = bytes_received;
  object->total_bytes_to_be_received_ = total_bytes_to_be_received;
}

}  // namespace proxy
}  // namespace pp
