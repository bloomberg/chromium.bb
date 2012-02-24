// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_FLASH_CLIPBOARD_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_FLASH_CLIPBOARD_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ppapi/shared_impl/function_group_base.h"
#include "ppapi/thunk/ppb_flash_clipboard_api.h"

namespace webkit_glue { class ClipboardClient; }
class ScopedClipboardWriterGlue;

namespace webkit {
namespace ppapi {

class PluginInstance;

class PPB_Flash_Clipboard_Impl
    : public ::ppapi::FunctionGroupBase,
      public ::ppapi::thunk::PPB_Flash_Clipboard_FunctionAPI {
 public:
  PPB_Flash_Clipboard_Impl(PluginInstance* instance);
  virtual ~PPB_Flash_Clipboard_Impl();

  // FunctionGroupBase overrides.
  virtual ::ppapi::thunk::PPB_Flash_Clipboard_FunctionAPI*
      AsPPB_Flash_Clipboard_FunctionAPI() OVERRIDE;

  // PPB_Flash_Clipboard_FunctionAPI implementation.
  virtual PP_Bool IsFormatAvailable(PP_Instance instance,
                                    PP_Flash_Clipboard_Type clipboard_type,
                                    PP_Flash_Clipboard_Format format) OVERRIDE;
  virtual PP_Var ReadData(PP_Instance instance,
                          PP_Flash_Clipboard_Type clipboard_type,
                          PP_Flash_Clipboard_Format format);
  virtual int32_t WriteData(PP_Instance instance,
                            PP_Flash_Clipboard_Type clipboard_type,
                            uint32_t data_item_count,
                            const PP_Flash_Clipboard_Format formats[],
                            const PP_Var data_items[]);

 private:
  bool Init();
  int32_t WriteDataItem(const PP_Flash_Clipboard_Format format,
                        const PP_Var& data,
                        ScopedClipboardWriterGlue* scw);

  PluginInstance* instance_;
  scoped_ptr<webkit_glue::ClipboardClient> client_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Flash_Clipboard_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_FLASH_CLIPBOARD_IMPL_H_
