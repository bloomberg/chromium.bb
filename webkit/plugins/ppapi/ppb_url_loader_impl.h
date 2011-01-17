// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_URL_LOADER_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_URL_LOADER_IMPL_H_

#include <deque>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLLoaderClient.h"
#include "webkit/plugins/ppapi/callbacks.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/resource.h"

struct PPB_URLLoader;
struct PPB_URLLoaderTrusted;

namespace WebKit {
class WebFrame;
class WebURL;
}

namespace webkit {
namespace ppapi {

class PluginInstance;
class PPB_URLRequestInfo_Impl;
class PPB_URLResponseInfo_Impl;

class PPB_URLLoader_Impl : public Resource, public WebKit::WebURLLoaderClient {
 public:
  PPB_URLLoader_Impl(PluginInstance* instance, bool main_document_loader);
  virtual ~PPB_URLLoader_Impl();

  // Returns a pointer to the interface implementing PPB_URLLoader that is
  // exposed to the plugin.
  static const PPB_URLLoader* GetInterface();

  // Returns a pointer to the interface implementing PPB_URLLoaderTrusted that
  // is exposed to the plugin.
  static const PPB_URLLoaderTrusted* GetTrustedInterface();

  // Resource overrides.
  virtual PPB_URLLoader_Impl* AsPPB_URLLoader_Impl();
  virtual void LastPluginRefWasDeleted(bool instance_destroyed);

  // PPB_URLLoader implementation.
  int32_t Open(PPB_URLRequestInfo_Impl* request,
               PP_CompletionCallback callback);
  int32_t FollowRedirect(PP_CompletionCallback callback);
  bool GetUploadProgress(int64_t* bytes_sent,
                         int64_t* total_bytes_to_be_sent);
  bool GetDownloadProgress(int64_t* bytes_received,
                           int64_t* total_bytes_to_be_received);
  int32_t ReadResponseBody(char* buffer, int32_t bytes_to_read,
                           PP_CompletionCallback callback);
  int32_t FinishStreamingToFile(PP_CompletionCallback callback);
  void Close();

  // PPB_URLLoaderTrusted implementation.
  void GrantUniversalAccess();
  void SetStatusCallback(PP_URLLoaderTrusted_StatusCallback cb);

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
                              int data_length);
  virtual void didFinishLoading(WebKit::WebURLLoader* loader,
                                double finish_time);
  virtual void didFail(WebKit::WebURLLoader* loader,
                       const WebKit::WebURLError& error);

  PPB_URLResponseInfo_Impl* response_info() const { return response_info_; }

 private:
  // Check that |callback| is valid (only non-blocking operation is supported)
  // and that no callback is already pending. Returns |PP_OK| if okay, else
  // |PP_ERROR_...| to be returned to the plugin.
  int32_t ValidateCallback(PP_CompletionCallback callback);

  // Sets up |callback| as the pending callback. This should only be called once
  // it is certain that |PP_ERROR_WOULDBLOCK| will be returned.
  void RegisterCallback(PP_CompletionCallback callback);

  void RunCallback(int32_t result);

  size_t FillUserBuffer();

  // Converts a WebURLResponse to a URLResponseInfo and saves it.
  void SaveResponse(const WebKit::WebURLResponse& response);

  int32_t CanRequest(const WebKit::WebFrame* frame, const WebKit::WebURL& url);

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

  // If true, then the plugin instance is a full-frame plugin and we're just
  // wrapping the main document's loader (i.e. loader_ is null).
  bool main_document_loader_;
  scoped_ptr<WebKit::WebURLLoader> loader_;
  scoped_refptr<PPB_URLRequestInfo_Impl> request_info_;
  scoped_refptr<PPB_URLResponseInfo_Impl> response_info_;
  scoped_refptr<TrackedCompletionCallback> pending_callback_;
  std::deque<char> buffer_;
  int64_t bytes_sent_;
  int64_t total_bytes_to_be_sent_;
  int64_t bytes_received_;
  int64_t total_bytes_to_be_received_;
  char* user_buffer_;
  size_t user_buffer_size_;
  int32_t done_status_;

  bool has_universal_access_;

  PP_URLLoaderTrusted_StatusCallback status_callback_;

  DISALLOW_COPY_AND_ASSIGN(PPB_URLLoader_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_URL_LOADER_IMPL_H_
