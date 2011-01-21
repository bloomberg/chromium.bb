// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_URL_RESPONSE_INFO_PROXY_H_
#define PPAPI_PROXY_PPB_URL_RESPONSE_INFO_PROXY_H_

#include "base/basictypes.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/proxy/interface_proxy.h"

struct PPB_URLResponseInfo;

namespace pp {
namespace proxy {

class SerializedVarReturnValue;

class PPB_URLResponseInfo_Proxy : public InterfaceProxy {
 public:
  PPB_URLResponseInfo_Proxy(Dispatcher* dispatcher,
                            const void* target_interface);
  virtual ~PPB_URLResponseInfo_Proxy();

  // URLResponseInfo objects are actually returned by the URLLoader class.
  // This function allows the URLLoader proxy to start the tracking of
  // a response info object in the plugin.
  static void TrackPluginResource(PP_Instance instance,
                                  PP_Resource response_resource);

  const PPB_URLResponseInfo* ppb_url_response_info_target() const {
    return static_cast<const PPB_URLResponseInfo*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual const void* GetSourceInterface() const;
  virtual InterfaceID GetInterfaceId() const;
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgGetProperty(PP_Resource response,
                        int32_t property,
                        SerializedVarReturnValue result);
  void OnMsgGetBodyAsFileRef(PP_Resource response,
                             PP_Resource* file_ref_result);

  DISALLOW_COPY_AND_ASSIGN(PPB_URLResponseInfo_Proxy);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_PPB_URL_RESPONSE_INFO_PROXY_H_
