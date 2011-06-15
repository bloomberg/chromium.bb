// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_URL_REQUEST_INFO_H_
#define NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_URL_REQUEST_INFO_H_

#include <string>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_var_deprecated.h"
#include "native_client/tests/fake_browser_ppapi/fake_resource.h"
#include "ppapi/c/ppb_url_request_info.h"

namespace fake_browser_ppapi {

// Implements the PPB_URLRequestInfo interface.
class URLRequestInfo : public Resource {
 public:
  explicit URLRequestInfo(PP_Instance instance_id)
      : instance_id_(instance_id), stream_to_file_(false) {}

  URLRequestInfo* AsURLRequestInfo() { return this; }

  // Setters
  void set_url(const std::string& url) { url_ = url; }
  void set_method(const std::string& method) { method_ = method; }
  void set_headers(const std::string& headers) { headers_ = headers; }
  void set_stream_to_file(bool stream_to_file) {
    stream_to_file_ = stream_to_file;
  }

  // Getters
  PP_Instance instance_id() const { return instance_id_; }
  const std::string& url() const { return url_; }
  bool stream_to_file() { return stream_to_file_; }

  // Return an interface pointer usable by PPAPI.
  static const PPB_URLRequestInfo* GetInterface();

 private:
  PP_Instance instance_id_;
  std::string url_;
  std::string method_;
  std::string headers_;
  bool stream_to_file_;

  NACL_DISALLOW_COPY_AND_ASSIGN(URLRequestInfo);
};

}  // namespace fake_browser_ppapi

#endif  // NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_URL_REQUEST_INFO_H_
