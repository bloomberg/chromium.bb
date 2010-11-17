// Copyright 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.
#ifndef TESTS_PPAPI_GETURL_URL_LOAD_REQUEST_H_
#define TESTS_PPAPI_GETURL_URL_LOAD_REQUEST_H_

#include <ppapi/c/ppb_url_loader.h>
#include <ppapi/c/ppb_url_request_info.h>
#include <ppapi/c/pp_instance.h>
#include <ppapi/c/pp_resource.h>
#include <ppapi/c/pp_stdint.h>
#include <string>
#include <vector>

class UrlLoadRequest {
 public:
    explicit UrlLoadRequest(PP_Instance instance);
    ~UrlLoadRequest();
    bool Init(std::string url, std::string* error);

    void OpenCallback(int32_t result);
    void ReadCallback(int32_t result);

 private:
  bool GetRequiredInterfaces(std::string* error);
  void Clear();
  void ReadBody();

  std::string url_;
  PP_Instance instance_;
  PP_Resource url_request_;
  PP_Resource loader_;
  const PPB_URLRequestInfo* req_info_interface_;
  const PPB_URLLoader* loader_interface_;
  char buffer_[1024];
  std::string url_response_body_;
};

#endif  // TESTS_PPAPI_GETURL_URL_LOAD_REQUEST_H_

