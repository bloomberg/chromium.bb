// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_flash_proxy.h"

#include "base/logging.h"
#include "build/build_config.h"
#include "ppapi/c/dev/pp_file_info_dev.h"
#include "ppapi/c/dev/ppb_font_dev.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/serialized_var.h"
#include "webkit/glue/plugins/ppb_private2.h"

namespace pp {
namespace proxy {

namespace {

// Given an error code and a handle result from a Pepper API call, converts
// to a PlatformFileForTransit, possibly also updating the error value if
// an error occurred.
IPC::PlatformFileForTransit PlatformFileToPlatformFileForTransit(
    int32_t* error,
    base::PlatformFile file) {
  if (*error != PP_OK)
    return IPC::InvalidPlatformFileForTransit();
#if defined(OS_WIN)
/* TODO(brettw): figure out how to get the target process handle.
  HANDLE result;
  if (!::DuplicateHandle(::GetCurrentProcess(), file,
                         target_process, &result, 0, false,
                         DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS)) {
    *error = PP_ERROR_NOACCESS;
    return INVALID_HANDLE_VALUE;
  }
  return result;
*/
  *error = PP_ERROR_NOACCESS;
  return INVALID_HANDLE_VALUE;
#elif defined(OS_POSIX)
  return base::FileDescriptor(file, true);
#endif
}

void SetInstanceAlwaysOnTop(PP_Instance pp_instance, bool on_top) {
  PluginDispatcher::Get()->Send(
      new PpapiHostMsg_PPBFlash_SetInstanceAlwaysOnTop(
          INTERFACE_ID_PPB_FLASH, pp_instance, on_top));
}

bool DrawGlyphs(PP_Resource pp_image_data,
                const PP_FontDescription_Dev* font_desc,
                uint32_t color,
                PP_Point position,
                PP_Rect clip,
                float transformation[3][3],
                uint32_t glyph_count,
                uint16_t glyph_indices[],
                PP_Point glyph_advances[]) {
  Dispatcher* dispatcher = PluginDispatcher::Get();

  PPBFlash_DrawGlyphs_Params params;
  params.pp_image_data = pp_image_data;
  params.font_desc.SetFromPPFontDescription(dispatcher, *font_desc, true);
  params.color = color;
  params.position = position;
  params.clip = clip;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++)
      params.transformation[i][j] = transformation[i][j];
  }

  params.glyph_indices.insert(params.glyph_indices.begin(),
                              &glyph_indices[0],
                              &glyph_indices[glyph_count]);
  params.glyph_advances.insert(params.glyph_advances.begin(),
                               &glyph_advances[0],
                               &glyph_advances[glyph_count]);

  dispatcher->Send(new PpapiHostMsg_PPBFlash_DrawGlyphs(
      INTERFACE_ID_PPB_FLASH, params));
  return true;
}

PP_Var GetProxyForURL(PP_Module pp_module, const char* url) {
  ReceiveSerializedVarReturnValue result;
  PluginDispatcher::Get()->Send(new PpapiHostMsg_PPBFlash_GetProxyForURL(
      INTERFACE_ID_PPB_FLASH, pp_module, url, &result));
  return result.Return(PluginDispatcher::Get());
}

int32_t OpenModuleLocalFile(PP_Module module,
                            const char* path,
                            int32_t mode,
                            PP_FileHandle* file) {
  int32_t result = PP_ERROR_FAILED;
  IPC::PlatformFileForTransit transit;
  PluginDispatcher::Get()->Send(new PpapiHostMsg_PPBFlash_OpenModuleLocalFile(
      INTERFACE_ID_PPB_FLASH, module, path, mode, &transit, &result));
  *file = IPC::PlatformFileForTransitToPlatformFile(transit);
  return result;
}

int32_t RenameModuleLocalFile(PP_Module module,
                              const char* path_from,
                              const char* path_to) {
  int32_t result = PP_ERROR_FAILED;
  PluginDispatcher::Get()->Send(new PpapiHostMsg_PPBFlash_RenameModuleLocalFile(
      INTERFACE_ID_PPB_FLASH, module, path_from, path_to, &result));
  return result;
}

int32_t DeleteModuleLocalFileOrDir(PP_Module module,
                                   const char* path,
                                   bool recursive) {
  int32_t result = PP_ERROR_FAILED;
  PluginDispatcher::Get()->Send(
      new PpapiHostMsg_PPBFlash_DeleteModuleLocalFileOrDir(
          INTERFACE_ID_PPB_FLASH, module, path, recursive, &result));
  return result;
}

int32_t CreateModuleLocalDir(PP_Module module, const char* path) {
  int32_t result = PP_ERROR_FAILED;
  PluginDispatcher::Get()->Send(new PpapiHostMsg_PPBFlash_CreateModuleLocalDir(
      INTERFACE_ID_PPB_FLASH, module, path, &result));
  return result;
}

int32_t QueryModuleLocalFile(PP_Module module,
                             const char* path,
                             PP_FileInfo_Dev* info) {
  int32_t result = PP_ERROR_FAILED;
  PluginDispatcher::Get()->Send(
      new PpapiHostMsg_PPBFlash_QueryModuleLocalFile(
          INTERFACE_ID_PPB_FLASH, module, path, info, &result));
  return result;
}

int32_t GetModuleLocalDirContents(PP_Module module,
                                  const char* path,
                                  PP_DirContents_Dev** contents) {
  int32_t result = PP_ERROR_FAILED;
  std::vector<SerializedDirEntry> entries;
  PluginDispatcher::Get()->Send(
      new PpapiHostMsg_PPBFlash_GetModuleLocalDirContents(
          INTERFACE_ID_PPB_FLASH, module, path, &entries, &result));

  // TODO(brettw) implement this.

  return result;
}

void FreeModuleLocalDirContents(PP_Module module,
                                PP_DirContents_Dev* contents) {
  // TODO(brettw) implement this.
}

bool NavigateToURL(PP_Instance pp_instance,
                   const char* url,
                   const char* target) {
  bool result = false;
  PluginDispatcher::Get()->Send(
      new PpapiHostMsg_PPBFlash_NavigateToURL(
          INTERFACE_ID_PPB_FLASH, pp_instance, url, target, &result));
  return result;
}

const PPB_Private2 ppb_flash = {
  &SetInstanceAlwaysOnTop,
  &DrawGlyphs,
  &GetProxyForURL,
  &OpenModuleLocalFile,
  &RenameModuleLocalFile,
  &DeleteModuleLocalFileOrDir,
  &CreateModuleLocalDir,
  &QueryModuleLocalFile,
  &GetModuleLocalDirContents,
  &FreeModuleLocalDirContents,
  &NavigateToURL,
};

}  // namespace

PPB_Flash_Proxy::PPB_Flash_Proxy(Dispatcher* dispatcher,
                                   const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_Flash_Proxy::~PPB_Flash_Proxy() {
}

const void* PPB_Flash_Proxy::GetSourceInterface() const {
  return &ppb_flash;
}

InterfaceID PPB_Flash_Proxy::GetInterfaceId() const {
  return INTERFACE_ID_PPB_FLASH;
}

void PPB_Flash_Proxy::OnMessageReceived(const IPC::Message& msg) {
  IPC_BEGIN_MESSAGE_MAP(PPB_Flash_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_SetInstanceAlwaysOnTop,
                        OnMsgSetInstanceAlwaysOnTop)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_DrawGlyphs,
                        OnMsgDrawGlyphs)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_GetProxyForURL,
                        OnMsgGetProxyForURL)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_OpenModuleLocalFile,
                        OnMsgOpenModuleLocalFile)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_RenameModuleLocalFile,
                        OnMsgRenameModuleLocalFile)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_DeleteModuleLocalFileOrDir,
                        OnMsgDeleteModuleLocalFileOrDir)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_CreateModuleLocalDir,
                        OnMsgCreateModuleLocalDir)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_QueryModuleLocalFile,
                        OnMsgQueryModuleLocalFile)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_GetModuleLocalDirContents,
                        OnMsgGetModuleLocalDirContents)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBFlash_NavigateToURL, OnMsgNavigateToURL)
  IPC_END_MESSAGE_MAP()
  // TODO(brettw) handle bad messages!
}

void PPB_Flash_Proxy::OnMsgSetInstanceAlwaysOnTop(
    PP_Instance instance,
    bool on_top) {
  ppb_flash_target()->SetInstanceAlwaysOnTop(instance, on_top);
}

void PPB_Flash_Proxy::OnMsgDrawGlyphs(
    const pp::proxy::PPBFlash_DrawGlyphs_Params& params) {
  PP_FontDescription_Dev font_desc;
  params.font_desc.SetToPPFontDescription(dispatcher(), &font_desc, false);

  if (params.glyph_indices.size() != params.glyph_advances.size() ||
      params.glyph_indices.empty())
    return;

  ppb_flash_target()->DrawGlyphs(
      params.pp_image_data, &font_desc, params.color,
      params.position, params.clip,
      const_cast<float(*)[3]>(params.transformation),
      static_cast<uint32_t>(params.glyph_indices.size()),
      const_cast<uint16_t*>(&params.glyph_indices[0]),
      const_cast<PP_Point*>(&params.glyph_advances[0]));
}

void PPB_Flash_Proxy::OnMsgGetProxyForURL(PP_Module module,
                                          const std::string& url,
                                          SerializedVarReturnValue result) {
  result.Return(dispatcher(), ppb_flash_target()->GetProxyForURL(
      module, url.c_str()));
}

void PPB_Flash_Proxy::OnMsgOpenModuleLocalFile(
    PP_Module module,
    const std::string& path,
    int32_t mode,
    IPC::PlatformFileForTransit* file_handle,
    int32_t* result) {
  base::PlatformFile file;
  *result = ppb_flash_target()->OpenModuleLocalFile(module, path.c_str(), mode,
                                                    &file);
  *file_handle = PlatformFileToPlatformFileForTransit(result, file);
}

void PPB_Flash_Proxy::OnMsgRenameModuleLocalFile(
    PP_Module module,
    const std::string& path_from,
    const std::string& path_to,
    int32_t* result) {
  *result = ppb_flash_target()->RenameModuleLocalFile(module, path_from.c_str(),
                                                      path_to.c_str());
}

void PPB_Flash_Proxy::OnMsgDeleteModuleLocalFileOrDir(
    PP_Module module,
    const std::string& path,
    bool recursive,
    int32_t* result) {
  *result = ppb_flash_target()->DeleteModuleLocalFileOrDir(module, path.c_str(),
                                                           recursive);
}

void PPB_Flash_Proxy::OnMsgCreateModuleLocalDir(PP_Module module,
                                                const std::string& path,
                                                int32_t* result) {
  *result = ppb_flash_target()->CreateModuleLocalDir(module, path.c_str());
}

void PPB_Flash_Proxy::OnMsgQueryModuleLocalFile(PP_Module module,
                                                const std::string& path,
                                                PP_FileInfo_Dev* info,
                                                int32_t* result) {
  *result = ppb_flash_target()->QueryModuleLocalFile(module, path.c_str(),
                                                     info);
}

void PPB_Flash_Proxy::OnMsgGetModuleLocalDirContents(
    PP_Module module,
    const std::string& path,
    std::vector<pp::proxy::SerializedDirEntry>* entries,
    int32_t* result) {
  // TODO(brettw) implement this.
}

void PPB_Flash_Proxy::OnMsgNavigateToURL(PP_Instance instance,
                                         const std::string& url,
                                         const std::string& target,
                                         bool* result) {
  *result = ppb_flash_target()->NavigateToURL(instance, url.c_str(),
                                              target.c_str());
}

}  // namespace proxy
}  // namespace pp
