// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_URL_REQUEST_INFO_PROXY_H_
#define PPAPI_PROXY_PPB_URL_REQUEST_INFO_PROXY_H_

#include "base/basictypes.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/shared_impl/host_resource.h"

struct PPB_URLRequestInfo;

namespace ppapi {
namespace proxy {

class SerializedVarReceiveInput;

class PPB_URLRequestInfo_Proxy : public InterfaceProxy {
 public:
  PPB_URLRequestInfo_Proxy(Dispatcher* dispatcher,
                           const void* target_interface);
  virtual ~PPB_URLRequestInfo_Proxy();

  static const Info* GetInfo();

  static PP_Resource CreateProxyResource(PP_Instance instance);

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgCreate(PP_Instance instance, ppapi::HostResource* result);
  void OnMsgSetProperty(ppapi::HostResource request,
                        int32_t property,
                        SerializedVarReceiveInput value);
  void OnMsgAppendDataToBody(ppapi::HostResource request,
                             const std::string& data);
  void OnMsgAppendFileToBody(ppapi::HostResource request,
                             ppapi::HostResource file_ref,
                             int64_t start_offset,
                             int64_t number_of_bytes,
                             double expected_last_modified_time);

  DISALLOW_COPY_AND_ASSIGN(PPB_URLRequestInfo_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_URL_REQUEST_INFO_PROXY_H_
