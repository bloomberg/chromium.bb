/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_URL_RESPONSE_INFO_H_
#define NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_URL_RESPONSE_INFO_H_

#include <string>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/tests/fake_browser_ppapi/fake_resource.h"

#include "ppapi/c/pp_module.h"
#include "ppapi/c/ppb_url_response_info.h"

namespace fake_browser_ppapi {

// Implements the PPB_URLResponse_Info interface.
class URLResponseInfo : public Resource {
 public:
  explicit URLResponseInfo(PP_Module module_id)
      : module_id_(module_id), body_as_file_ref_(NULL) {}

  URLResponseInfo* AsURLResponseInfo() { return this; }

  // Setters.
  void set_url(const std::string& url) { url_ = url; }
  void set_status_code(int32_t status_code) { status_code_ = status_code; }
  void set_body_as_file_ref(FileRef* file_ref) {
    body_as_file_ref_ = file_ref;
  }

  // Getters.
  PP_Module module_id() const { return module_id_; }
  const std::string& url() const { return url_; }
  int32_t status_code() { return status_code_; }
  FileRef* body_as_file_ref() const { return body_as_file_ref_; }

  // Return an interface pointer usable by PPAPI.
  static const PPB_URLResponseInfo* GetInterface();

 private:
  PP_Module module_id_;
  std::string url_;
  int32_t status_code_;  // Http status code.
  FileRef* body_as_file_ref_;

  NACL_DISALLOW_COPY_AND_ASSIGN(URLResponseInfo);
};

}  // namespace fake_browser_ppapi

#endif  // NATIVE_CLIENT_TESTS_FAKE_BROWSER_PPAPI_FAKE_URL_RESPONSE_INFO_H_
