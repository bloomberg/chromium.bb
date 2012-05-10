// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_FLASH_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_FLASH_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "ppapi/shared_impl/ppb_flash_shared.h"

class ScopedClipboardWriterGlue;

namespace webkit_glue {
class ClipboardClient;
}

namespace webkit {
namespace ppapi {

class PluginInstance;

class PPB_Flash_Impl : public ::ppapi::PPB_Flash_Shared {
 public:
  explicit PPB_Flash_Impl(PluginInstance* instance);
  virtual ~PPB_Flash_Impl();

  // PPB_Flash_API.
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
  virtual void UpdateActivity(PP_Instance instance) OVERRIDE;
  virtual PP_Var GetDeviceID(PP_Instance instance) OVERRIDE;
  virtual int32_t GetSettingInt(PP_Instance instance,
                                PP_FlashSetting settiny) OVERRIDE;
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

 private:
  // Call to ensure that the clipboard_client is properly initialized. Returns
  // true on success. On failure, you should not use the client.
  bool InitClipboard();

  int32_t WriteClipboardDataItem(const PP_Flash_Clipboard_Format format,
                                 const PP_Var& data,
                                 ScopedClipboardWriterGlue* scw);

  PluginInstance* instance_;

  // This object is lazily created by InitClipboard.
  scoped_ptr<webkit_glue::ClipboardClient> clipboard_client_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Flash_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_FLASH_IMPL_H_
