// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_GLUE_PLUGINS_PEPPER_URL_LOADER_H_
#define WEBKIT_GLUE_PLUGINS_PEPPER_URL_LOADER_H_

#include "webkit/glue/plugins/pepper_resource.h"

typedef struct _pp_CompletionCallback PP_CompletionCallback;
typedef struct _ppb_URLLoader PPB_URLLoader;

namespace pepper {

class PluginInstance;
class URLRequestInfo;
class URLResponseInfo;

class URLLoader : public Resource {
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

  URLResponseInfo* response_info() const { return response_info_; }

  // Progress counters:
  int64_t bytes_sent() const { return bytes_sent_; }
  int64_t total_bytes_to_be_sent() const { return total_bytes_to_be_sent_; }
  int64_t bytes_received() const { return bytes_received_; }
  int64_t total_bytes_to_be_received() const {
    return total_bytes_to_be_received_;
  }

 private:
  scoped_refptr<URLResponseInfo> response_info_;
  int64_t bytes_sent_;
  int64_t total_bytes_to_be_sent_;
  int64_t bytes_received_;
  int64_t total_bytes_to_be_received_;
};

}  // namespace pepper

#endif  // WEBKIT_GLUE_PLUGINS_PEPPER_URL_LOADER_H_
