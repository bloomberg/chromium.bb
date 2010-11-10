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
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_url_response_info_proxy.h"

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

  // Initialized to -1. Will be set to nonnegative values by the UpdateProgress
  // message when the values are known.
  int64_t bytes_sent_;
  int64_t total_bytes_to_be_sent_;
  int64_t bytes_received_;
  int64_t total_bytes_to_be_received_;

  // When an asynchronous read is pending, this will contain the callback and
  // the buffer to put the data.
  PP_CompletionCallback current_read_callback_;
  char* current_read_buffer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(URLLoader);
};

URLLoader::URLLoader()
    : bytes_sent_(-1),
      total_bytes_to_be_sent_(-1),
      bytes_received_(-1),
      total_bytes_to_be_received_(-1),
      current_read_callback_(PP_MakeCompletionCallback(NULL, NULL)),
      current_read_buffer_(NULL) {
}

URLLoader::~URLLoader() {
}

namespace {

// Plugin interface implmentation ----------------------------------------------

PP_Resource Create(PP_Instance instance_id) {
  PluginDispatcher* dispatcher = PluginDispatcher::Get();
  PP_Resource result = 0;
  dispatcher->Send(new PpapiHostMsg_PPBURLLoader_Create(
      INTERFACE_ID_PPB_URL_LOADER, instance_id, &result));
  if (result)
    PPB_URLLoader_Proxy::TrackPluginResource(result);
  return result;
}

PP_Bool IsURLLoader(PP_Resource resource) {
  URLLoader* object = PluginResource::GetAs<URLLoader>(resource);
  return BoolToPPBool(!!object);
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

PP_Bool GetUploadProgress(PP_Resource loader_id,
                          int64_t* bytes_sent,
                          int64_t* total_bytes_to_be_sent) {
  URLLoader* object = PluginResource::GetAs<URLLoader>(loader_id);
  if (!object || object->bytes_sent_ == -1) {
    *bytes_sent = 0;
    *total_bytes_to_be_sent = 0;
    return PP_FALSE;
  }
  *bytes_sent = object->bytes_sent_;
  *total_bytes_to_be_sent = object->total_bytes_to_be_sent_;
  return PP_TRUE;
}

PP_Bool GetDownloadProgress(PP_Resource loader_id,
                            int64_t* bytes_received,
                            int64_t* total_bytes_to_be_received) {
  URLLoader* object = PluginResource::GetAs<URLLoader>(loader_id);
  if (!object || object->bytes_received_ == -1) {
    *bytes_received = 0;
    *total_bytes_to_be_received = 0;
    return PP_FALSE;
  }
  *bytes_received = object->bytes_received_;
  *total_bytes_to_be_received = object->total_bytes_to_be_received_;
  return PP_TRUE;
}

PP_Resource GetResponseInfo(PP_Resource loader_id) {
  // If we find that plugins are frequently requesting the response info, we
  // can improve performance by caching the PP_Resource in the URLLoader
  // object. This way we only have to do IPC for the first request. However,
  // it seems that most plugins will only call this once so there's no use
  // optimizing this case.

  PP_Resource result;
  PluginDispatcher* dispatcher = PluginDispatcher::Get();
  dispatcher->Send(new PpapiHostMsg_PPBURLLoader_GetResponseInfo(
      INTERFACE_ID_PPB_URL_LOADER, loader_id, &result));
  if (dispatcher->plugin_resource_tracker()->PreparePreviouslyTrackedResource(
      result))
    return result;

  // Tell the response info to create a tracking object and add it to the
  // resource tracker.
  PPB_URLResponseInfo_Proxy::TrackPluginResource(result);
  return result;
}

int32_t ReadResponseBody(PP_Resource loader_id,
                         char* buffer,
                         int32_t bytes_to_read,
                         PP_CompletionCallback callback) {
  URLLoader* object = PluginResource::GetAs<URLLoader>(loader_id);
  if (!object)
    return PP_ERROR_BADRESOURCE;

  if (!buffer)
    return PP_ERROR_BADARGUMENT;  // Must specify an output buffer.
  if (object->current_read_callback_.func)
    return PP_ERROR_INPROGRESS;  // Can only have one request pending.

  // Currently we don't support sync calls to read. We'll need to revisit
  // how this works when we allow blocking calls (from background threads).
  if (!callback.func)
    return PP_ERROR_BADARGUMENT;

  object->current_read_callback_ = callback;
  object->current_read_buffer_ = buffer;

  PluginDispatcher::Get()->Send(new PpapiHostMsg_PPBURLLoader_ReadResponseBody(
      INTERFACE_ID_PPB_URL_LOADER, loader_id, bytes_to_read));
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

const PPB_URLLoader ppb_urlloader = {
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

// Renderer status updates -----------------------------------------------------

// Data associated with callbacks for ReadResponseBody.
struct ReadCallbackInfo {
  base::WeakPtr<PPB_URLLoader_Proxy> loader;
  PP_Resource pp_resource;
  std::string read_buffer;
};

// Callback for renderer calls to ReadResponseBody. This function will forward
// the result to the plugin and clean up the callback info.
void ReadCallbackHandler(void* user_data, int32_t result) {
  scoped_ptr<ReadCallbackInfo> info(static_cast<ReadCallbackInfo*>(user_data));
  if (!info->loader)
    return;

  int32_t bytes_read = 0;
  if (result > 0)
    bytes_read = result;  // Positive results indicate bytes read.
  info->read_buffer.resize(bytes_read);

  info->loader->dispatcher()->Send(
      new PpapiMsg_PPBURLLoader_ReadResponseBody_Ack(
          INTERFACE_ID_PPB_URL_LOADER, info->pp_resource,
          result, info->read_buffer));
}

}  // namespace

// PPB_URLLoader_Proxy ---------------------------------------------------------

PPB_URLLoader_Proxy::PPB_URLLoader_Proxy(Dispatcher* dispatcher,
                                         const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_URLLoader_Proxy::~PPB_URLLoader_Proxy() {
}

// static
void PPB_URLLoader_Proxy::TrackPluginResource(PP_Resource url_loader_resource) {
  linked_ptr<URLLoader> object(new URLLoader);
  PluginDispatcher::Get()->plugin_resource_tracker()->AddResource(
      url_loader_resource, object);
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
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBURLLoader_ReadResponseBody_Ack,
                        OnMsgReadResponseBodyAck)
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
  PP_CompletionCallback callback = ReceiveCallback(serialized_callback);
  int32_t result = ppb_url_loader_target()->Open(
      loader, request_info, callback);
  if (result != PP_ERROR_WOULDBLOCK)
    PP_RunCompletionCallback(&callback, result);
}

void PPB_URLLoader_Proxy::OnMsgFollowRedirect(
    PP_Resource loader,
    uint32_t serialized_callback) {
  PP_CompletionCallback callback = ReceiveCallback(serialized_callback);
  int32_t result = ppb_url_loader_target()->FollowRedirect(
      loader, callback);
  if (result != PP_ERROR_WOULDBLOCK)
    PP_RunCompletionCallback(&callback, result);
}

void PPB_URLLoader_Proxy::OnMsgGetResponseInfo(PP_Resource loader,
                                               PP_Resource* result) {
  *result = ppb_url_loader_target()->GetResponseInfo(loader);
}

void PPB_URLLoader_Proxy::OnMsgReadResponseBody(
    PP_Resource loader,
    int32_t bytes_to_read) {
  // The plugin could be sending us malicious messages, don't accept negative
  // sizes.
  if (bytes_to_read < 0) {
    // TODO(brettw) kill plugin.
    bytes_to_read = 0;
  }

  // This heap object will get deleted by the callback handler.
  // TODO(brettw) this will be leaked if the plugin closes the resource!
  // (Also including the plugin unloading and having the resource implicitly
  // destroyed. Depending on the cleanup ordering, we may not need the weak
  // pointer here.)
  ReadCallbackInfo* info = new ReadCallbackInfo;
  info->loader = AsWeakPtr();
  info->pp_resource = loader;
  info->read_buffer.resize(bytes_to_read);

  int32_t result = ppb_url_loader_target()->ReadResponseBody(
      loader, const_cast<char*>(info->read_buffer.c_str()), bytes_to_read,
      PP_MakeCompletionCallback(&ReadCallbackHandler, info));
  if (result != PP_ERROR_WOULDBLOCK) {
    // Send error (or perhaps success for synchronous reads) back to plugin.
    // The callback function is already set up to do this and also delete the
    // callback info.
    ReadCallbackHandler(info, result);
  }
}

void PPB_URLLoader_Proxy::OnMsgFinishStreamingToFile(
    PP_Resource loader,
    uint32_t serialized_callback) {
  PP_CompletionCallback callback = ReceiveCallback(serialized_callback);
  int32_t result = ppb_url_loader_target()->FinishStreamingToFile(
      loader, callback);
  if (result != PP_ERROR_WOULDBLOCK)
    PP_RunCompletionCallback(&callback, result);
}

void PPB_URLLoader_Proxy::OnMsgClose(PP_Resource loader) {
  ppb_url_loader_target()->Close(loader);
}

void PPB_URLLoader_Proxy::OnMsgUpdateProgress(
    PP_Resource resource,
    int64_t bytes_sent,
    int64_t total_bytes_to_be_sent,
    int64_t bytes_received,
    int64_t total_bytes_to_be_received) {
  URLLoader* object = PluginResource::GetAs<URLLoader>(resource);
  if (!object) {
    NOTREACHED();
    return;
  }

  object->bytes_sent_ = bytes_sent;
  object->total_bytes_to_be_sent_ = total_bytes_to_be_sent;
  object->bytes_received_ = bytes_received;
  object->total_bytes_to_be_received_ = total_bytes_to_be_received;
}

void PPB_URLLoader_Proxy::OnMsgReadResponseBodyAck(PP_Resource pp_resource,
                                                   int32 result,
                                                   const std::string& data) {
  URLLoader* object = PluginResource::GetAs<URLLoader>(pp_resource);
  if (!object) {
    NOTREACHED();
    return;
  }

  if (!object->current_read_callback_.func || !object->current_read_buffer_) {
    NOTREACHED();
    return;
  }

  // In the error case, the string will be empty, so we can always just copy
  // out of it before issuing the callback.
  memcpy(object->current_read_buffer_, data.c_str(), data.length());

  // The plugin should be able to make a new request from their callback, so
  // we have to clear our copy first.
  PP_CompletionCallback temp_callback = object->current_read_callback_;
  object->current_read_callback_ = PP_BlockUntilComplete();
  object->current_read_buffer_ = NULL;
  PP_RunCompletionCallback(&temp_callback, result);
}

}  // namespace proxy
}  // namespace pp
