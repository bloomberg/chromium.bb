// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_FLASH_PROXY_H_
#define PPAPI_PPB_FLASH_PROXY_H_

#include <string>
#include <vector>

#include "ipc/ipc_platform_file.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/shared_impl/host_resource.h"

struct PP_FileInfo;
struct PPB_Flash;

namespace ppapi {

struct PPB_URLRequestInfo_Data;

namespace proxy {

struct PPBFlash_DrawGlyphs_Params;
class SerializedVarReturnValue;

class PPB_Flash_Proxy : public InterfaceProxy {
 public:
  PPB_Flash_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_Flash_Proxy();

  static const Info* GetInfo();

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
                     bool from_user_action,
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

#endif  // PPAPI_PPB_FLASH_PROXY_H_
