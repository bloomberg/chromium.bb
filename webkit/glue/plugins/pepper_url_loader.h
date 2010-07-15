// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_URL_LOADER_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_URL_LOADER_H_

#include <deque>

#include "base/scoped_ptr.h"
#include "third_party/ppapi/c/pp_completion_callback.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLLoader.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURLLoaderClient.h"
#include "webkit/glue/plugins/pepper_resource.h"

typedef struct _ppb_URLLoader PPB_URLLoader;

namespace pepper {

class PluginInstance;
class URLRequestInfo;
class URLResponseInfo;

class URLLoader : public Resource, public WebKit::WebURLLoaderClient {
 public:
  explicit URLLoader(PluginInstance* instance);
  virtual ~URLLoader();

  // Returns a pointer to the interface implementing PPB_URLLoader that is
  // exposed to the plugin.
  static const PPB_URLLoader* GetInterface();

  // Resource overrides.
  URLLoader* AsURLLoader() { return this; }

  // PPB_URLLoader implementation.
  int32_t Open(URLRequestInfo* request, PP_CompletionCallback callback);
  int32_t FollowRedirect(PP_CompletionCallback callback);
  int32_t ReadResponseBody(char* buffer, int32_t bytes_to_read,
                           PP_CompletionCallback callback);
  void Close();

  // WebKit::WebURLLoaderClient implementation.
  virtual void willSendRequest(WebKit::WebURLLoader* loader,
                               WebKit::WebURLRequest& new_request,
                               const WebKit::WebURLResponse& redir_response);
  virtual void didSendData(WebKit::WebURLLoader* loader,
                           unsigned long long bytes_sent,
                           unsigned long long total_bytes_to_be_sent);
  virtual void didReceiveResponse(WebKit::WebURLLoader* loader,
                                  const WebKit::WebURLResponse& response);
  virtual void didReceiveData(WebKit::WebURLLoader* loader,
                              const char* data,
                              int data_length);
  virtual void didFinishLoading(WebKit::WebURLLoader* loader);
  virtual void didFail(WebKit::WebURLLoader* loader,
                       const WebKit::WebURLError& error);

  URLResponseInfo* response_info() const { return response_info_; }

  // Progress counters.
  int64_t bytes_sent() const { return bytes_sent_; }
  int64_t total_bytes_to_be_sent() const { return total_bytes_to_be_sent_; }
  int64_t bytes_received() const { return bytes_received_; }
  int64_t total_bytes_to_be_received() const {
    return total_bytes_to_be_received_;
  }

 private:
  void RunCallback(int32_t result);
  size_t FillUserBuffer();

  scoped_refptr<PluginInstance> instance_;
  scoped_ptr<WebKit::WebURLLoader> loader_;
  scoped_refptr<URLResponseInfo> response_info_;
  PP_CompletionCallback pending_callback_;
  std::deque<char> buffer_;
  int64_t bytes_sent_;
  int64_t total_bytes_to_be_sent_;
  int64_t bytes_received_;
  int64_t total_bytes_to_be_received_;
  char* user_buffer_;
  size_t user_buffer_size_;
  bool done_;
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_URL_LOADER_H_
