// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_URL_LOADER_H_
#define PPAPI_CPP_URL_LOADER_H_

#include "ppapi/c/pp_stdint.h"
#include "ppapi/cpp/resource.h"

namespace pp {

class CompletionCallback;
class Instance;
class URLRequestInfo;
class URLResponseInfo;

// URLLoader provides an API to download URLs.
//
// Please see the example in ppapi/examples/url_loader/url_loader.cc.
class URLLoader : public Resource {
 public:
  // Creates an is_null() URLLoader object.
  URLLoader() {}

  // TODO(brettw) remove this when NaCl is updated to use the new version
  // that takes a pointer.
  explicit URLLoader(const Instance& instance);

  explicit URLLoader(PP_Resource resource);
  explicit URLLoader(Instance* instance);
  URLLoader(const URLLoader& other);

  // PPB_URLLoader methods:
  int32_t Open(const URLRequestInfo& request_info,
               const CompletionCallback& cc);
  int32_t FollowRedirect(const CompletionCallback& cc);
  bool GetUploadProgress(int64_t* bytes_sent,
                         int64_t* total_bytes_to_be_sent) const;
  bool GetDownloadProgress(int64_t* bytes_received,
                           int64_t* total_bytes_to_be_received) const;
  URLResponseInfo GetResponseInfo() const;
  int32_t ReadResponseBody(void* buffer,
                           int32_t bytes_to_read,
                           const CompletionCallback& cc);
  int32_t FinishStreamingToFile(const CompletionCallback& cc);
  void Close();
};

}  // namespace pp

#endif  // PPAPI_CPP_URL_LOADER_H_
