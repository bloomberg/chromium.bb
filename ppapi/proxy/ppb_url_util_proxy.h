// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_URL_UTIL_PROXY_H_
#define PPAPI_PROXY_PPB_URL_UTIL_PROXY_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/serialized_var.h"

struct PPB_URLUtil_Dev;

namespace ppapi {
namespace proxy {

class PPB_URLUtil_Proxy : public InterfaceProxy {
 public:
  PPB_URLUtil_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_URLUtil_Proxy();

  static const Info* GetInfo();

  const PPB_URLUtil_Dev* ppb_url_util_target() const {
    return static_cast<const PPB_URLUtil_Dev*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgResolveRelativeToDocument(PP_Instance instance,
                                      SerializedVarReceiveInput relative,
                                      SerializedVarReturnValue result);
  void OnMsgDocumentCanRequest(PP_Instance instance,
                               SerializedVarReceiveInput url,
                               PP_Bool* result);
  void OnMsgDocumentCanAccessDocument(PP_Instance active,
                                      PP_Instance target,
                                      PP_Bool* result);
  void OnMsgGetDocumentURL(PP_Instance instance,
                           SerializedVarReturnValue result);
  void OnMsgGetPluginInstanceURL(PP_Instance instance,
                                 SerializedVarReturnValue result);

  DISALLOW_COPY_AND_ASSIGN(PPB_URLUtil_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_URL_UTIL_PROXY_H_

