// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_url_loader_impl.h"

#include "base/logging.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKitClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLLoader.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLResponse.h"
#include "webkit/appcache/web_application_cache_host_impl.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/plugin_module.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_url_request_info_impl.h"
#include "webkit/plugins/ppapi/ppb_url_response_info_impl.h"

using appcache::WebApplicationCacheHostImpl;
using WebKit::WebFrame;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebURLError;
using WebKit::WebURLLoader;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;

#ifdef _MSC_VER
// Do not warn about use of std::copy with raw pointers.
#pragma warning(disable : 4996)
#endif

namespace webkit {
namespace ppapi {

namespace {

PP_Resource Create(PP_Instance instance_id) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return 0;

  PPB_URLLoader_Impl* loader = new PPB_URLLoader_Impl(instance, false);
  return loader->GetReference();
}

PP_Bool IsURLLoader(PP_Resource resource) {
  return BoolToPPBool(!!Resource::GetAs<PPB_URLLoader_Impl>(resource));
}

int32_t Open(PP_Resource loader_id,
             PP_Resource request_id,
             PP_CompletionCallback callback) {
  scoped_refptr<PPB_URLLoader_Impl> loader(
      Resource::GetAs<PPB_URLLoader_Impl>(loader_id));
  if (!loader)
    return PP_ERROR_BADRESOURCE;

  scoped_refptr<PPB_URLRequestInfo_Impl> request(
      Resource::GetAs<PPB_URLRequestInfo_Impl>(request_id));
  if (!request)
    return PP_ERROR_BADRESOURCE;

  return loader->Open(request, callback);
}

int32_t FollowRedirect(PP_Resource loader_id,
                       PP_CompletionCallback callback) {
  scoped_refptr<PPB_URLLoader_Impl> loader(
      Resource::GetAs<PPB_URLLoader_Impl>(loader_id));
  if (!loader)
    return PP_ERROR_BADRESOURCE;

  return loader->FollowRedirect(callback);
}

PP_Bool GetUploadProgress(PP_Resource loader_id,
                          int64_t* bytes_sent,
                          int64_t* total_bytes_to_be_sent) {
  scoped_refptr<PPB_URLLoader_Impl> loader(
      Resource::GetAs<PPB_URLLoader_Impl>(loader_id));
  if (!loader)
    return PP_FALSE;

  return BoolToPPBool(loader->GetUploadProgress(bytes_sent,
                                                total_bytes_to_be_sent));
}

PP_Bool GetDownloadProgress(PP_Resource loader_id,
                            int64_t* bytes_received,
                            int64_t* total_bytes_to_be_received) {
  scoped_refptr<PPB_URLLoader_Impl> loader(
      Resource::GetAs<PPB_URLLoader_Impl>(loader_id));
  if (!loader)
    return PP_FALSE;

  return BoolToPPBool(loader->GetDownloadProgress(bytes_received,
                                                  total_bytes_to_be_received));
}

PP_Resource GetResponseInfo(PP_Resource loader_id) {
  scoped_refptr<PPB_URLLoader_Impl> loader(
      Resource::GetAs<PPB_URLLoader_Impl>(loader_id));
  if (!loader)
    return 0;

  PPB_URLResponseInfo_Impl* response_info = loader->response_info();
  if (!response_info)
    return 0;

  return response_info->GetReference();
}

int32_t ReadResponseBody(PP_Resource loader_id,
                         char* buffer,
                         int32_t bytes_to_read,
                         PP_CompletionCallback callback) {
  scoped_refptr<PPB_URLLoader_Impl> loader(
      Resource::GetAs<PPB_URLLoader_Impl>(loader_id));
  if (!loader)
    return PP_ERROR_BADRESOURCE;

  return loader->ReadResponseBody(buffer, bytes_to_read, callback);
}

int32_t FinishStreamingToFile(PP_Resource loader_id,
                              PP_CompletionCallback callback) {
  scoped_refptr<PPB_URLLoader_Impl> loader(
      Resource::GetAs<PPB_URLLoader_Impl>(loader_id));
  if (!loader)
    return PP_ERROR_BADRESOURCE;

  return loader->FinishStreamingToFile(callback);
}

void Close(PP_Resource loader_id) {
  scoped_refptr<PPB_URLLoader_Impl> loader(
      Resource::GetAs<PPB_URLLoader_Impl>(loader_id));
  if (!loader)
    return;

  loader->Close();
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

void GrantUniversalAccess(PP_Resource loader_id) {
  scoped_refptr<PPB_URLLoader_Impl> loader(
      Resource::GetAs<PPB_URLLoader_Impl>(loader_id));
  if (!loader)
    return;

  loader->GrantUniversalAccess();
}

void SetStatusCallback(PP_Resource loader_id,
                       PP_URLLoaderTrusted_StatusCallback cb) {
  scoped_refptr<PPB_URLLoader_Impl> loader(
      Resource::GetAs<PPB_URLLoader_Impl>(loader_id));
  if (!loader)
    return;
  loader->SetStatusCallback(cb);
}

const PPB_URLLoaderTrusted ppb_urlloadertrusted = {
  &GrantUniversalAccess,
  &SetStatusCallback
};

WebKit::WebFrame* GetFrame(PluginInstance* instance) {
  return instance->container()->element().document().frame();
}

}  // namespace

PPB_URLLoader_Impl::PPB_URLLoader_Impl(PluginInstance* instance,
                                       bool main_document_loader)
    : Resource(instance),
      main_document_loader_(main_document_loader),
      pending_callback_(),
      bytes_sent_(0),
      total_bytes_to_be_sent_(-1),
      bytes_received_(0),
      total_bytes_to_be_received_(-1),
      user_buffer_(NULL),
      user_buffer_size_(0),
      done_status_(PP_ERROR_WOULDBLOCK),
      has_universal_access_(false),
      status_callback_(NULL) {
}

PPB_URLLoader_Impl::~PPB_URLLoader_Impl() {
}

// static
const PPB_URLLoader* PPB_URLLoader_Impl::GetInterface() {
  return &ppb_urlloader;
}

// static
const PPB_URLLoaderTrusted* PPB_URLLoader_Impl::GetTrustedInterface() {
  return &ppb_urlloadertrusted;
}

PPB_URLLoader_Impl* PPB_URLLoader_Impl::AsPPB_URLLoader_Impl() {
  return this;
}

void PPB_URLLoader_Impl::LastPluginRefWasDeleted(bool instance_destroyed) {
  Resource::LastPluginRefWasDeleted(instance_destroyed);
  if (instance_destroyed) {
    // Free the WebKit request when the instance has been destroyed to avoid
    // using bandwidth just in case this object lives longer than the instance.
    loader_.reset();
  }
}

int32_t PPB_URLLoader_Impl::Open(PPB_URLRequestInfo_Impl* request,
                                 PP_CompletionCallback callback) {
  int32_t rv = ValidateCallback(callback);
  if (rv != PP_OK)
    return rv;

  if (loader_.get())
    return PP_ERROR_INPROGRESS;

  WebFrame* frame = GetFrame(instance());
  if (!frame)
    return PP_ERROR_FAILED;
  WebURLRequest web_request(request->ToWebURLRequest(frame));

  rv = CanRequest(frame, web_request.url());
  if (rv != PP_OK)
    return rv;

  loader_.reset(frame->createAssociatedURLLoader());
  if (!loader_.get())
    return PP_ERROR_FAILED;

  loader_->loadAsynchronously(web_request, this);

  request_info_ = scoped_refptr<PPB_URLRequestInfo_Impl>(request);

  // Notify completion when we receive a redirect or response headers.
  RegisterCallback(callback);
  return PP_ERROR_WOULDBLOCK;
}

int32_t PPB_URLLoader_Impl::FollowRedirect(PP_CompletionCallback callback) {
  int32_t rv = ValidateCallback(callback);
  if (rv != PP_OK)
    return rv;

  WebURL redirect_url = GURL(response_info_->redirect_url());

  rv = CanRequest(GetFrame(instance()), redirect_url);
  if (rv != PP_OK)
    return rv;

  loader_->setDefersLoading(false);  // Allow the redirect to continue.
  RegisterCallback(callback);
  return PP_ERROR_WOULDBLOCK;
}

bool PPB_URLLoader_Impl::GetUploadProgress(int64_t* bytes_sent,
                                           int64_t* total_bytes_to_be_sent) {
  if (!RecordUploadProgress()) {
    *bytes_sent = 0;
    *total_bytes_to_be_sent = 0;
    return false;
  }
  *bytes_sent = bytes_sent_;
  *total_bytes_to_be_sent = total_bytes_to_be_sent_;
  return true;
}

bool PPB_URLLoader_Impl::GetDownloadProgress(
    int64_t* bytes_received,
    int64_t* total_bytes_to_be_received) {
  if (!RecordDownloadProgress()) {
    *bytes_received = 0;
    *total_bytes_to_be_received = 0;
    return false;
  }
  *bytes_received = bytes_received_;
  *total_bytes_to_be_received = total_bytes_to_be_received_;
  return true;
}

int32_t PPB_URLLoader_Impl::ReadResponseBody(char* buffer,
                                             int32_t bytes_to_read,
                                             PP_CompletionCallback callback) {
  int32_t rv = ValidateCallback(callback);
  if (rv != PP_OK)
    return rv;
  if (!response_info_ || response_info_->body())
    return PP_ERROR_FAILED;
  if (bytes_to_read <= 0 || !buffer)
    return PP_ERROR_BADARGUMENT;

  user_buffer_ = buffer;
  user_buffer_size_ = bytes_to_read;

  if (!buffer_.empty())
    return FillUserBuffer();

  // We may have already reached EOF.
  if (done_status_ != PP_ERROR_WOULDBLOCK) {
    user_buffer_ = NULL;
    user_buffer_size_ = 0;
    return done_status_;
  }

  RegisterCallback(callback);
  return PP_ERROR_WOULDBLOCK;
}

int32_t PPB_URLLoader_Impl::FinishStreamingToFile(
    PP_CompletionCallback callback) {
  int32_t rv = ValidateCallback(callback);
  if (rv != PP_OK)
    return rv;
  if (!response_info_ || !response_info_->body())
    return PP_ERROR_FAILED;

  // We may have already reached EOF.
  if (done_status_ != PP_ERROR_WOULDBLOCK)
    return done_status_;

  // Wait for didFinishLoading / didFail.
  RegisterCallback(callback);
  return PP_ERROR_WOULDBLOCK;
}

void PPB_URLLoader_Impl::Close() {
  if (loader_.get()) {
    loader_->cancel();
  } else if (main_document_loader_) {
    WebFrame* frame = instance()->container()->element().document().frame();
    frame->stopLoading();
  }
  // TODO(viettrungluu): Check what happens to the callback (probably the
  // wrong thing). May need to post abort here. crbug.com/69457
}

void PPB_URLLoader_Impl::GrantUniversalAccess() {
  has_universal_access_ = true;
}

void PPB_URLLoader_Impl::SetStatusCallback(
    PP_URLLoaderTrusted_StatusCallback cb) {
  status_callback_ = cb;
}

void PPB_URLLoader_Impl::willSendRequest(
    WebURLLoader* loader,
    WebURLRequest& new_request,
    const WebURLResponse& redirect_response) {
  if (!request_info_->follow_redirects()) {
    SaveResponse(redirect_response);
    loader_->setDefersLoading(true);
    RunCallback(PP_OK);
  } else {
    int32_t rv = CanRequest(GetFrame(instance()), new_request.url());
    if (rv != PP_OK) {
      loader_->setDefersLoading(true);
      RunCallback(rv);
    }
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
                                        int data_length) {
  bytes_received_ += data_length;

  buffer_.insert(buffer_.end(), data, data + data_length);
  if (user_buffer_) {
    RunCallback(FillUserBuffer());
  } else {
    DCHECK(!pending_callback_.get() || pending_callback_->completed());
  }
}

void PPB_URLLoader_Impl::didFinishLoading(WebURLLoader* loader,
                                          double finish_time) {
  done_status_ = PP_OK;
  RunCallback(done_status_);
}

void PPB_URLLoader_Impl::didFail(WebURLLoader* loader,
                                 const WebURLError& error) {
  // TODO(darin): Provide more detailed error information.
  done_status_ = PP_ERROR_FAILED;
  RunCallback(done_status_);
}

int32_t PPB_URLLoader_Impl::ValidateCallback(PP_CompletionCallback callback) {
  // We only support non-blocking calls.
  if (!callback.func)
    return PP_ERROR_BADARGUMENT;

  if (pending_callback_.get() && !pending_callback_->completed())
    return PP_ERROR_INPROGRESS;

  return PP_OK;
}

void PPB_URLLoader_Impl::RegisterCallback(PP_CompletionCallback callback) {
  DCHECK(callback.func);
  DCHECK(!pending_callback_.get() || pending_callback_->completed());

  PP_Resource resource_id = GetReferenceNoAddRef();
  CHECK(resource_id);
  pending_callback_ = new TrackedCompletionCallback(
      instance()->module()->GetCallbackTracker(), resource_id, callback);
}

void PPB_URLLoader_Impl::RunCallback(int32_t result) {
  // This may be null only when this is a main document loader.
  if (!pending_callback_.get()) {
    // TODO(viettrungluu): put this CHECK back when the callback race condition
    // is fixed: http://code.google.com/p/chromium/issues/detail?id=70347.
    //CHECK(main_document_loader_);
    return;
  }

  scoped_refptr<TrackedCompletionCallback> callback;
  callback.swap(pending_callback_);
  callback->Run(result);  // Will complete abortively if necessary.
}

size_t PPB_URLLoader_Impl::FillUserBuffer() {
  DCHECK(user_buffer_);
  DCHECK(user_buffer_size_);

  size_t bytes_to_copy = std::min(buffer_.size(), user_buffer_size_);
  std::copy(buffer_.begin(), buffer_.begin() + bytes_to_copy, user_buffer_);
  buffer_.erase(buffer_.begin(), buffer_.begin() + bytes_to_copy);

  // Reset for next time.
  user_buffer_ = NULL;
  user_buffer_size_ = 0;
  return bytes_to_copy;
}

void PPB_URLLoader_Impl::SaveResponse(const WebKit::WebURLResponse& response) {
  scoped_refptr<PPB_URLResponseInfo_Impl> response_info(
      new PPB_URLResponseInfo_Impl(instance()));
  if (response_info->Initialize(response))
    response_info_ = response_info;
}

// Checks that the client can request the URL. Returns a PPAPI error code.
int32_t PPB_URLLoader_Impl::CanRequest(const WebKit::WebFrame* frame,
                              const WebKit::WebURL& url) {
  if (!has_universal_access_ &&
      !frame->securityOrigin().canRequest(url))
    return PP_ERROR_NOACCESS;

  return PP_OK;
}

void PPB_URLLoader_Impl::UpdateStatus() {
  if (status_callback_ &&
      (RecordDownloadProgress() || RecordUploadProgress())) {
    PP_Resource pp_resource = GetReferenceNoAddRef();
    if (pp_resource) {
      // The PP_Resource on the plugin will be NULL if the plugin has no
      // reference to this object. That's fine, because then we don't need to
      // call UpdateStatus.
      //
      // Here we go through some effort to only send the exact information that
      // the requestor wanted in the request flags. It would be just as
      // efficient to send all of it, but we don't want people to rely on
      // getting download progress when they happen to set the upload progress
      // flag.
      status_callback_(
          instance()->pp_instance(), pp_resource,
          RecordUploadProgress() ? bytes_sent_ : -1,
          RecordUploadProgress() ?  total_bytes_to_be_sent_ : -1,
          RecordDownloadProgress() ? bytes_received_ : -1,
          RecordDownloadProgress() ? total_bytes_to_be_received_ : -1);
    }
  }
}

bool PPB_URLLoader_Impl::RecordDownloadProgress() const {
  return request_info_ && request_info_->record_download_progress();
}

bool PPB_URLLoader_Impl::RecordUploadProgress() const {
  return request_info_ && request_info_->record_upload_progress();
}

}  // namespace ppapi
}  // namespace webkit
