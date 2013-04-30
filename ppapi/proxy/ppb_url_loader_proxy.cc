// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_url_loader_proxy.h"

#include <algorithm>
#include <deque>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/private/ppb_proxy_private.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "ppapi/proxy/enter_proxy.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_file_ref_proxy.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_url_loader_api.h"
#include "ppapi/thunk/ppb_url_request_info_api.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

#if defined(OS_LINUX)
#include <sys/shm.h>
#endif

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_URLLoader_API;
using ppapi::thunk::ResourceCreationAPI;

namespace ppapi {
namespace proxy {

namespace {

// The maximum size we'll read into the plugin without being explicitly
// asked for a larger buffer.
const int32_t kMaxReadBufferSize = 16777216;  // 16MB

#if !defined(OS_NACL)
// Called in the renderer when the byte counts have changed. We send a message
// to the plugin to synchronize its counts so it can respond to status polls
// from the plugin.
void UpdateResourceLoadStatus(PP_Instance pp_instance,
                              PP_Resource pp_resource,
                              int64 bytes_sent,
                              int64 total_bytes_to_be_sent,
                              int64 bytes_received,
                              int64 total_bytes_to_be_received) {
  Dispatcher* dispatcher = HostDispatcher::GetForInstance(pp_instance);
  if (!dispatcher)
    return;

  PPBURLLoader_UpdateProgress_Params params;
  params.instance = pp_instance;
  params.resource.SetHostResource(pp_instance, pp_resource);
  params.bytes_sent = bytes_sent;
  params.total_bytes_to_be_sent = total_bytes_to_be_sent;
  params.bytes_received = bytes_received;
  params.total_bytes_to_be_received = total_bytes_to_be_received;
  dispatcher->Send(new PpapiMsg_PPBURLLoader_UpdateProgress(
      API_ID_PPB_URL_LOADER, params));
}
#endif  // !defined(OS_NACL)

InterfaceProxy* CreateURLLoaderProxy(Dispatcher* dispatcher) {
  return new PPB_URLLoader_Proxy(dispatcher);
}

}  // namespace

// URLLoader -------------------------------------------------------------------

class URLLoader : public Resource, public PPB_URLLoader_API {
 public:
  URLLoader(const HostResource& resource);
  virtual ~URLLoader();

  // Resource overrides.
  virtual PPB_URLLoader_API* AsPPB_URLLoader_API() OVERRIDE;

  // PPB_URLLoader_API implementation.
  virtual int32_t Open(PP_Resource request_id,
                       scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t Open(const URLRequestInfoData& data,
                       int requestor_pid,
                       scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t FollowRedirect(
      scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual PP_Bool GetUploadProgress(int64_t* bytes_sent,
                                    int64_t* total_bytes_to_be_sent) OVERRIDE;
  virtual PP_Bool GetDownloadProgress(
      int64_t* bytes_received,
      int64_t* total_bytes_to_be_received) OVERRIDE;
  virtual PP_Resource GetResponseInfo() OVERRIDE;
  virtual int32_t ReadResponseBody(
      void* buffer,
      int32_t bytes_to_read,
      scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual int32_t FinishStreamingToFile(
      scoped_refptr<TrackedCallback> callback) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void GrantUniversalAccess() OVERRIDE;
  virtual void RegisterStatusCallback(
      PP_URLLoaderTrusted_StatusCallback cb) OVERRIDE;
  virtual bool GetResponseInfoData(URLResponseInfoData* data) OVERRIDE;

  // Called when the browser has new up/download progress to report.
  void UpdateProgress(const PPBURLLoader_UpdateProgress_Params& params);

  // Called when the browser responds to our ReadResponseBody request.
  void ReadResponseBodyAck(int32_t result, const char* data);

  // Called when any callback other than the read callback has been executed.
  void CallbackComplete(int32_t result);

 private:
  // Reads the give bytes out of the buffer_, placing them in the given output
  // buffer, and removes the bytes from the buffer.
  //
  // The size must be not more than the current size of the buffer.
  void PopBuffer(void* output_buffer, int32_t output_size);

  PluginDispatcher* GetDispatcher() const {
    return PluginDispatcher::GetForResource(this);
  }

  // Initialized to -1. Will be set to nonnegative values by the UpdateProgress
  // message when the values are known.
  int64_t bytes_sent_;
  int64_t total_bytes_to_be_sent_;
  int64_t bytes_received_;
  int64_t total_bytes_to_be_received_;

  // Current completion callback for the current phase of loading. We have only
  // one thing (open, follow redirect, read, etc.) outstanding at once.
  scoped_refptr<TrackedCallback> current_callback_;

  // When an asynchronous read is pending, this will contain the buffer to put
  // the data. The current_callback_ will identify the read callback.
  void* current_read_buffer_;
  int32_t current_read_buffer_size_;

  // A buffer of all the data that's been sent to us from the host that we
  // have yet to send out to the plugin.
  std::deque<char> buffer_;

  // Cached copy of the response info. When nonzero, we're holding a reference
  // to this resource.
  PP_Resource response_info_;

 private:
  DISALLOW_COPY_AND_ASSIGN(URLLoader);
};

URLLoader::URLLoader(const HostResource& resource)
    : Resource(OBJECT_IS_PROXY, resource),
      bytes_sent_(-1),
      total_bytes_to_be_sent_(-1),
      bytes_received_(-1),
      total_bytes_to_be_received_(-1),
      current_read_buffer_(NULL),
      current_read_buffer_size_(0),
      response_info_(0) {
}

URLLoader::~URLLoader() {
  if (response_info_)
    PpapiGlobals::Get()->GetResourceTracker()->ReleaseResource(response_info_);
}

PPB_URLLoader_API* URLLoader::AsPPB_URLLoader_API() {
  return this;
}

int32_t URLLoader::Open(PP_Resource request_id,
                        scoped_refptr<TrackedCallback> callback) {
  EnterResourceNoLock<thunk::PPB_URLRequestInfo_API> enter(request_id, true);
  if (enter.failed()) {
    Log(PP_LOGLEVEL_ERROR, "PPB_URLLoader.Open: The URL you're requesting is "
        " on a different security origin than your plugin. To request "
        " cross-origin resources, see "
        " PP_URLREQUESTPROPERTY_ALLOWCROSSORIGINREQUESTS.");
    return PP_ERROR_BADRESOURCE;
  }
  return Open(enter.object()->GetData(), 0, callback);
}

int32_t URLLoader::Open(const URLRequestInfoData& data,
                        int requestor_pid,
                        scoped_refptr<TrackedCallback> callback) {
  DCHECK_EQ(0, requestor_pid);  // Used in-process only.

  if (TrackedCallback::IsPending(current_callback_))
    return PP_ERROR_INPROGRESS;

  current_callback_ = callback;

  GetDispatcher()->Send(new PpapiHostMsg_PPBURLLoader_Open(
      API_ID_PPB_URL_LOADER, host_resource(), data));
  return PP_OK_COMPLETIONPENDING;
}

int32_t URLLoader::FollowRedirect(scoped_refptr<TrackedCallback> callback) {
  if (TrackedCallback::IsPending(current_callback_))
    return PP_ERROR_INPROGRESS;

  current_callback_ = callback;

  GetDispatcher()->Send(new PpapiHostMsg_PPBURLLoader_FollowRedirect(
      API_ID_PPB_URL_LOADER, host_resource()));
  return PP_OK_COMPLETIONPENDING;
}

PP_Bool URLLoader::GetUploadProgress(int64_t* bytes_sent,
                                     int64_t* total_bytes_to_be_sent) {
  if (bytes_sent_ == -1) {
    *bytes_sent = 0;
    *total_bytes_to_be_sent = 0;
    return PP_FALSE;
  }
  *bytes_sent = bytes_sent_;
  *total_bytes_to_be_sent = total_bytes_to_be_sent_;
  return PP_TRUE;
}

PP_Bool URLLoader::GetDownloadProgress(
    int64_t* bytes_received,
    int64_t* total_bytes_to_be_received) {
  if (bytes_received_ == -1) {
    *bytes_received = 0;
    *total_bytes_to_be_received = 0;
    return PP_FALSE;
  }
  *bytes_received = bytes_received_;
  *total_bytes_to_be_received = total_bytes_to_be_received_;
  return PP_TRUE;
}

PP_Resource URLLoader::GetResponseInfo() {
  if (!response_info_) {
    bool success = false;
    URLResponseInfoData data;
    GetDispatcher()->Send(new PpapiHostMsg_PPBURLLoader_GetResponseInfo(
        API_ID_PPB_URL_LOADER, host_resource(), &success, &data));
    if (!success)
      return 0;

    // Create a proxy resource for the the file ref host resource if needed.
    PP_Resource body_as_file_ref = 0;
    if (!data.body_as_file_ref.resource.is_null()) {
      body_as_file_ref =
          PPB_FileRef_Proxy::DeserializeFileRef(data.body_as_file_ref);
    }

    // Assumes ownership of body_as_file_ref.
    thunk::EnterResourceCreationNoLock enter(pp_instance());
    response_info_ = enter.functions()->CreateURLResponseInfo(
        pp_instance(), data, body_as_file_ref);
  }

  // The caller expects to get a ref, and we want to keep holding ours.
  PpapiGlobals::Get()->GetResourceTracker()->AddRefResource(response_info_);
  return response_info_;
}

int32_t URLLoader::ReadResponseBody(void* buffer,
                                    int32_t bytes_to_read,
                                    scoped_refptr<TrackedCallback> callback) {
  if (!buffer || bytes_to_read <= 0)
    return PP_ERROR_BADARGUMENT;  // Must specify an output buffer.
  if (TrackedCallback::IsPending(current_callback_))
    return PP_ERROR_INPROGRESS;  // Can only have one request pending.

  if (buffer_.size()) {
    // Special case: we've already buffered some data that we can synchronously
    // return to the caller. Do so without making IPCs.
    int32_t bytes_to_return =
        std::min(bytes_to_read, static_cast<int32_t>(buffer_.size()));
    PopBuffer(buffer, bytes_to_return);
    return bytes_to_return;
  }

  current_callback_ = callback;
  current_read_buffer_ = buffer;
  current_read_buffer_size_ = bytes_to_read;

  GetDispatcher()->Send(new PpapiHostMsg_PPBURLLoader_ReadResponseBody(
      API_ID_PPB_URL_LOADER, host_resource(), bytes_to_read));
  return PP_OK_COMPLETIONPENDING;
}

int32_t URLLoader::FinishStreamingToFile(
    scoped_refptr<TrackedCallback> callback) {
  if (TrackedCallback::IsPending(current_callback_))
    return PP_ERROR_INPROGRESS;

  current_callback_ = callback;

  GetDispatcher()->Send(new PpapiHostMsg_PPBURLLoader_FinishStreamingToFile(
      API_ID_PPB_URL_LOADER, host_resource()));
  return PP_OK_COMPLETIONPENDING;
}

void URLLoader::Close() {
  GetDispatcher()->Send(new PpapiHostMsg_PPBURLLoader_Close(
      API_ID_PPB_URL_LOADER, host_resource()));
}

void URLLoader::GrantUniversalAccess() {
  GetDispatcher()->Send(
      new PpapiHostMsg_PPBURLLoader_GrantUniversalAccess(
          API_ID_PPB_URL_LOADER, host_resource()));
}

void URLLoader::RegisterStatusCallback(
    PP_URLLoaderTrusted_StatusCallback cb) {
  // Not implemented in the proxied version, this is for implementing the
  // proxy itself in the host.
}

bool URLLoader::GetResponseInfoData(URLResponseInfoData* data) {
  // Not implemented in the proxied version, this is for implementing the
  // proxy itself in the host.
  return false;
}

void URLLoader::UpdateProgress(
    const PPBURLLoader_UpdateProgress_Params& params) {
  bytes_sent_ = params.bytes_sent;
  total_bytes_to_be_sent_ = params.total_bytes_to_be_sent;
  bytes_received_ = params.bytes_received;
  total_bytes_to_be_received_ = params.total_bytes_to_be_received;
}

void URLLoader::ReadResponseBodyAck(int32 result, const char* data) {
  if (!TrackedCallback::IsPending(current_callback_) || !current_read_buffer_) {
    NOTREACHED();
    return;
  }

  if (result >= 0) {
    DCHECK_EQ(0U, buffer_.size());

    int32_t bytes_to_return = std::min(current_read_buffer_size_, result);
    std::copy(data,
              data + bytes_to_return,
              static_cast<char*>(current_read_buffer_));

    if (result > bytes_to_return) {
      // Save what remains to be copied when ReadResponseBody is called again.
      buffer_.insert(buffer_.end(),
                     data + bytes_to_return,
                     data + result);
    }

    result = bytes_to_return;
  }

  current_callback_->Run(result);
}

void URLLoader::CallbackComplete(int32_t result) {
  current_callback_->Run(result);
}

void URLLoader::PopBuffer(void* output_buffer, int32_t output_size) {
  CHECK(output_size <= static_cast<int32_t>(buffer_.size()));
  std::copy(buffer_.begin(),
            buffer_.begin() + output_size,
            static_cast<char*>(output_buffer));
  buffer_.erase(buffer_.begin(),
                buffer_.begin() + output_size);
}

// PPB_URLLoader_Proxy ---------------------------------------------------------

PPB_URLLoader_Proxy::PPB_URLLoader_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      callback_factory_(this) {
}

PPB_URLLoader_Proxy::~PPB_URLLoader_Proxy() {
}

// static
PP_Resource PPB_URLLoader_Proxy::TrackPluginResource(
    const HostResource& url_loader_resource) {
  return (new URLLoader(url_loader_resource))->GetReference();
}

// static
const InterfaceProxy::Info* PPB_URLLoader_Proxy::GetTrustedInfo() {
  static const Info info = {
    thunk::GetPPB_URLLoaderTrusted_0_3_Thunk(),
    PPB_URLLOADERTRUSTED_INTERFACE_0_3,
    API_ID_NONE,  // URL_LOADER is the canonical one.
    false,
    &CreateURLLoaderProxy
  };
  return &info;
}

// static
PP_Resource PPB_URLLoader_Proxy::CreateProxyResource(PP_Instance pp_instance) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(pp_instance);
  if (!dispatcher)
    return 0;

  HostResource result;
  dispatcher->Send(new PpapiHostMsg_PPBURLLoader_Create(
      API_ID_PPB_URL_LOADER, pp_instance, &result));
  if (result.is_null())
    return 0;
  return PPB_URLLoader_Proxy::TrackPluginResource(result);
}

bool PPB_URLLoader_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_URLLoader_Proxy, msg)
#if !defined(OS_NACL)
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
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBURLLoader_GrantUniversalAccess,
                        OnMsgGrantUniversalAccess)
#endif  // !defined(OS_NACL)

    IPC_MESSAGE_HANDLER(PpapiMsg_PPBURLLoader_UpdateProgress,
                        OnMsgUpdateProgress)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBURLLoader_ReadResponseBody_Ack,
                        OnMsgReadResponseBodyAck)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPBURLLoader_CallbackComplete,
                        OnMsgCallbackComplete)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw) handle bad messages!
  return handled;
}

#if !defined(OS_NACL)
void PPB_URLLoader_Proxy::PrepareURLLoaderForSendingToPlugin(
    PP_Resource resource) {
  // So the plugin can query load status, we need to register our status
  // callback before sending any URLLoader to the plugin.
  EnterResourceNoLock<PPB_URLLoader_API> enter(resource, false);
  if (enter.succeeded())
    enter.object()->RegisterStatusCallback(&UpdateResourceLoadStatus);
  else
    NOTREACHED();  // Only called internally, resource should be valid.
}

void PPB_URLLoader_Proxy::OnMsgCreate(PP_Instance instance,
                                      HostResource* result) {
  thunk::EnterResourceCreation enter(instance);
  if (enter.succeeded()) {
    result->SetHostResource(instance,
                            enter.functions()->CreateURLLoader(instance));
    PrepareURLLoaderForSendingToPlugin(result->host_resource());
  }
}

void PPB_URLLoader_Proxy::OnMsgOpen(const HostResource& loader,
                                    const URLRequestInfoData& data) {
  int peer_pid = dispatcher()->channel()->peer_pid();

  EnterHostFromHostResourceForceCallback<PPB_URLLoader_API> enter(
      loader, callback_factory_, &PPB_URLLoader_Proxy::OnCallback, loader);
  enter.SetResult(enter.object()->Open(data, peer_pid, enter.callback()));
  // TODO(brettw) bug 73236 register for the status callbacks.
}

void PPB_URLLoader_Proxy::OnMsgFollowRedirect(
    const HostResource& loader) {
  EnterHostFromHostResourceForceCallback<PPB_URLLoader_API> enter(
      loader, callback_factory_, &PPB_URLLoader_Proxy::OnCallback, loader);
  if (enter.succeeded())
    enter.SetResult(enter.object()->FollowRedirect(enter.callback()));
}

void PPB_URLLoader_Proxy::OnMsgGetResponseInfo(const HostResource& loader,
                                               bool* success,
                                               URLResponseInfoData* result) {
  EnterHostFromHostResource<PPB_URLLoader_API> enter(loader);
  if (enter.succeeded())
    *success = enter.object()->GetResponseInfoData(result);
  else
    *success = false;
}

void PPB_URLLoader_Proxy::OnMsgReadResponseBody(
    const HostResource& loader,
    int32_t bytes_to_read) {
  // The plugin could be sending us malicious messages, don't accept negative
  // sizes.
  if (bytes_to_read < 0) {
    // TODO(brettw) kill plugin.
    bytes_to_read = 0;
  }

  // Read more than requested if there are bytes available for synchronous
  // reading. This prevents us from getting too far behind due to IPC message
  // latency. Any extra data will get buffered in the plugin.
  int32_t synchronously_available_bytes =
      static_cast<HostDispatcher*>(dispatcher())->ppb_proxy()->
          GetURLLoaderBufferedBytes(loader.host_resource());
  if (bytes_to_read < kMaxReadBufferSize) {
    // Grow the amount to read so we read ahead synchronously, if possible.
    bytes_to_read =
        std::max(bytes_to_read,
                 std::min(synchronously_available_bytes, kMaxReadBufferSize));
  }

  // This heap object will get deleted by the callback handler.
  // TODO(brettw) this will be leaked if the plugin closes the resource!
  // (Also including the plugin unloading and having the resource implicitly
  // destroyed. Depending on the cleanup ordering, we may not need the weak
  // pointer here.)
  IPC::Message* message =
      new PpapiMsg_PPBURLLoader_ReadResponseBody_Ack(API_ID_PPB_URL_LOADER);
  IPC::ParamTraits<HostResource>::Write(message, loader);

  char* ptr = message->BeginWriteData(bytes_to_read);
  if (!ptr) {
    // TODO(brettw) have a way to check for out-of-memory.
  }

  EnterHostFromHostResourceForceCallback<PPB_URLLoader_API> enter(
      loader, callback_factory_, &PPB_URLLoader_Proxy::OnReadCallback, message);
  if (enter.succeeded()) {
    enter.SetResult(enter.object()->ReadResponseBody(ptr, bytes_to_read,
                                                     enter.callback()));
  }
}

void PPB_URLLoader_Proxy::OnMsgFinishStreamingToFile(
    const HostResource& loader) {
  EnterHostFromHostResourceForceCallback<PPB_URLLoader_API> enter(
      loader, callback_factory_, &PPB_URLLoader_Proxy::OnCallback, loader);
  if (enter.succeeded())
    enter.SetResult(enter.object()->FinishStreamingToFile(enter.callback()));;
}

void PPB_URLLoader_Proxy::OnMsgClose(const HostResource& loader) {
  EnterHostFromHostResource<PPB_URLLoader_API> enter(loader);
  if (enter.succeeded())
    enter.object()->Close();
}

void PPB_URLLoader_Proxy::OnMsgGrantUniversalAccess(
    const HostResource& loader) {
  EnterHostFromHostResource<PPB_URLLoader_API> enter(loader);
  if (enter.succeeded())
    enter.object()->GrantUniversalAccess();
}
#endif  // !defined(OS_NACL)

// Called in the Plugin.
void PPB_URLLoader_Proxy::OnMsgUpdateProgress(
    const PPBURLLoader_UpdateProgress_Params& params) {
  EnterPluginFromHostResource<PPB_URLLoader_API> enter(params.resource);
  if (enter.succeeded())
    static_cast<URLLoader*>(enter.object())->UpdateProgress(params);
}

// Called in the Plugin.
void PPB_URLLoader_Proxy::OnMsgReadResponseBodyAck(
    const IPC::Message& message) {
  PickleIterator iter(message);

  HostResource host_resource;
  if (!IPC::ParamTraits<HostResource>::Read(&message, &iter, &host_resource)) {
    NOTREACHED() << "Expecting HostResource";
    return;
  }

  const char* data;
  int data_len;
  if (!iter.ReadData(&data, &data_len)) {
    NOTREACHED() << "Expecting data";
    return;
  }

  int result;
  if (!iter.ReadInt(&result)) {
    NOTREACHED() << "Expecting result";
    return;
  }

  if (result >= 0 && result != data_len) {
    NOTREACHED() << "Data size mismatch";
    return;
  }

  EnterPluginFromHostResource<PPB_URLLoader_API> enter(host_resource);
  if (enter.succeeded())
    static_cast<URLLoader*>(enter.object())->ReadResponseBodyAck(result, data);
}

// Called in the plugin.
void PPB_URLLoader_Proxy::OnMsgCallbackComplete(
    const HostResource& host_resource,
    int32_t result) {
  EnterPluginFromHostResource<PPB_URLLoader_API> enter(host_resource);
  if (enter.succeeded())
    static_cast<URLLoader*>(enter.object())->CallbackComplete(result);
}

#if !defined(OS_NACL)
void PPB_URLLoader_Proxy::OnReadCallback(int32_t result,
                                         IPC::Message* message) {
  int32_t bytes_read = 0;
  if (result > 0)
    bytes_read = result;  // Positive results indicate bytes read.

  message->TrimWriteData(bytes_read);
  message->WriteInt(result);

  dispatcher()->Send(message);
}

void PPB_URLLoader_Proxy::OnCallback(int32_t result,
                                     const HostResource& resource) {
  dispatcher()->Send(new PpapiMsg_PPBURLLoader_CallbackComplete(
      API_ID_PPB_URL_LOADER, resource, result));
}
#endif  // !defined(OS_NACL)

}  // namespace proxy
}  // namespace ppapi
