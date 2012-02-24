// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_FLASH_CLIPBOARD_API_H_
#define PPAPI_THUNK_PPB_FLASH_CLIPBOARD_API_H_

#include "ppapi/c/private/ppb_flash_clipboard.h"
#include "ppapi/shared_impl/api_id.h"

namespace ppapi {
namespace thunk {

class PPB_Flash_Clipboard_FunctionAPI {
 public:
  virtual ~PPB_Flash_Clipboard_FunctionAPI() {}

  virtual PP_Bool IsFormatAvailable(PP_Instance instance,
                                    PP_Flash_Clipboard_Type clipboard_type,
                                    PP_Flash_Clipboard_Format format) = 0;
  virtual PP_Var ReadData(PP_Instance instance,
                          PP_Flash_Clipboard_Type clipboard_type,
                          PP_Flash_Clipboard_Format format) = 0;
  virtual int32_t WriteData(PP_Instance instance,
                            PP_Flash_Clipboard_Type clipboard_type,
                            uint32_t data_item_count,
                            const PP_Flash_Clipboard_Format formats[],
                            const PP_Var data_items[]) = 0;

  static const ApiID kApiID = API_ID_PPB_FLASH_CLIPBOARD;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_FLASH_CLIPBOARD_API_H_
