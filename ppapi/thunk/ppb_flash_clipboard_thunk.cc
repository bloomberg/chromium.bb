// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_flash_clipboard.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/ppb_flash_clipboard_api.h"

namespace ppapi {
namespace thunk {

namespace {

typedef EnterFunction<PPB_Flash_Clipboard_FunctionAPI> EnterFlashClipboard;

PP_Bool IsFormatAvailable(PP_Instance instance,
                          PP_Flash_Clipboard_Type clipboard_type,
                          PP_Flash_Clipboard_Format format) {
  EnterFlashClipboard enter(instance, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.functions()->IsFormatAvailable(instance, clipboard_type, format);
}

PP_Var ReadPlainText(PP_Instance instance,
                     PP_Flash_Clipboard_Type clipboard_type) {
  EnterFlashClipboard enter(instance, true);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.functions()->ReadPlainText(instance, clipboard_type);
}

int32_t WritePlainText(PP_Instance instance,
                       PP_Flash_Clipboard_Type clipboard_type,
                       PP_Var text) {
  EnterFlashClipboard enter(instance, true);
  if (enter.failed())
    return PP_ERROR_NOINTERFACE;
  return enter.functions()->WritePlainText(instance, clipboard_type, text);
}

const PPB_Flash_Clipboard g_ppb_flash_clipboard_thunk = {
  &IsFormatAvailable,
  &ReadPlainText,
  &WritePlainText
};

}  // namespace

const PPB_Flash_Clipboard_3_0* GetPPB_Flash_Clipboard_3_0_Thunk() {
  return &g_ppb_flash_clipboard_thunk;
}

}  // namespace thunk
}  // namespace ppapi
