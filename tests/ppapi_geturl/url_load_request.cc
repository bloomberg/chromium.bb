// Copyright 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "native_client/tests/ppapi_geturl/url_load_request.h"

#include <stdio.h>
#include <string.h>
#include <nacl/nacl_inttypes.h>

#include "native_client/tests/ppapi_geturl/module.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"


namespace {
  void OpenCallback(void* user_data, int32_t result);
  void ReadCallback(void* user_data, int32_t result);
}

UrlLoadRequest::UrlLoadRequest(PP_Instance instance)
    : instance_(instance),
      url_request_(0),
      loader_(0),
      req_info_interface_(0),
      loader_interface_(0) {
}

UrlLoadRequest::~UrlLoadRequest() {
  Clear();
}

void UrlLoadRequest::Clear() {
  Module* module = Module::Get();
  if (0 != url_request_) {
    module->core_interface()->ReleaseResource(url_request_);
    url_request_ = 0;
  }
  if (0 != loader_) {
    module->core_interface()->ReleaseResource(loader_);
    loader_ = 0;
  }
  url_response_body_.clear();
}

bool UrlLoadRequest::Init(std::string url,
                          std::string* error) {
  printf("--- UrlLoadRequest::Init()\n");
  url_ = url;
  Clear();
  if (!GetRequiredInterfaces(error)) {
    printf("--- GetRequiredInterfaces failed:%s\n", error->c_str());
    return false;
  }
  bool res1 = req_info_interface_->SetProperty(url_request_,
                                               PP_URLREQUESTPROPERTY_URL,
                                               Module::StrToVar(url));

  bool res2 = req_info_interface_->SetProperty(url_request_,
                                               PP_URLREQUESTPROPERTY_METHOD,
                                               Module::StrToVar("GET"));
  if (!res1 || !res2) {
    const char* msg = "--- req_info_interface_->SetProperty failed";
    printf("%s\n", msg);
    *error = msg;
    return false;
  }

  int32_t open_err = loader_interface_->Open(
      loader_, url_request_, PP_MakeCompletionCallback(::OpenCallback, this));
  if ((PP_OK != open_err) && (PP_ERROR_WOULDBLOCK != open_err)) {
    std::string err_text = Module::ErrorCodeToStr(open_err);
    printf("--- UrlLoadRequest::Init() : "
           "error %"NACL_PRId32" [%s] calling PPB_URLLoader::Open\n",
           open_err,
           err_text.c_str());
    *error = "error calling PPB_URLLoader::Open";
    return false;
  }
  return true;
}

bool UrlLoadRequest::GetRequiredInterfaces(std::string* error) {
  Module* module = Module::Get();
  req_info_interface_ = static_cast<const PPB_URLRequestInfo*>(
      module->GetBrowserInterface(PPB_URLREQUESTINFO_INTERFACE));
  if (NULL == req_info_interface_) {
    std::string msg = "Failed to get interface '";
    msg.append(PPB_URLREQUESTINFO_INTERFACE);
    msg.append("' from browser.");
    *error = msg;
    return false;
  }
  url_request_ = req_info_interface_->Create(module->module_id());
  if (0 == url_request_) {
    char tmp[200];
    snprintf(tmp,
             sizeof(tmp) - 1,
             PPB_URLREQUESTINFO_INTERFACE "::Create() failed");
    tmp[sizeof(tmp) - 1] = 0;
    *error = tmp;
    return false;
  }

  loader_interface_ = static_cast<const PPB_URLLoader*>(
          module->GetBrowserInterface(PPB_URLLOADER_INTERFACE));

  if (NULL == loader_interface_) {
    std::string msg = "Failed to get interface '";
    msg.append(PPB_URLLOADER_INTERFACE);
    msg.append("' from browser.");
    *error = msg;
    return false;
  }
  loader_ = loader_interface_->Create(instance_);
  if (0 == loader_) {
    *error = PPB_URLLOADER_INTERFACE "::Create() failed";
    return false;
  }
  return true;
}

void UrlLoadRequest::ReadBody() {
  int32_t read_result = loader_interface_->ReadResponseBody(
      loader_,
      buffer_,
      sizeof(buffer_),
      PP_MakeCompletionCallback(::ReadCallback, this));

  if ((PP_OK != read_result) && (PP_ERROR_WOULDBLOCK != read_result)) {
    std::string error = "PPB_URLLoader::ReadResponseBody error:";
    error.append(Module::ErrorCodeToStr(read_result));
    Module::Get()->ReportResult(instance_,
                                url_.c_str(),
                                error.c_str(),
                                false);
    delete this;
  }
}

void UrlLoadRequest::OpenCallback(int32_t result) {
  printf("--- UrlLoadRequest::OpenCallback(%"NACL_PRId32")\n", result);
  if (result < 0) {
    std::string error = "UrlLoadRequest::OpenCallback error:";
    error.append(Module::ErrorCodeToStr(result));
    Module::Get()->ReportResult(instance_,
                                url_.c_str(),
                                error.c_str(),
                                false);
  } else {
    ReadBody();
  }
}

void UrlLoadRequest::ReadCallback(int32_t result) {
  printf("--- UrlLoadRequest::ReadCallback(%"NACL_PRId32")\n", result);
  if (result < 0) {
    std::string error = "UrlLoadRequest::ReadCallback error:";
    error.append(Module::ErrorCodeToStr(result));
    Module::Get()->ReportResult(instance_,
                                url_.c_str(),
                                error.c_str(),
                                false);
    return;
  }
  for (int32_t i = 0; i < result; i++)
    url_response_body_.push_back(buffer_[i]);

  if (0 == result) {  // end of file
    Module::Get()->ReportResult(instance_,
                                url_.c_str(),
                                url_response_body_.c_str(),
                                true);
    delete this;
  } else {
    ReadBody();
  }
}

namespace {
  void OpenCallback(void* user_data, int32_t result) {
    UrlLoadRequest* obj = static_cast<UrlLoadRequest*>(user_data);
    if (NULL != obj)
      obj->OpenCallback(result);
  }
  void ReadCallback(void* user_data, int32_t result) {
    UrlLoadRequest* obj = static_cast<UrlLoadRequest*>(user_data);
    if (NULL != obj)
      obj->ReadCallback(result);
  }
}
