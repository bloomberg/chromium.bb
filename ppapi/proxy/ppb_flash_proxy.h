// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
struct SerializedDirEntry;
class SerializedVar;
class SerializedVarReturnValue;

class PPB_Flash_Proxy : public InterfaceProxy {
 public:
  PPB_Flash_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_Flash_Proxy();

  const PPB_Flash* ppb_flash_target() const {
    return static_cast<const PPB_Flash*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual const void* GetSourceInterface() const;
  virtual InterfaceID GetInterfaceId() const;
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
  void OnMsgOpenModuleLocalFile(PP_Instance instance,
                                const std::string& path,
                                int32_t mode,
                                IPC::PlatformFileForTransit* file_handle,
                                int32_t* result);
  void OnMsgRenameModuleLocalFile(PP_Instance instance,
                                  const std::string& path_from,
                                  const std::string& path_to,
                                  int32_t* result);
  void OnMsgDeleteModuleLocalFileOrDir(PP_Instance instance,
                                       const std::string& path,
                                       PP_Bool recursive,
                                       int32_t* result);
  void OnMsgCreateModuleLocalDir(PP_Instance instance,
                                 const std::string& path,
                                 int32_t* result);
  void OnMsgQueryModuleLocalFile(PP_Instance instance,
                                 const std::string& path,
                                 PP_FileInfo_Dev* info,
                                 int32_t* result);
  void OnMsgGetModuleLocalDirContents(
      PP_Instance instance,
      const std::string& path,
      std::vector<pp::proxy::SerializedDirEntry>* entries,
      int32_t* result);
  void OnMsgNavigateToURL(PP_Instance instance,
                          const std::string& url,
                          const std::string& target,
                          PP_Bool* result);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PPB_FLASH_PROXY_H_
