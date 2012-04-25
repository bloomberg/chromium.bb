// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_flash_clipboard.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/ppb_flash_api.h"

namespace ppapi {
namespace thunk {

namespace {

PP_Bool IsFormatAvailable(PP_Instance instance,
                          PP_Flash_Clipboard_Type clipboard_type,
                          PP_Flash_Clipboard_Format format) {
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->GetFlashAPI()->IsClipboardFormatAvailable(
      instance, clipboard_type, format);
}

PP_Var ReadData(PP_Instance instance,
                PP_Flash_Clipboard_Type clipboard_type,
                PP_Flash_Clipboard_Format format) {
  EnterInstance enter(instance);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.functions()->GetFlashAPI()->ReadClipboardData(
      instance, clipboard_type, format);
}

int32_t WriteData(PP_Instance instance,
                  PP_Flash_Clipboard_Type clipboard_type,
                  uint32_t data_item_count,
                  const PP_Flash_Clipboard_Format formats[],
                  const PP_Var data_items[]) {
  EnterInstance enter(instance);
  if (enter.failed())
    return enter.retval();
  return enter.functions()->GetFlashAPI()->WriteClipboardData(
      instance, clipboard_type, data_item_count, formats, data_items);
}

PP_Var ReadPlainText(PP_Instance instance,
                     PP_Flash_Clipboard_Type clipboard_type) {
  return ReadData(instance,
                  clipboard_type,
                  PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT);
}

int32_t WritePlainText(PP_Instance instance,
                       PP_Flash_Clipboard_Type clipboard_type,
                       PP_Var text) {
  PP_Flash_Clipboard_Format format = PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT;
  return WriteData(instance, clipboard_type, 1, &format, &text);
}

const PPB_Flash_Clipboard_3_0 g_ppb_flash_clipboard_thunk_3_0 = {
  &IsFormatAvailable,
  &ReadPlainText,
  &WritePlainText
};

const PPB_Flash_Clipboard_4_0 g_ppb_flash_clipboard_thunk_4_0 = {
  &IsFormatAvailable,
  &ReadData,
  &WriteData
};

}  // namespace

const PPB_Flash_Clipboard_3_0* GetPPB_Flash_Clipboard_3_0_Thunk() {
  return &g_ppb_flash_clipboard_thunk_3_0;
}

const PPB_Flash_Clipboard_4_0* GetPPB_Flash_Clipboard_4_0_Thunk() {
  return &g_ppb_flash_clipboard_thunk_4_0;
}

}  // namespace thunk
}  // namespace ppapi
