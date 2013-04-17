// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_url_loader_impl.h"

#include "base/logging.h"
#include "net/base/net_errors.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/url_response_info_data.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_url_request_info_api.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLError.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLLoader.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLResponse.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLLoaderOptions.h"
#include "webkit/appcache/web_application_cache_host_impl.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource_helper.h"
#include "webkit/plugins/ppapi/url_request_info_util.h"
#include "webkit/plugins/ppapi/url_response_info_util.h"

using appcache::WebApplicationCacheHostImpl;
using ppapi::Resource;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_URLLoader_API;
using ppapi::thunk::PPB_URLRequestInfo_API;
using ppapi::TrackedCallback;
using WebKit::WebFrame;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebURLError;
using WebKit::WebURLLoader;
using WebKit::WebURLLoaderOptions;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;

#ifdef _MSC_VER
// Do not warn about use of std::copy with raw pointers.
#pragma warning(disable : 4996)
#endif

namespace webkit {
namespace ppapi {

namespace {

WebFrame* GetFrameForResource(const Resource* resource) {
  PluginInstance* plugin_instance = ResourceHelper::GetPluginInstance(resource);
  if (!plugin_instance)
    return NULL;
  return plugin_instance->container()->element().document().frame();
}

}  // namespace

PPB_URLLoader_Impl::PPB_URLLoader_Impl(PP_Instance instance,
                                       bool main_document_loader)
    : Resource(::ppapi::OBJECT_IS_IMPL, instance),
      main_document_loader_(main_document_loader),
      pending_callback_(),
      bytes_sent_(0),
      total_bytes_to_be_sent_(-1),
      bytes_received_(0),
      total_bytes_to_be_received_(-1),
      user_buffer_(NULL),
      user_buffer_size_(0),
      done_status_(PP_OK_COMPLETIONPENDING),
      is_streaming_to_file_(false),
      is_asynchronous_load_suspended_(false),
      has_universal_access_(false),
      status_callback_(NULL) {
}

PPB_URLLoader_Impl::~PPB_URLLoader_Impl() {
  // There is a path whereby the destructor for the loader_ member can
  // invoke InstanceWasDeleted() upon this PPB_URLLoader_Impl, thereby
  // re-entering the scoped_ptr destructor with the same scoped_ptr object
  // via loader_.reset(). Be sure that loader_ is first NULL then destroy
  // the scoped_ptr. See http://crbug.com/159429.
  scoped_ptr<WebKit::WebURLLoader> for_destruction_only(loader_.release());
}

PPB_URLLoader_API* PPB_URLLoader_Impl::AsPPB_URLLoader_API() {
  return this;
}

void PPB_URLLoader_Impl::InstanceWasDeleted() {
  loader_.reset();
}

int32_t PPB_URLLoader_Impl::Open(PP_Resource request_id,
                                 scoped_refptr<TrackedCallback> callback) {
  EnterResourceNoLock<PPB_URLRequestInfo_API> enter_request(request_id, true);
  if (enter_request.failed()) {
    Log(PP_LOGLEVEL_ERROR,
        "PPB_URLLoader.Open: invalid request resource ID. (Hint to C++ wrapper"
        " users: use the ResourceRequest constructor that takes an instance or"
        " else the request will be null.)");
    return PP_ERROR_BADARGUMENT;
  }
  return Open(enter_request.object()->GetData(), 0, callback);
}

int32_t PPB_URLLoader_Impl::Open(
    const ::ppapi::URLRequestInfoData& request_data,
    int requestor_pid,
    scoped_refptr<TrackedCallback> callback) {
  // Main document loads are already open, so don't allow people to open them
  // again.
  if (main_document_loader_)
    return PP_ERROR_INPROGRESS;

  int32_t rv = ValidateCallback(callback);
  if (rv != PP_OK)
    return rv;

  // Create a copy of the request data since CreateWebURLRequest will populate
  // the file refs.
  ::ppapi::URLRequestInfoData filled_in_request_data = request_data;

  if (URLRequestRequiresUniversalAccess(filled_in_request_data) &&
      !has_universal_access_) {
    Log(PP_LOGLEVEL_ERROR, "PPB_URLLoader.Open: The URL you're requesting is "
        " on a different security origin than your plugin. To request "
        " cross-origin resources, see "
        " PP_URLREQUESTPROPERTY_ALLOWCROSSORIGINREQUESTS.");
    return PP_ERROR_NOACCESS;
  }

  if (loader_.get())
    return PP_ERROR_INPROGRESS;

  WebFrame* frame = GetFrameForResource(this);
  if (!frame)
    return PP_ERROR_FAILED;
  WebURLRequest web_request;
  if (!CreateWebURLRequest(&filled_in_request_data, frame, &web_request))
    return PP_ERROR_FAILED;
  web_request.setRequestorProcessID(requestor_pid);

  // Save a copy of the request info so the plugin can continue to use and
  // change it while we're doing the request without affecting us. We must do
  // this after CreateWebURLRequest since that fills out the file refs.
  request_data_ = filled_in_request_data;

  WebURLLoaderOptions options;
  if (has_universal_access_) {
    options.allowCredentials = true;
    options.crossOriginRequestPolicy =
        WebURLLoaderOptions::CrossOriginRequestPolicyAllow;
  } else {
    // All other HTTP requests are untrusted.
    options.untrustedHTTP = true;
    if (request_data_.allow_cross_origin_requests) {
      // Allow cross-origin requests with access control. The request specifies
      // if credentials are to be sent.
      options.allowCredentials = request_data_.allow_credentials;
      options.crossOriginRequestPolicy =
          WebURLLoaderOptions::CrossOriginRequestPolicyUseAccessControl;
    } else {
      // Same-origin requests can always send credentials.
      options.allowCredentials = true;
    }
  }

  is_asynchronous_load_suspended_ = false;
  loader_.reset(frame->createAssociatedURLLoader(options));
  if (!loader_.get())
    return PP_ERROR_FAILED;

  loader_->loadAsynchronously(web_request, this);

  // Notify completion when we receive a redirect or response headers.
  RegisterCallback(callback);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_URLLoader_Impl::FollowRedirect(
    scoped_refptr<TrackedCallback> callback) {
  int32_t rv = ValidateCallback(callback);
  if (rv != PP_OK)
    return rv;

  SetDefersLoading(false);  // Allow the redirect to continue.
  RegisterCallback(callback);
  return PP_OK_COMPLETIONPENDING;
}

PP_Bool PPB_URLLoader_Impl::GetUploadProgress(int64_t* bytes_sent,
                                              int64_t* total_bytes_to_be_sent) {
  if (!RecordUploadProgress()) {
    *bytes_sent = 0;
    *total_bytes_to_be_sent = 0;
    return PP_FALSE;
  }
  *bytes_sent = bytes_sent_;
  *total_bytes_to_be_sent = total_bytes_to_be_sent_;
  return PP_TRUE;
}

PP_Bool PPB_URLLoader_Impl::GetDownloadProgress(
    int64_t* bytes_received,
    int64_t* total_bytes_to_be_received) {
  if (!RecordDownloadProgress()) {
    *bytes_received = 0;
    *total_bytes_to_be_received = 0;
    return PP_FALSE;
  }
  *bytes_received = bytes_received_;
  *total_bytes_to_be_received = total_bytes_to_be_received_;
  return PP_TRUE;
}

PP_Resource PPB_URLLoader_Impl::GetResponseInfo() {
  ::ppapi::thunk::EnterResourceCreationNoLock enter(pp_instance());
  if (enter.failed() || !response_info_.get())
    return 0;

  // Since we're the "host" the process-local resource for the file ref is
  // the same as the host resource. We pass a ref to the file ref.
  if (!response_info_->body_as_file_ref.resource.is_null()) {
    ::ppapi::PpapiGlobals::Get()->GetResourceTracker()->AddRefResource(
        response_info_->body_as_file_ref.resource.host_resource());
  }
  return enter.functions()->CreateURLResponseInfo(
      pp_instance(),
      *response_info_,
      response_info_->body_as_file_ref.resource.host_resource());
}

int32_t PPB_URLLoader_Impl::ReadResponseBody(
    void* buffer,
    int32_t bytes_to_read,
    scoped_refptr<TrackedCallback> callback) {
  int32_t rv = ValidateCallback(callback);
  if (rv != PP_OK)
    return rv;
  if (!response_info_.get() ||
      !response_info_->body_as_file_ref.resource.is_null())
    return PP_ERROR_FAILED;
  if (bytes_to_read <= 0 || !buffer)
    return PP_ERROR_BADARGUMENT;

  user_buffer_ = static_cast<char*>(buffer);
  user_buffer_size_ = bytes_to_read;

  if (!buffer_.empty())
    return FillUserBuffer();

  // We may have already reached EOF.
  if (done_status_ != PP_OK_COMPLETIONPENDING) {
    user_buffer_ = NULL;
    user_buffer_size_ = 0;
    return done_status_;
  }

  RegisterCallback(callback);
  return PP_OK_COMPLETIONPENDING;
}

int32_t PPB_URLLoader_Impl::FinishStreamingToFile(
    scoped_refptr<TrackedCallback> callback) {
  int32_t rv = ValidateCallback(callback);
  if (rv != PP_OK)
    return rv;
  if (!response_info_.get() ||
      response_info_->body_as_file_ref.resource.is_null())
    return PP_ERROR_FAILED;

  // We may have already reached EOF.
  if (done_status_ != PP_OK_COMPLETIONPENDING)
    return done_status_;

  is_streaming_to_file_ = true;
  if (is_asynchronous_load_suspended_)
    SetDefersLoading(false);

  // Wait for didFinishLoading / didFail.
  RegisterCallback(callback);
  return PP_OK_COMPLETIONPENDING;
}

void PPB_URLLoader_Impl::Close() {
  if (loader_.get())
    loader_->cancel();
  else if (main_document_loader_)
    GetFrameForResource(this)->stopLoading();

  // We must not access the buffer provided by the caller from this point on.
  user_buffer_ = NULL;
  user_buffer_size_ = 0;
  if (TrackedCallback::IsPending(pending_callback_))
    pending_callback_->PostAbort();
}

void PPB_URLLoader_Impl::GrantUniversalAccess() {
  has_universal_access_ = true;
}

void PPB_URLLoader_Impl::SetStatusCallback(
    PP_URLLoaderTrusted_StatusCallback cb) {
  status_callback_ = cb;
}

bool PPB_URLLoader_Impl::GetResponseInfoData(
    ::ppapi::URLResponseInfoData* data) {
  if (!response_info_.get())
    return false;

  *data = *response_info_;

  // We transfer one plugin reference to the FileRef to the caller.
  if (!response_info_->body_as_file_ref.resource.is_null()) {
    ::ppapi::PpapiGlobals::Get()->GetResourceTracker()->AddRefResource(
        response_info_->body_as_file_ref.resource.host_resource());
  }
  return true;
}

void PPB_URLLoader_Impl::willSendRequest(
    WebURLLoader* loader,
    WebURLRequest& new_request,
    const WebURLResponse& redirect_response) {
  if (!request_data_.follow_redirects) {
    SaveResponse(redirect_response);
    SetDefersLoading(true);
    RunCallback(PP_OK);
  }
}

void PPB_URLLoader_Impl::didSendData(
    WebURLLoader* loader,
    unsigned long long bytes_sent,
    unsigned long long total_bytes_to_be_sent) {
  // TODO(darin): Bounds check input?
  bytes_sent_ = static_cast<int64_t>(bytes_sent);
  total_bytes_to_be_sent_ = static_cast<int64_t>(total_bytes_to_be_sent);
  UpdateStatus();
}

void PPB_URLLoader_Impl::didReceiveResponse(WebURLLoader* loader,
                                            const WebURLResponse& response) {
  SaveResponse(response);

  // Sets -1 if the content length is unknown.
  total_bytes_to_be_received_ = response.expectedContentLength();
  UpdateStatus();

  RunCallback(PP_OK);
}

void PPB_URLLoader_Impl::didDownloadData(WebURLLoader* loader,
                                         int data_length) {
  bytes_received_ += data_length;
  UpdateStatus();
}

void PPB_URLLoader_Impl::didReceiveData(WebURLLoader* loader,
                                        const char* data,
                                        int data_length,
                                        int encoded_data_length) {
  // Note that |loader| will be NULL for document loads.
  bytes_received_ += data_length;
  UpdateStatus();

  buffer_.insert(buffer_.end(), data, data + data_length);

  // To avoid letting the network stack download an entire stream all at once,
  // defer loading when we have enough buffer.
  // Check for this before we run the callback, even though that could move
  // data out of the buffer. Doing anything after the callback is unsafe.
  DCHECK(request_data_.prefetch_buffer_lower_threshold <
         request_data_.prefetch_buffer_upper_threshold);
  if (!is_streaming_to_file_ &&
      !is_asynchronous_load_suspended_ &&
      (buffer_.size() >= static_cast<size_t>(
          request_data_.prefetch_buffer_upper_threshold))) {
    DVLOG(1) << "Suspending async load - buffer size: " << buffer_.size();
    SetDefersLoading(true);
  }

  if (user_buffer_) {
    RunCallback(FillUserBuffer());
  } else {
    DCHECK(!TrackedCallback::IsPending(pending_callback_));
  }
}

void PPB_URLLoader_Impl::didFinishLoading(WebURLLoader* loader,
                                          double finish_time) {
  FinishLoading(PP_OK);
}

void PPB_URLLoader_Impl::didFail(WebURLLoader* loader,
                                 const WebURLError& error) {
  int32_t pp_error = PP_ERROR_FAILED;
  if (error.domain.equals(WebString::fromUTF8(net::kErrorDomain))) {
    // TODO(bbudge): Extend pp_errors.h to cover interesting network errors
    // from the net error domain.
    switch (error.reason) {
      case net::ERR_ABORTED:
        pp_error = PP_ERROR_ABORTED;
        break;
      case net::ERR_ACCESS_DENIED:
      case net::ERR_NETWORK_ACCESS_DENIED:
        pp_error = PP_ERROR_NOACCESS;
        break;
    }
  } else {
    // It's a WebKit error.
    pp_error = PP_ERROR_NOACCESS;
  }

  FinishLoading(pp_error);
}

void PPB_URLLoader_Impl::SetDefersLoading(bool defers_loading) {
  if (loader_.get()) {
    loader_->setDefersLoading(defers_loading);
    is_asynchronous_load_suspended_ = defers_loading;
  }

  // TODO(brettw) bug 96770: We need a way to set the defers loading flag on
  // main document loads (when the loader_ is null).
}

void PPB_URLLoader_Impl::FinishLoading(int32_t done_status) {
  done_status_ = done_status;
  user_buffer_ = NULL;
  user_buffer_size_ = 0;
  // If the client hasn't called any function that takes a callback since
  // the initial call to Open, or called ReadResponseBody and got a
  // synchronous return, then the callback will be NULL.
  if (TrackedCallback::IsPending(pending_callback_))
    RunCallback(done_status_);
}

int32_t PPB_URLLoader_Impl::ValidateCallback(
    scoped_refptr<TrackedCallback> callback) {
  DCHECK(callback);

  if (TrackedCallback::IsPending(pending_callback_))
    return PP_ERROR_INPROGRESS;

  return PP_OK;
}

void PPB_URLLoader_Impl::RegisterCallback(
    scoped_refptr<TrackedCallback> callback) {
  DCHECK(!TrackedCallback::IsPending(pending_callback_));

  PluginModule* plugin_module = ResourceHelper::GetPluginModule(this);
  if (!plugin_module)
    return;

  pending_callback_ = callback;
}

void PPB_URLLoader_Impl::RunCallback(int32_t result) {
  // This may be null only when this is a main document loader.
  if (!pending_callback_.get()) {
    CHECK(main_document_loader_);
    return;
  }

  // If |user_buffer_| was set as part of registering a callback, the paths
  // which trigger that callack must have cleared it since the callback is now
  // free to delete it.
  DCHECK(!user_buffer_);

  // As a second line of defense, clear the |user_buffer_| in case the
  // callbacks get called in an unexpected order.
  user_buffer_ = NULL;
  user_buffer_size_ = 0;
  pending_callback_->Run(result);
}

size_t PPB_URLLoader_Impl::FillUserBuffer() {
  DCHECK(user_buffer_);
  DCHECK(user_buffer_size_);

  size_t bytes_to_copy = std::min(buffer_.size(), user_buffer_size_);
  std::copy(buffer_.begin(), buffer_.begin() + bytes_to_copy, user_buffer_);
  buffer_.erase(buffer_.begin(), buffer_.begin() + bytes_to_copy);

  // If the buffer is getting too empty, resume asynchronous loading.
  if (is_asynchronous_load_suspended_ &&
      buffer_.size() <= static_cast<size_t>(
          request_data_.prefetch_buffer_lower_threshold)) {
    DVLOG(1) << "Resuming async load - buffer size: " << buffer_.size();
    SetDefersLoading(false);
  }

  // Reset for next time.
  user_buffer_ = NULL;
  user_buffer_size_ = 0;
  return bytes_to_copy;
}

void PPB_URLLoader_Impl::SaveResponse(const WebURLResponse& response) {
  // DataFromWebURLResponse returns a file ref with one reference to it, which
  // we take over via our ScopedPPResource.
  response_info_.reset(new ::ppapi::URLResponseInfoData(
      DataFromWebURLResponse(pp_instance(), response)));
  response_info_file_ref_ = ::ppapi::ScopedPPResource(
      ::ppapi::ScopedPPResource::PassRef(),
      response_info_->body_as_file_ref.resource.host_resource());
}

void PPB_URLLoader_Impl::UpdateStatus() {
  if (status_callback_ &&
      (RecordDownloadProgress() || RecordUploadProgress())) {
    // Here we go through some effort to only send the exact information that
    // the requestor wanted in the request flags. It would be just as
    // efficient to send all of it, but we don't want people to rely on
    // getting download progress when they happen to set the upload progress
    // flag.
    status_callback_(
        pp_instance(), pp_resource(),
        RecordUploadProgress() ? bytes_sent_ : -1,
        RecordUploadProgress() ?  total_bytes_to_be_sent_ : -1,
        RecordDownloadProgress() ? bytes_received_ : -1,
        RecordDownloadProgress() ? total_bytes_to_be_received_ : -1);
  }
}

bool PPB_URLLoader_Impl::RecordDownloadProgress() const {
  return request_data_.record_download_progress;
}

bool PPB_URLLoader_Impl::RecordUploadProgress() const {
  return request_data_.record_upload_progress;
}

}  // namespace ppapi
}  // namespace webkit
