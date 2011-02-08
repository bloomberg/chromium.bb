// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_URL_REQUEST_INFO_PROXY_H_
#define PPAPI_PROXY_PPB_URL_REQUEST_INFO_PROXY_H_

#include "base/basictypes.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/proxy/host_resource.h"
#include "ppapi/proxy/interface_proxy.h"

struct PPB_URLRequestInfo;

namespace pp {
namespace proxy {

class SerializedVarReceiveInput;

class PPB_URLRequestInfo_Proxy : public InterfaceProxy {
 public:
  PPB_URLRequestInfo_Proxy(Dispatcher* dispatcher,
                           const void* target_interface);
  virtual ~PPB_URLRequestInfo_Proxy();

  static const Info* GetInfo();

  const PPB_URLRequestInfo* ppb_url_request_info_target() const {
    return static_cast<const PPB_URLRequestInfo*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgCreate(PP_Instance instance, HostResource* result);
  void OnMsgSetProperty(HostResource request,
                        int32_t property,
                        SerializedVarReceiveInput value);
  void OnMsgAppendDataToBody(HostResource request,
                             const std::string& data);
  void OnMsgAppendFileToBody(HostResource request,
                             HostResource file_ref,
                             int64_t start_offset,
                             int64_t number_of_bytes,
                             double expected_last_modified_time);

  DISALLOW_COPY_AND_ASSIGN(PPB_URLRequestInfo_Proxy);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_PPB_URL_REQUEST_INFO_PROXY_H_
