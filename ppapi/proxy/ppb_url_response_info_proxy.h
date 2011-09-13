// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_URL_RESPONSE_INFO_PROXY_H_
#define PPAPI_PROXY_PPB_URL_RESPONSE_INFO_PROXY_H_

#include "base/basictypes.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/shared_impl/host_resource.h"

struct PPB_URLResponseInfo;

namespace ppapi {

struct PPB_FileRef_CreateInfo;

namespace proxy {

class SerializedVarReturnValue;

class PPB_URLResponseInfo_Proxy : public InterfaceProxy {
 public:
  PPB_URLResponseInfo_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_URLResponseInfo_Proxy();

  // URLResponseInfo objects are actually created and returned by the
  // URLLoader. This function allows the URLLoader to convert a new
  // HostResource representing a response info to a properly tracked
  // URLReponseInfo Resource. Returns the plugin resource ID for the
  // new resource.
  static PP_Resource CreateResponseForResource(
      const ppapi::HostResource& resource);

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  static const InterfaceID kInterfaceID = INTERFACE_ID_PPB_URL_RESPONSE_INFO;

 private:
  // Message handlers.
  void OnMsgGetProperty(const ppapi::HostResource& response,
                        int32_t property,
                        SerializedVarReturnValue result);
  void OnMsgGetBodyAsFileRef(const ppapi::HostResource& response,
                             PPB_FileRef_CreateInfo* result);

  DISALLOW_COPY_AND_ASSIGN(PPB_URLResponseInfo_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_URL_RESPONSE_INFO_PROXY_H_
