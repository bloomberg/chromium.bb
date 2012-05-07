// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_FLASH_API_H_
#define PPAPI_THUNK_PPB_FLASH_API_H_

#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/c/private/ppb_flash_clipboard.h"
#include "ppapi/c/private/ppb_flash_file.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {
namespace thunk {

// This class collects all of the Flash interface-related APIs into one place.
class PPAPI_THUNK_EXPORT PPB_Flash_API {
 public:
  virtual ~PPB_Flash_API() {}

  // Flash.
  virtual void SetInstanceAlwaysOnTop(PP_Instance instance, PP_Bool on_top) = 0;
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
                             const PP_Point glyph_advances[]) = 0;
  virtual PP_Var GetProxyForURL(PP_Instance instance, const char* url) = 0;
  virtual int32_t Navigate(PP_Instance instance,
                           PP_Resource request_info,
                           const char* target,
                           PP_Bool from_user_action) = 0;
  virtual void RunMessageLoop(PP_Instance instance) = 0;
  virtual void QuitMessageLoop(PP_Instance instance) = 0;
  virtual double GetLocalTimeZoneOffset(PP_Instance instance, PP_Time t) = 0;
  virtual PP_Bool IsRectTopmost(PP_Instance instance, const PP_Rect* rect) = 0;
  virtual void UpdateActivity(PP_Instance instance) = 0;
  virtual PP_Var GetDeviceID(PP_Instance instance) = 0;
  virtual int32_t GetSettingInt(PP_Instance instance,
                                PP_FlashSetting setting) = 0;
  virtual void SetAllowSuddenTermination(PP_Instance instance,
                                         PP_Bool allowed) = 0;

  // FlashClipboard.
  virtual PP_Bool IsClipboardFormatAvailable(
      PP_Instance instance,
      PP_Flash_Clipboard_Type clipboard_type,
      PP_Flash_Clipboard_Format format) = 0;
  virtual PP_Var ReadClipboardData(PP_Instance instance,
                                   PP_Flash_Clipboard_Type clipboard_type,
                                   PP_Flash_Clipboard_Format format) = 0;
  virtual int32_t WriteClipboardData(PP_Instance instance,
                                     PP_Flash_Clipboard_Type clipboard_type,
                                     uint32_t data_item_count,
                                     const PP_Flash_Clipboard_Format formats[],
                                     const PP_Var data_items[]) = 0;

  // FlashFile_ModuleLocal.
  virtual bool CreateThreadAdapterForInstance(PP_Instance instance) = 0;
  virtual void ClearThreadAdapterForInstance(PP_Instance instance) = 0;
  virtual int32_t OpenFile(PP_Instance instance,
                           const char* path,
                           int32_t mode,
                           PP_FileHandle* file) = 0;
  virtual int32_t RenameFile(PP_Instance instance,
                             const char* path_from,
                             const char* path_to) = 0;
  virtual int32_t DeleteFileOrDir(PP_Instance instance,
                                  const char* path,
                                  PP_Bool recursive) = 0;
  virtual int32_t CreateDir(PP_Instance instance, const char* path) = 0;
  virtual int32_t QueryFile(PP_Instance instance,
                            const char* path,
                            PP_FileInfo* info) = 0;
  virtual int32_t GetDirContents(PP_Instance instance,
                                 const char* path,
                                 PP_DirContents_Dev** contents) = 0;
  virtual void FreeDirContents(PP_Instance instance,
                               PP_DirContents_Dev* contents) = 0;

  // FlashFile_FileRef.
  virtual int32_t OpenFileRef(PP_Instance instance,
                              PP_Resource file_ref,
                              int32_t mode,
                              PP_FileHandle* file) = 0;
  virtual int32_t QueryFileRef(PP_Instance instance,
                               PP_Resource file_ref,
                               PP_FileInfo* info) = 0;

  // FlashFullscreen.
  virtual PP_Bool FlashIsFullscreen(PP_Instance instance) = 0;
  virtual PP_Bool FlashSetFullscreen(PP_Instance instance,
                                     PP_Bool fullscreen) = 0;
  virtual PP_Bool FlashGetScreenSize(PP_Instance instance, PP_Size* size) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif // PPAPI_THUNK_PPB_FLASH_API_H_
