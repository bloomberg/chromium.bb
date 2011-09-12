// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_FLASH_FILE_PROXY_H_
#define PPAPI_PPB_FLASH_FILE_PROXY_H_

#include <string>
#include <vector>

#include "ipc/ipc_platform_file.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/proxy/interface_proxy.h"

struct PP_FileInfo;
struct PPB_Flash_File_FileRef;
struct PPB_Flash_File_ModuleLocal;

namespace ppapi {

class HostResource;

namespace proxy {

struct SerializedDirEntry;

class PPB_Flash_File_ModuleLocal_Proxy : public InterfaceProxy {
 public:
  PPB_Flash_File_ModuleLocal_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_Flash_File_ModuleLocal_Proxy();

  static const Info* GetInfo();

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
                      PP_FileInfo* info,
                      int32_t* result);
  void OnMsgGetDirContents(PP_Instance instance,
                           const std::string& path,
                           std::vector<SerializedDirEntry>* entries,
                           int32_t* result);

  // When this proxy is in the host side, this value caches the interface
  // pointer so we don't have to retrieve it from the dispatcher each time.
  // In the plugin, this value is always NULL.
  const PPB_Flash_File_ModuleLocal* ppb_flash_file_module_local_impl_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Flash_File_ModuleLocal_Proxy);
};

class PPB_Flash_File_FileRef_Proxy : public InterfaceProxy {
 public:
  PPB_Flash_File_FileRef_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_Flash_File_FileRef_Proxy();

  static const Info* GetInfo();

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgOpenFile(const ppapi::HostResource& host_resource,
                     int32_t mode,
                     IPC::PlatformFileForTransit* file_handle,
                     int32_t* result);
  void OnMsgQueryFile(const ppapi::HostResource& host_resource,
                      PP_FileInfo* info,
                      int32_t* result);

  // When this proxy is in the host side, this value caches the interface
  // pointer so we don't have to retrieve it from the dispatcher each time.
  // In the plugin, this value is always NULL.
  const PPB_Flash_File_FileRef* ppb_flash_file_fileref_impl_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Flash_File_FileRef_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PPB_FLASH_FILE_PROXY_H_
