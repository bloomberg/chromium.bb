// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_FLASH_PROXY_H_
#define PPAPI_PROXY_PPB_FLASH_PROXY_H_

#include <string>
#include <vector>

#include "ipc/ipc_platform_file.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/shared_impl/host_resource.h"

namespace ppapi {

struct PPB_URLRequestInfo_Data;

namespace proxy {

struct PPBFlash_DrawGlyphs_Params;
class SerializedVarReturnValue;

class PPB_Flash_Proxy : public InterfaceProxy {
 public:
  explicit PPB_Flash_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_Flash_Proxy();

  // Returns the corresponding version of the Flash interface pointer.
  static const PPB_Flash_11* GetInterface11();
  static const PPB_Flash_12_0* GetInterface12_0();

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgSetInstanceAlwaysOnTop(PP_Instance instance,
                                   PP_Bool on_top);
  void OnMsgDrawGlyphs(const PPBFlash_DrawGlyphs_Params& params,
                       PP_Bool* result);
  void OnMsgGetProxyForURL(PP_Instance instance,
                           const std::string& url,
                           SerializedVarReturnValue result);
  void OnMsgNavigate(PP_Instance instance,
                     const PPB_URLRequestInfo_Data& data,
                     const std::string& target,
                     PP_Bool from_user_action,
                     int32_t* result);
  void OnMsgRunMessageLoop(PP_Instance instance);
  void OnMsgQuitMessageLoop(PP_Instance instance);
  void OnMsgGetLocalTimeZoneOffset(PP_Instance instance, PP_Time t,
                                   double* result);

  // When this proxy is in the host side, this value caches the interface
  // pointer so we don't have to retrieve it from the dispatcher each time.
  // In the plugin, this value is always NULL.
  const PPB_Flash* ppb_flash_impl_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Flash_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_FLASH_PROXY_H_
