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
struct PPB_Private2;

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

  const PPB_Private2* ppb_flash_target() const {
    return static_cast<const PPB_Private2*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual const void* GetSourceInterface() const;
  virtual InterfaceID GetInterfaceId() const;
  virtual void OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgSetInstanceAlwaysOnTop(PP_Instance instance,
                                   bool on_top);
  void OnMsgDrawGlyphs(const pp::proxy::PPBFlash_DrawGlyphs_Params& params);
  void OnMsgGetProxyForURL(PP_Module module,
                           const std::string& url,
                           SerializedVarReturnValue result);
  void OnMsgOpenModuleLocalFile(PP_Module module,
                                const std::string& path,
                                int32_t mode,
                                IPC::PlatformFileForTransit* file_handle,
                                int32_t* result);
  void OnMsgRenameModuleLocalFile(PP_Module module,
                                  const std::string& path_from,
                                  const std::string& path_to,
                                  int32_t* result);
  void OnMsgDeleteModuleLocalFileOrDir(PP_Module module,
                                       const std::string& path,
                                       bool recursive,
                                       int32_t* result);
  void OnMsgCreateModuleLocalDir(PP_Module module,
                                 const std::string& path,
                                 int32_t* result);
  void OnMsgQueryModuleLocalFile(PP_Module module,
                                 const std::string& path,
                                 PP_FileInfo_Dev* info,
                                 int32_t* result);
  void OnMsgGetModuleLocalDirContents(
      PP_Module module,
      const std::string& path,
      std::vector<pp::proxy::SerializedDirEntry>* entries,
      int32_t* result);
  void OnMsgNavigateToURL(PP_Instance instance,
                          const std::string& url,
                          const std::string& target,
                          bool* result);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PPB_FLASH_PROXY_H_
