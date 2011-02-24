// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_FLASH_FILE_PROXY_H_
#define PPAPI_PPB_FLASH_FILE_PROXY_H_

#include <vector>

#include "ipc/ipc_platform_file.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/proxy/interface_proxy.h"

struct PP_FileInfo_Dev;
struct PPB_Flash_File_ModuleLocal;

namespace pp {
namespace proxy {

struct SerializedDirEntry;

class PPB_Flash_File_ModuleLocal_Proxy : public InterfaceProxy {
 public:
  PPB_Flash_File_ModuleLocal_Proxy(Dispatcher* dispatcher,
                                   const void* target_interface);
  virtual ~PPB_Flash_File_ModuleLocal_Proxy();

  static const Info* GetInfo();

  const PPB_Flash_File_ModuleLocal* ppb_flash_file_module_local_target() const {
    return static_cast<const PPB_Flash_File_ModuleLocal*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgOpenFile(PP_Instance instance,
                     const std::string& path,
                     int32_t mode,
                     IPC::PlatformFileForTransit* file_handle,
                     int32_t* result);
  void OnMsgRenameFile(PP_Instance instance,
                       const std::string& path_from,
                       const std::string& path_to,
                       int32_t* result);
  void OnMsgDeleteFileOrDir(PP_Instance instance,
                            const std::string& path,
                            PP_Bool recursive,
                            int32_t* result);
  void OnMsgCreateDir(PP_Instance instance,
                      const std::string& path,
                      int32_t* result);
  void OnMsgQueryFile(PP_Instance instance,
                      const std::string& path,
                      PP_FileInfo_Dev* info,
                      int32_t* result);
  void OnMsgGetDirContents(PP_Instance instance,
                           const std::string& path,
                           std::vector<pp::proxy::SerializedDirEntry>* entries,
                           int32_t* result);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PPB_FLASH_FILE_PROXY_H_
