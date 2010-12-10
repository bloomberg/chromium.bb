// Copyright 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.
#ifndef TESTS_PPAPI_GETURL_URL_LOAD_REQUEST_H_
#define TESTS_PPAPI_GETURL_URL_LOAD_REQUEST_H_

#include <string>
#include <vector>

#include "ppapi/c/dev/ppb_file_io_dev.h"
#if __native_client__
// TODO(polina): add file_io_nacl include
#else
#include "ppapi/c/dev/ppb_file_io_trusted_dev.h"
#endif
#include "ppapi/c/dev/ppb_file_ref_dev.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/ppb_url_request_info.h"
#include "ppapi/c/ppb_url_response_info.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"


class UrlLoadRequest {
 public:
  explicit UrlLoadRequest(PP_Instance instance);
  ~UrlLoadRequest();
  bool Load(bool stream_as_file, std::string url);

  void OpenCallback(int32_t pp_error);
  void FinishStreamingToFileCallback(int32_t pp_error);
  void ReadResponseBodyCallback(int32_t pp_error_or_bytes);
  void ReadFileBodyCallback(int32_t pp_error);

  void set_delete_this_after_report() { delete_this_after_report = true; }

 private:
  bool GetRequiredInterfaces(std::string* error);
  void Clear();

  void ReadResponseBody();
  void ReadFileBody();

  bool ReportSuccess();
  bool ReportFailure(const std::string& error);
  bool ReportFailure(const std::string& message, int32_t pp_error);

  bool delete_this_after_report;

  std::string url_;
  bool as_file_;

  PP_Instance instance_;
  PP_Resource request_;
  PP_Resource loader_;
  PP_Resource response_;
  PP_Resource fileio_;

  const PPB_URLRequestInfo* request_interface_;
  const PPB_URLResponseInfo* response_interface_;
  const PPB_URLLoader* loader_interface_;
  const PPB_FileIO_Dev* fileio_interface_;
#if __native_client__
  // TODO(polina): when proxy supports FileIO_NaCl, add it here.
#else
  const PPB_FileIOTrusted_Dev* fileio_trusted_interface_;
#endif

  char buffer_[1024];
  std::string url_body_;

};

#endif  // TESTS_PPAPI_GETURL_URL_LOAD_REQUEST_H_
