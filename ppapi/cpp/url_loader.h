// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
// EXAMPLE USAGE:
//
//   class MyHandler {
//    public:
//     MyHandler(const Instance& instance)
//         : factory_(this),
//           loader_(instance),
//           did_open_(false) {
//     }
//     void ProcessURL(const char* url) {
//       CompletionCallback* cc = NewCallback();
//       int32_t rv = loader_.Open(MakeRequest(url), cc);
//       if (rv != PP_Error_WouldBlock)
//         cc->Run(rv);
//     }
//    private:
//     CompletionCallback* NewCallback() {
//       return factory_.NewCallback(&MyHandler::DidCompleteIO);
//     }
//     URLRequestInfo MakeRequest(const char* url) {
//       URLRequestInfo request;
//       request.SetURL(url);
//       request.SetMethod("GET");
//       request.SetFollowRedirects(true);
//       return request;
//     }
//     void DidCompleteIO(int32_t result) {
//       if (result > 0) {
//         // buf_ now contains 'result' number of bytes from the URL.
//         ProcessBytes(buf_, result);
//         ReadMore();
//       } else if (result == PP_OK && !did_open_) {
//         // Headers are available, and we can start reading the body.
//         did_open_ = true;
//         ProcessResponseInfo(loader_.GetResponseInfo());
//         ReadMore();
//       } else {
//         // Done reading (possibly with an error given by 'result').
//       }
//     }
//     void ReadMore() {
//       CompletionCallback* cc = NewCallback();
//       int32_t rv = fio_.Read(offset_, buf_, sizeof(buf_), cc);
//       if (rv != PP_Error_WouldBlock)
//         cc->Run(rv);
//     }
//     void ProcessResponseInfo(const URLResponseInfo& response_info) {
//       // Read response headers, etc.
//     }
//     void ProcessBytes(const char* bytes, int32_t length) {
//       // Do work ...
//     }
//     pp::CompletionCallbackFactory<MyHandler> factory_;
//     pp::URLLoader loader_;
//     char buf_[4096];
//     bool did_open_;
//   };
//
class URLLoader : public Resource {
 public:
  // Creates an is_null() URLLoader object.
  URLLoader() {}

  explicit URLLoader(PP_Resource resource);
  explicit URLLoader(const Instance& instance);
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
  int32_t ReadResponseBody(char* buffer,
                           int32_t bytes_to_read,
                           const CompletionCallback& cc);
  int32_t FinishStreamingToFile(const CompletionCallback& cc);
  void Close();
};

}  // namespace pp

#endif  // PPAPI_CPP_URL_LOADER_H_
