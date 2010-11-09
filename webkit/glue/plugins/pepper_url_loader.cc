// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/pepper_url_loader.h"

#include "base/logging.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/dev/ppb_url_loader_dev.h"
#include "ppapi/c/dev/ppb_url_loader_trusted_dev.h"
#include "third_party/WebKit/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/WebKit/chromium/public/WebElement.h"
#include "third_party/WebKit/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/WebKit/chromium/public/WebKitClient.h"
#include "third_party/WebKit/WebKit/chromium/public/WebPluginContainer.h"
#include "third_party/WebKit/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLLoader.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLResponse.h"
#include "webkit/appcache/web_application_cache_host_impl.h"
#include "webkit/glue/plugins/pepper_common.h"
#include "webkit/glue/plugins/pepper_plugin_instance.h"
#include "webkit/glue/plugins/pepper_url_request_info.h"
#include "webkit/glue/plugins/pepper_url_response_info.h"

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

namespace pepper {

namespace {

PP_Resource Create(PP_Instance instance_id) {
  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return 0;

  URLLoader* loader = new URLLoader(instance, false);
  return loader->GetReference();
}

PP_Bool IsURLLoader(PP_Resource resource) {
  return BoolToPPBool(!!Resource::GetAs<URLLoader>(resource));
}

int32_t Open(PP_Resource loader_id,
             PP_Resource request_id,
             PP_CompletionCallback callback) {
  scoped_refptr<URLLoader> loader(Resource::GetAs<URLLoader>(loader_id));
  if (!loader)
    return PP_ERROR_BADRESOURCE;

  scoped_refptr<URLRequestInfo> request(
      Resource::GetAs<URLRequestInfo>(request_id));
  if (!request)
    return PP_ERROR_BADRESOURCE;

  return loader->Open(request, callback);
}

int32_t FollowRedirect(PP_Resource loader_id,
                       PP_CompletionCallback callback) {
  scoped_refptr<URLLoader> loader(Resource::GetAs<URLLoader>(loader_id));
  if (!loader)
    return PP_ERROR_BADRESOURCE;

  return loader->FollowRedirect(callback);
}

PP_Bool GetUploadProgress(PP_Resource loader_id,
                       int64_t* bytes_sent,
                       int64_t* total_bytes_to_be_sent) {
  scoped_refptr<URLLoader> loader(Resource::GetAs<URLLoader>(loader_id));
  if (!loader)
    return PP_FALSE;

  return BoolToPPBool(loader->GetUploadProgress(bytes_sent,
                                                total_bytes_to_be_sent));
}

PP_Bool GetDownloadProgress(PP_Resource loader_id,
                            int64_t* bytes_received,
                            int64_t* total_bytes_to_be_received) {
  scoped_refptr<URLLoader> loader(Resource::GetAs<URLLoader>(loader_id));
  if (!loader)
    return PP_FALSE;

  return BoolToPPBool(loader->GetDownloadProgress(bytes_received,
                                                  total_bytes_to_be_received));
}

PP_Resource GetResponseInfo(PP_Resource loader_id) {
  scoped_refptr<URLLoader> loader(Resource::GetAs<URLLoader>(loader_id));
  if (!loader)
    return 0;

  URLResponseInfo* response_info = loader->response_info();
  if (!response_info)
    return 0;

  return response_info->GetReference();
}

int32_t ReadResponseBody(PP_Resource loader_id,
                         char* buffer,
                         int32_t bytes_to_read,
                         PP_CompletionCallback callback) {
  scoped_refptr<URLLoader> loader(Resource::GetAs<URLLoader>(loader_id));
  if (!loader)
    return PP_ERROR_BADRESOURCE;

  return loader->ReadResponseBody(buffer, bytes_to_read, callback);
}

int32_t FinishStreamingToFile(PP_Resource loader_id,
                              PP_CompletionCallback callback) {
  scoped_refptr<URLLoader> loader(Resource::GetAs<URLLoader>(loader_id));
  if (!loader)
    return PP_ERROR_BADRESOURCE;

  return loader->FinishStreamingToFile(callback);
}

void Close(PP_Resource loader_id) {
  scoped_refptr<URLLoader> loader(Resource::GetAs<URLLoader>(loader_id));
  if (!loader)
    return;

  loader->Close();
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

void GrantUniversalAccess(PP_Resource loader_id) {
  scoped_refptr<URLLoader> loader(Resource::GetAs<URLLoader>(loader_id));
  if (!loader)
    return;

  loader->GrantUniversalAccess();
}

void SetStatusCallback(PP_Resource loader_id,
                       PP_URLLoaderTrusted_StatusCallback cb) {
  scoped_refptr<URLLoader> loader(Resource::GetAs<URLLoader>(loader_id));
  if (!loader)
    return;
  loader->SetStatusCallback(cb);
}

const PPB_URLLoaderTrusted_Dev ppb_urlloadertrusted = {
  &GrantUniversalAccess,
  &SetStatusCallback
};

WebKit::WebFrame* GetFrame(PluginInstance* instance) {
  return instance->container()->element().document().frame();
}

}  // namespace

URLLoader::URLLoader(PluginInstance* instance, bool main_document_loader)
    : Resource(instance->module()),
      instance_(instance),
      main_document_loader_(main_document_loader),
      pending_callback_(),
      bytes_sent_(0),
      total_bytes_to_be_sent_(-1),
      bytes_received_(0),
      total_bytes_to_be_received_(-1),
      user_buffer_(NULL),
      user_buffer_size_(0),
      done_status_(PP_ERROR_WOULDBLOCK),
      record_download_progress_(false),
      record_upload_progress_(false),
      has_universal_access_(false),
      status_callback_(NULL) {
  instance->AddObserver(this);
}

URLLoader::~URLLoader() {
  if (instance_)
    instance_->RemoveObserver(this);
}

// static
const PPB_URLLoader_Dev* URLLoader::GetInterface() {
  return &ppb_urlloader;
}

// static
const PPB_URLLoaderTrusted_Dev* URLLoader::GetTrustedInterface() {
  return &ppb_urlloadertrusted;
}

int32_t URLLoader::Open(URLRequestInfo* request,
                        PP_CompletionCallback callback) {
  if (loader_.get())
    return PP_ERROR_INPROGRESS;

  // We only support non-blocking calls.
  if (!callback.func)
    return PP_ERROR_BADARGUMENT;

  WebFrame* frame = GetFrame(instance_);
  if (!frame)
    return PP_ERROR_FAILED;
  WebURLRequest web_request(request->ToWebURLRequest(frame));

  int32_t rv = CanRequest(frame, web_request.url());
  if (rv != PP_OK)
    return rv;

  frame->dispatchWillSendRequest(web_request);

  // Sets the appcache host id to allow retrieval from the appcache.
  if (WebApplicationCacheHostImpl* appcache_host =
          WebApplicationCacheHostImpl::FromFrame(frame)) {
    appcache_host->willStartSubResourceRequest(web_request);
  }

  loader_.reset(WebKit::webKitClient()->createURLLoader());
  if (!loader_.get())
    return PP_ERROR_FAILED;

  loader_->loadAsynchronously(web_request, this);

  request_info_ = scoped_refptr<URLRequestInfo>(request);
  pending_callback_ = callback;
  record_download_progress_ = request->record_download_progress();
  record_upload_progress_  = request->record_upload_progress();

  // Notify completion when we receive a redirect or response headers.
  return PP_ERROR_WOULDBLOCK;
}

int32_t URLLoader::FollowRedirect(PP_CompletionCallback callback) {
  if (pending_callback_.func)
    return PP_ERROR_INPROGRESS;

  // We only support non-blocking calls.
  if (!callback.func)
    return PP_ERROR_BADARGUMENT;

  WebURL redirect_url = GURL(response_info_->redirect_url());

  int32_t rv = CanRequest(GetFrame(instance_), redirect_url);
  if (rv != PP_OK)
    return rv;

  pending_callback_ = callback;
  loader_->setDefersLoading(false);  // Allow the redirect to continue.
  return PP_ERROR_WOULDBLOCK;
}

bool URLLoader::GetUploadProgress(int64_t* bytes_sent,
                                  int64_t* total_bytes_to_be_sent) {
  if (!record_upload_progress_) {
    *bytes_sent = 0;
    *total_bytes_to_be_sent = 0;
    return false;
  }
  *bytes_sent = bytes_sent_;
  *total_bytes_to_be_sent = total_bytes_to_be_sent_;
  return true;
}

bool URLLoader::GetDownloadProgress(int64_t* bytes_received,
                                    int64_t* total_bytes_to_be_received) {
  if (!record_download_progress_) {
    *bytes_received = 0;
    *total_bytes_to_be_received = 0;
    return false;
  }
  *bytes_received = bytes_received_;
  *total_bytes_to_be_received = total_bytes_to_be_received_;
  return true;
}

int32_t URLLoader::ReadResponseBody(char* buffer, int32_t bytes_to_read,
                                    PP_CompletionCallback callback) {
  if (!response_info_ || response_info_->body())
    return PP_ERROR_FAILED;
  if (bytes_to_read <= 0 || !buffer)
    return PP_ERROR_BADARGUMENT;
  if (pending_callback_.func)
    return PP_ERROR_INPROGRESS;

  // We only support non-blocking calls.
  if (!callback.func)
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

  pending_callback_ = callback;
  return PP_ERROR_WOULDBLOCK;
}

int32_t URLLoader::FinishStreamingToFile(PP_CompletionCallback callback) {
  if (!response_info_ || !response_info_->body())
    return PP_ERROR_FAILED;
  if (pending_callback_.func)
    return PP_ERROR_INPROGRESS;

  // We may have already reached EOF.
  if (done_status_ != PP_ERROR_WOULDBLOCK)
    return done_status_;

  // Wait for didFinishLoading / didFail.
  pending_callback_ = callback;
  return PP_ERROR_WOULDBLOCK;
}

void URLLoader::Close() {
  if (loader_.get()) {
    loader_->cancel();
  } else if (main_document_loader_) {
    WebFrame* frame = instance_->container()->element().document().frame();
    frame->stopLoading();
  }
}

void URLLoader::GrantUniversalAccess() {
  has_universal_access_ = true;
}

void URLLoader::SetStatusCallback(PP_URLLoaderTrusted_StatusCallback cb) {
  status_callback_ = cb;
}

void URLLoader::willSendRequest(WebURLLoader* loader,
                                WebURLRequest& new_request,
                                const WebURLResponse& redirect_response) {
  if (!request_info_->follow_redirects()) {
    SaveResponse(redirect_response);
    loader_->setDefersLoading(true);
    RunCallback(PP_OK);
  } else {
    int32_t rv = CanRequest(GetFrame(instance_), new_request.url());
    if (rv != PP_OK) {
      loader_->setDefersLoading(true);
      RunCallback(rv);
    }
  }
}

void URLLoader::didSendData(WebURLLoader* loader,
                            unsigned long long bytes_sent,
                            unsigned long long total_bytes_to_be_sent) {
  // TODO(darin): Bounds check input?
  bytes_sent_ = static_cast<int64_t>(bytes_sent);
  total_bytes_to_be_sent_ = static_cast<int64_t>(total_bytes_to_be_sent);
  UpdateStatus();
}

void URLLoader::didReceiveResponse(WebURLLoader* loader,
                                   const WebURLResponse& response) {
  SaveResponse(response);

  // Sets -1 if the content length is unknown.
  total_bytes_to_be_received_ = response.expectedContentLength();
  UpdateStatus();

  RunCallback(PP_OK);
}

void URLLoader::didDownloadData(WebURLLoader* loader,
                                int data_length) {
  bytes_received_ += data_length;
  UpdateStatus();
}

void URLLoader::didReceiveData(WebURLLoader* loader,
                               const char* data,
                               int data_length) {
  bytes_received_ += data_length;

  buffer_.insert(buffer_.end(), data, data + data_length);
  if (user_buffer_) {
    RunCallback(FillUserBuffer());
  } else {
    DCHECK(!pending_callback_.func);
  }
}

void URLLoader::didFinishLoading(WebURLLoader* loader, double finish_time) {
  done_status_ = PP_OK;
  RunCallback(done_status_);
}

void URLLoader::didFail(WebURLLoader* loader, const WebURLError& error) {
  // TODO(darin): Provide more detailed error information.
  done_status_ = PP_ERROR_FAILED;
  RunCallback(done_status_);
}

void URLLoader::InstanceDestroyed(PluginInstance* instance) {
  // When the instance is destroyed, we force delete any associated loads.
  DCHECK(instance == instance_);
  instance_ = NULL;

  // Normally the only ref to this class will be from the plugin which
  // ForceDeletePluginResourceRefs will free. We don't want our object to be
  // deleted out from under us until the function completes.
  scoped_refptr<URLLoader> death_grip(this);

  // Force delete any plugin refs to us. If the instance is being deleted, we
  // don't want to allow the requests to continue to use bandwidth and send us
  // callbacks (for which we might have no plugin).
  ResourceTracker *tracker = ResourceTracker::Get();
  PP_Resource loader_resource = GetReferenceNoAddRef();
  if (loader_resource)
    tracker->ForceDeletePluginResourceRefs(loader_resource);

  // Also force free the response from the plugin, both the plugin's ref(s)
  // and ours.
  if (response_info_.get()) {
    PP_Resource response_info_resource = response_info_->GetReferenceNoAddRef();
    if (response_info_resource)
      tracker->ForceDeletePluginResourceRefs(response_info_resource);
    response_info_ = NULL;
  }

  // Free the WebKit request.
  loader_.reset();

  // Often, |this| will be deleted at the end of this function when death_grip
  // goes out of scope.
}

void URLLoader::RunCallback(int32_t result) {
  if (!pending_callback_.func)
    return;

  PP_CompletionCallback callback = {0};
  std::swap(callback, pending_callback_);
  PP_RunCompletionCallback(&callback, result);
}

size_t URLLoader::FillUserBuffer() {
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

void URLLoader::SaveResponse(const WebKit::WebURLResponse& response) {
  scoped_refptr<URLResponseInfo> response_info(new URLResponseInfo(module()));
  if (response_info->Initialize(response))
    response_info_ = response_info;
}

// Checks that the client can request the URL. Returns a PPAPI error code.
int32_t URLLoader::CanRequest(const WebKit::WebFrame* frame,
                              const WebKit::WebURL& url) {
  if (!has_universal_access_ &&
      !frame->securityOrigin().canRequest(url))
    return PP_ERROR_NOACCESS;

  return PP_OK;
}

void URLLoader::UpdateStatus() {
  if (status_callback_ &&
      (record_download_progress_ || record_upload_progress_)) {
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
          instance_->pp_instance(), pp_resource,
          record_upload_progress_ ? bytes_sent_ : -1,
          record_upload_progress_ ? total_bytes_to_be_sent_ : -1,
          record_download_progress_ ? bytes_received_ : -1,
          record_download_progress_ ? total_bytes_to_be_received_ : -1);
    }
  }
}

}  // namespace pepper
