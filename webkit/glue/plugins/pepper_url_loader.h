// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_URL_LOADER_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_URL_LOADER_H_

#include <deque>

#include "base/scoped_ptr.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/trusted/ppb_url_loader_trusted.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLLoaderClient.h"
#include "webkit/glue/plugins/pepper_plugin_instance.h"
#include "webkit/glue/plugins/pepper_resource.h"

struct PPB_URLLoader;
struct PPB_URLLoaderTrusted;

namespace WebKit {
class WebFrame;
class WebURL;
}

namespace pepper {

class PluginInstance;
class URLRequestInfo;
class URLResponseInfo;

class URLLoader : public Resource,
                  public WebKit::WebURLLoaderClient,
                  public PluginInstance::Observer {
 public:
  URLLoader(PluginInstance* instance, bool main_document_loader);
  virtual ~URLLoader();

  // Returns a pointer to the interface implementing PPB_URLLoader that is
  // exposed to the plugin.
  static const PPB_URLLoader* GetInterface();

  // Returns a pointer to the interface implementing PPB_URLLoaderTrusted that
  // is exposed to the plugin.
  static const PPB_URLLoaderTrusted* GetTrustedInterface();

  // Resource overrides.
  URLLoader* AsURLLoader() { return this; }

  // PPB_URLLoader implementation.
  int32_t Open(URLRequestInfo* request, PP_CompletionCallback callback);
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

  // PluginInstance::Observer implementation.
  void InstanceDestroyed(PluginInstance* instance);

  URLResponseInfo* response_info() const { return response_info_; }

 private:
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

  // This will be NULL if the instance has been deleted but this URLLoader was
  // somehow leaked. In general, you should not need to check this for NULL.
  // However, if you see a NULL pointer crash, that means somebody is holding
  // a reference to this object longer than the PluginInstance's lifetime.
  PluginInstance* instance_;

  // If true, then the plugin instance is a full-frame plugin and we're just
  // wrapping the main document's loader (i.e. loader_ is null).
  bool main_document_loader_;
  scoped_ptr<WebKit::WebURLLoader> loader_;
  scoped_refptr<URLRequestInfo> request_info_;
  scoped_refptr<URLResponseInfo> response_info_;
  PP_CompletionCallback pending_callback_;
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
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_URL_LOADER_H_
