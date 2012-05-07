// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_FLASH_PROXY_H_
#define PPAPI_PROXY_PPB_FLASH_PROXY_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "ipc/ipc_platform_file.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_time.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/ppb_flash_shared.h"

namespace ppapi {

struct PPB_URLRequestInfo_Data;

namespace proxy {

struct PPBFlash_DrawGlyphs_Params;
struct SerializedDirEntry;
class SerializedVarReturnValue;

class PPB_Flash_Proxy : public InterfaceProxy, public PPB_Flash_Shared {
 public:
  explicit PPB_Flash_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_Flash_Proxy();

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  // PPB_Flash_API implementation.
  virtual void SetInstanceAlwaysOnTop(PP_Instance instance,
                                      PP_Bool on_top) OVERRIDE;
  virtual PP_Bool DrawGlyphs(PP_Instance instance,
                             PP_Resource pp_image_data,
                             const PP_FontDescription_Dev* font_desc,
                             uint32_t color,
                             const PP_Point* position,
                             const PP_Rect* clip,
                             const float transformation[3][3],
                             PP_Bool allow_subpixel_aa,
                             uint32_t glyph_count,
                             const uint16_t glyph_indices[],
                             const PP_Point glyph_advances[]) OVERRIDE;
  virtual PP_Var GetProxyForURL(PP_Instance instance, const char* url) OVERRIDE;
  virtual int32_t Navigate(PP_Instance instance,
                           PP_Resource request_info,
                           const char* target,
                           PP_Bool from_user_action) OVERRIDE;
  virtual void RunMessageLoop(PP_Instance instance) OVERRIDE;
  virtual void QuitMessageLoop(PP_Instance instance) OVERRIDE;
  virtual double GetLocalTimeZoneOffset(PP_Instance instance,
                                        PP_Time t) OVERRIDE;
  virtual PP_Bool IsRectTopmost(PP_Instance instance,
                                const PP_Rect* rect) OVERRIDE;
  virtual int32_t InvokePrinting(PP_Instance instance) OVERRIDE;
  virtual void UpdateActivity(PP_Instance instance) OVERRIDE;
  virtual PP_Var GetDeviceID(PP_Instance instance) OVERRIDE;
  virtual int32_t GetSettingInt(PP_Instance instance,
                                PP_FlashSetting setting) OVERRIDE;
  virtual PP_Bool IsClipboardFormatAvailable(
      PP_Instance instance,
      PP_Flash_Clipboard_Type clipboard_type,
      PP_Flash_Clipboard_Format format) OVERRIDE;
  virtual PP_Var ReadClipboardData(PP_Instance instance,
                                   PP_Flash_Clipboard_Type clipboard_type,
                                   PP_Flash_Clipboard_Format format) OVERRIDE;
  virtual int32_t WriteClipboardData(PP_Instance instance,
                                     PP_Flash_Clipboard_Type clipboard_type,
                                     uint32_t data_item_count,
                                     const PP_Flash_Clipboard_Format formats[],
                                     const PP_Var data_items[]) OVERRIDE;
  virtual bool CreateThreadAdapterForInstance(PP_Instance instance) OVERRIDE;
  virtual void ClearThreadAdapterForInstance(PP_Instance instance) OVERRIDE;
  virtual int32_t OpenFile(PP_Instance instance,
                           const char* path,
                           int32_t mode,
                           PP_FileHandle* file) OVERRIDE;
  virtual int32_t RenameFile(PP_Instance instance,
                             const char* path_from,
                             const char* path_to) OVERRIDE;
  virtual int32_t DeleteFileOrDir(PP_Instance instance,
                                  const char* path,
                                  PP_Bool recursive) OVERRIDE;
  virtual int32_t CreateDir(PP_Instance instance, const char* path) OVERRIDE;
  virtual int32_t QueryFile(PP_Instance instance,
                            const char* path,
                            PP_FileInfo* info) OVERRIDE;
  virtual int32_t GetDirContents(PP_Instance instance,
                                 const char* path,
                                 PP_DirContents_Dev** contents) OVERRIDE;
  virtual int32_t OpenFileRef(PP_Instance instance,
                              PP_Resource file_ref,
                              int32_t mode,
                              PP_FileHandle* file) OVERRIDE;
  virtual int32_t QueryFileRef(PP_Instance instance,
                               PP_Resource file_ref,
                               PP_FileInfo* info) OVERRIDE;
  virtual PP_Bool FlashIsFullscreen(PP_Instance instance) OVERRIDE;
  virtual PP_Bool FlashSetFullscreen(PP_Instance instance,
                                     PP_Bool fullscreen) OVERRIDE;
  virtual PP_Bool FlashGetScreenSize(PP_Instance instance,
                                     PP_Size* size) OVERRIDE;
  virtual void SetAllowSuddenTermination(PP_Instance instance,
                                         PP_Bool allowed) OVERRIDE;

  static const ApiID kApiID = API_ID_PPB_FLASH;

 private:
  // Message handlers.
  void OnHostMsgSetInstanceAlwaysOnTop(PP_Instance instance,
                                       PP_Bool on_top);
  void OnHostMsgDrawGlyphs(PP_Instance instance,
                           const PPBFlash_DrawGlyphs_Params& params,
                           PP_Bool* result);
  void OnHostMsgGetProxyForURL(PP_Instance instance,
                               const std::string& url,
                               SerializedVarReturnValue result);
  void OnHostMsgNavigate(PP_Instance instance,
                         const PPB_URLRequestInfo_Data& data,
                         const std::string& target,
                         PP_Bool from_user_action,
                         int32_t* result);
  void OnHostMsgRunMessageLoop(PP_Instance instance);
  void OnHostMsgQuitMessageLoop(PP_Instance instance);
  void OnHostMsgGetLocalTimeZoneOffset(PP_Instance instance, PP_Time t,
                                       double* result);
  void OnHostMsgIsRectTopmost(PP_Instance instance,
                              PP_Rect rect,
                              PP_Bool* result);
  void OnHostMsgFlashSetFullscreen(PP_Instance instance,
                                   PP_Bool fullscreen,
                                   PP_Bool* result);
  void OnHostMsgFlashGetScreenSize(PP_Instance instance,
                                   PP_Bool* result,
                                   PP_Size* size);
  void OnHostMsgFlashSetAllowSuddenTermination(PP_Instance instance,
                                               PP_Bool allowed);
  void OnHostMsgIsClipboardFormatAvailable(PP_Instance instance,
                                           int clipboard_type,
                                           int format,
                                           bool* result);
  void OnHostMsgReadClipboardData(PP_Instance instance,
                                  int clipboard_type,
                                  int format,
                                  SerializedVarReturnValue result);
  void OnHostMsgWriteClipboardData(PP_Instance instance,
                                   int clipboard_type,
                                   const std::vector<int>& formats,
                                   SerializedVarVectorReceiveInput data_items);
  void OnHostMsgOpenFile(PP_Instance instance,
                         const std::string& path,
                         int32_t mode,
                         IPC::PlatformFileForTransit* file_handle,
                         int32_t* result);
  void OnHostMsgRenameFile(PP_Instance instance,
                           const std::string& path_from,
                           const std::string& path_to,
                           int32_t* result);
  void OnHostMsgDeleteFileOrDir(PP_Instance instance,
                                const std::string& path,
                                PP_Bool recursive,
                                int32_t* result);
  void OnHostMsgCreateDir(PP_Instance instance,
                          const std::string& path,
                          int32_t* result);
  void OnHostMsgQueryFile(PP_Instance instance,
                          const std::string& path,
                          PP_FileInfo* info,
                          int32_t* result);
  void OnHostMsgGetDirContents(PP_Instance instance,
                               const std::string& path,
                               std::vector<SerializedDirEntry>* entries,
                               int32_t* result);
  void OnHostMsgOpenFileRef(PP_Instance instance,
                            const ppapi::HostResource& host_resource,
                            int32_t mode,
                            IPC::PlatformFileForTransit* file_handle,
                            int32_t* result);
  void OnHostMsgQueryFileRef(PP_Instance instance,
                             const ppapi::HostResource& host_resource,
                             PP_FileInfo* info,
                             int32_t* result);
  void OnHostMsgGetDeviceID(PP_Instance instance,
                            SerializedVarReturnValue id);

  DISALLOW_COPY_AND_ASSIGN(PPB_Flash_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_FLASH_PROXY_H_
