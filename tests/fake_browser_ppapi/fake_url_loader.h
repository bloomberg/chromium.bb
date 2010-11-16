/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_URL_LOADER_H_
#define NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_URL_LOADER_H_

#include <string>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/tests/fake_browser_ppapi/fake_resource.h"
#include "ppapi/c/ppb_url_loader.h"

namespace fake_browser_ppapi {

// Must be set by main.cc to fake-map a url to a local path.
extern std::string g_nacl_ppapi_url_path;
extern std::string g_nacl_ppapi_local_path;

// Implements the PPB_URLLoader interface.
class URLLoader : public Resource {
 public:
  explicit URLLoader(PP_Instance instance_id)
      : instance_id_(instance_id), response_(NULL) {}

  URLLoader* AsURLLoader() { return this; }

  URLRequestInfo* request() const { return request_; }
  URLResponseInfo* response() const { return response_; }
  void set_request(URLRequestInfo* request) { request_ = request; }
  void set_response(URLResponseInfo* response) { response_ = response; }

  // Return an interface pointer usable by PPAPI.
  static const PPB_URLLoader* GetInterface();

 private:
  PP_Instance instance_id_;
  URLRequestInfo* request_;
  URLResponseInfo* response_;

  NACL_DISALLOW_COPY_AND_ASSIGN(URLLoader);
};

}  // namespace fake_browser_ppapi

#endif  // NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_URL_LOADER_H_
