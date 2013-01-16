// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_URL_LOADER_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_URL_LOADER_IMPL_H_

#include <deque>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/scoped_pp_resource.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/shared_impl/url_request_info_data.h"
#include "ppapi/thunk/ppb_url_loader_api.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebURLLoaderClient.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

namespace WebKit {
class WebURL;
}

namespace webkit {
namespace ppapi {

class PPB_URLLoader_Impl : public ::ppapi::Resource,
                           public ::ppapi::thunk::PPB_URLLoader_API,
                           public WebKit::WebURLLoaderClient {
 public:
  PPB_URLLoader_Impl(PP_Instance instance, bool main_document_loader);
  virtual ~PPB_URLLoader_Impl();

  // Resource overrides.
  virtual ::ppapi::thunk::PPB_URLLoader_API* AsPPB_URLLoader_API() OVERRIDE;
  virtual void InstanceWasDeleted() OVERRIDE;

  // PPB_URLLoader_API implementation.
  virtual int32_t Open(
      PP_Resource request_id,
      scoped_refptr< ::ppapi::TrackedCallback> callback) OVERRIDE;
  virtual int32_t Open(
      const ::ppapi::URLRequestInfoData& data,
      int requestor_pid,
      scoped_refptr< ::ppapi::TrackedCallback> callback) OVERRIDE;
  virtual int32_t FollowRedirect(
      scoped_refptr< ::ppapi::TrackedCallback> callback) OVERRIDE;
  virtual PP_Bool GetUploadProgress(int64_t* bytes_sent,
                                    int64_t* total_bytes_to_be_sent) OVERRIDE;
  virtual PP_Bool GetDownloadProgress(
      int64_t* bytes_received,
      int64_t* total_bytes_to_be_received) OVERRIDE;
  virtual PP_Resource GetResponseInfo() OVERRIDE;
  virtual int32_t ReadResponseBody(
      void* buffer,
      int32_t bytes_to_read,
      scoped_refptr< ::ppapi::TrackedCallback> callback) OVERRIDE;
  virtual int32_t FinishStreamingToFile(
      scoped_refptr< ::ppapi::TrackedCallback> callback) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void GrantUniversalAccess() OVERRIDE;
  virtual void SetStatusCallback(
      PP_URLLoaderTrusted_StatusCallback cb) OVERRIDE;
  virtual bool GetResponseInfoData(
      ::ppapi::URLResponseInfoData* data) OVERRIDE;

  // WebKit::WebURLLoaderClient implementation.
  virtual void willSendRequest(WebKit::WebURLLoader* loader,
                               WebKit::WebURLRequest& new_request,
                               const WebKit::WebURLResponse& redir_response);
  virtual void didSendData(WebKit::WebURLLoader* loader,
                           unsigned long long bytes_sent,
                           unsigned long long total_bytes_to_be_sent);
  virtual void didReceiveResponse(WebKit::WebURLLoader* loader,
                                  const WebKit::WebURLResponse& response);
  virtual void didDownloadData(WebKit::WebURLLoader* loader,
                               int data_length);

  virtual void didReceiveData(WebKit::WebURLLoader* loader,
                               const char* data,
                               int data_length,
                               int encoded_data_length);
  virtual void didFinishLoading(WebKit::WebURLLoader* loader,
                                double finish_time);
  virtual void didFail(WebKit::WebURLLoader* loader,
                       const WebKit::WebURLError& error);

  // Returns the number of bytes currently available for synchronous reading
  // in the loader.
  int32_t buffer_size() const { return buffer_.size(); }

 private:
  // Check that |callback| is valid (only non-blocking operation is supported)
  // and that no callback is already pending. Returns |PP_OK| if okay, else
  // |PP_ERROR_...| to be returned to the plugin.
  int32_t ValidateCallback(scoped_refptr< ::ppapi::TrackedCallback> callback);

  // Sets up |callback| as the pending callback. This should only be called once
  // it is certain that |PP_OK_COMPLETIONPENDING| will be returned.
  void RegisterCallback(scoped_refptr< ::ppapi::TrackedCallback> callback);

  void RunCallback(int32_t result);

  size_t FillUserBuffer();

  // Converts a WebURLResponse to a URLResponseInfo and saves it.
  void SaveResponse(const WebKit::WebURLResponse& response);

  // Calls the status_callback_ (if any) with the current upload and download
  // progress. Call this function if you update any of these values to
  // synchronize an out-of-process plugin's state.
  void UpdateStatus();

  // Returns true if the plugin has requested we record download or upload
  // progress. When false, we don't need to update the counters. We go out of
  // our way not to allow access to this information unless it's requested,
  // even when it would be easier just to return it and not check, so that
  // plugins don't depend on access without setting the flag.
  bool RecordDownloadProgress() const;
  bool RecordUploadProgress() const;

  // Calls SetDefersLoading on the current load. This encapsulates the logic
  // differences between document loads and regular ones.
  void SetDefersLoading(bool defers_loading);

  void FinishLoading(int32_t done_status);

  // If true, then the plugin instance is a full-frame plugin and we're just
  // wrapping the main document's loader (i.e. loader_ is null).
  bool main_document_loader_;

  // Keep a copy of the request data. We specifically do this instead of
  // keeping a reference to the request resource, because the plugin might
  // change the request info resource out from under us.
  ::ppapi::URLRequestInfoData request_data_;

  // The loader associated with this request. MAY BE NULL.
  //
  // This will be NULL if the load hasn't been opened yet, or if this is a main
  // document loader (when registered as a mime type). Therefore, you should
  // always NULL check this value before using it. In the case of a main
  // document load, you would call the functions on the document to cancel the
  // load, etc. since there is no loader.
  scoped_ptr<WebKit::WebURLLoader> loader_;

  scoped_refptr< ::ppapi::TrackedCallback> pending_callback_;
  std::deque<char> buffer_;
  int64_t bytes_sent_;
  int64_t total_bytes_to_be_sent_;
  int64_t bytes_received_;
  int64_t total_bytes_to_be_received_;
  char* user_buffer_;
  size_t user_buffer_size_;
  int32_t done_status_;
  bool is_streaming_to_file_;
  bool is_asynchronous_load_suspended_;

  bool has_universal_access_;

  PP_URLLoaderTrusted_StatusCallback status_callback_;

  // When the response info is received, this stores the data. The
  // ScopedResource maintains the reference to the file ref (if any) in the
  // data object so we don't forget to dereference it.
  scoped_ptr< ::ppapi::URLResponseInfoData > response_info_;
  ::ppapi::ScopedPPResource response_info_file_ref_;

  DISALLOW_COPY_AND_ASSIGN(PPB_URLLoader_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_URL_LOADER_IMPL_H_
