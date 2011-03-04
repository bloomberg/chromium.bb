// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_PRIVATE_PPB_FLASH_CLIPBOARD_H_
#define PPAPI_C_PRIVATE_PPB_FLASH_CLIPBOARD_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_var.h"

#define PPB_FLASH_CLIPBOARD_INTERFACE "PPB_Flash_Clipboard;2"

typedef enum {
  PP_FLASH_CLIPBOARD_TYPE_STANDARD = 0,
  PP_FLASH_CLIPBOARD_TYPE_SELECTION = 1,
  PP_FLASH_CLIPBOARD_TYPE_DRAG = 2
} PP_Flash_Clipboard_Type;

struct PPB_Flash_Clipboard {
  // Reads plain text data from the clipboard.
  struct PP_Var (*ReadPlainText)(PP_Instance instance_id,
                                 PP_Flash_Clipboard_Type clipboard_type);

  // Writes plain text data to the clipboard. If |text| is too large, it will
  // return |PP_ERROR_NOSPACE| (and not write to the clipboard).
  int32_t (*WritePlainText)(PP_Instance instance_id,
                            PP_Flash_Clipboard_Type clipboard_type,
                            struct PP_Var text);

  // TODO(vtl): More formats, a |IsFormatAvailable()|, ....
};

#endif  // PPAPI_C_PRIVATE_PPB_FLASH_CLIPBOARD_H_
