// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_FLASH_PROXY_H_
#define PPAPI_PPB_FLASH_PROXY_H_

#include <vector>

#include "ipc/ipc_platform_file.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/proxy/interface_proxy.h"

struct PP_FileInfo_Dev;
struct PPB_Flash;

namespace pp {
namespace proxy {

struct PPBFlash_DrawGlyphs_Params;
class SerializedVarReturnValue;

class PPB_Flash_Proxy : public InterfaceProxy {
 public:
  PPB_Flash_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_Flash_Proxy();

  static const Info* GetInfo();

  const PPB_Flash* ppb_flash_target() const {
    return static_cast<const PPB_Flash*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgSetInstanceAlwaysOnTop(PP_Instance instance,
                                   PP_Bool on_top);
  void OnMsgDrawGlyphs(const pp::proxy::PPBFlash_DrawGlyphs_Params& params,
                       PP_Bool* result);
  void OnMsgGetProxyForURL(PP_Instance instance,
                           const std::string& url,
                           SerializedVarReturnValue result);
  void OnMsgNavigateToURL(PP_Instance instance,
                          const std::string& url,
                          const std::string& target,
                          PP_Bool* result);
  void OnMsgRunMessageLoop(PP_Instance instance);
  void OnMsgQuitMessageLoop(PP_Instance instance);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PPB_FLASH_PROXY_H_
