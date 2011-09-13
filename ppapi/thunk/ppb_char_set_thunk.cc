// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/c/pp_var.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_char_set_api.h"

namespace ppapi {
namespace thunk {

namespace {

char* UTF16ToCharSet(PP_Instance instance,
                     const uint16_t* utf16, uint32_t utf16_len,
                     const char* output_char_set,
                     PP_CharSet_ConversionError on_error,
                     uint32_t* output_length) {
  EnterFunction<PPB_CharSet_FunctionAPI> enter(instance, true);
  if (enter.failed())
    return NULL;
  return enter.functions()->UTF16ToCharSet(instance, utf16, utf16_len,
                                           output_char_set, on_error,
                                           output_length);
}

uint16_t* CharSetToUTF16(PP_Instance instance,
                         const char* input, uint32_t input_len,
                         const char* input_char_set,
                         PP_CharSet_ConversionError on_error,
                         uint32_t* output_length) {
  EnterFunction<PPB_CharSet_FunctionAPI> enter(instance, true);
  if (enter.failed())
    return NULL;
  return enter.functions()->CharSetToUTF16(instance, input, input_len,
                                           input_char_set, on_error,
                                           output_length);
}

PP_Var GetDefaultCharSet(PP_Instance instance) {
  EnterFunction<PPB_CharSet_FunctionAPI> enter(instance, true);
  if (enter.failed())
    return PP_MakeUndefined();
  return enter.functions()->GetDefaultCharSet(instance);
}

const PPB_CharSet_Dev g_ppb_char_set_thunk = {
  &UTF16ToCharSet,
  &CharSetToUTF16,
  &GetDefaultCharSet
};


}  // namespace

const PPB_CharSet_Dev* GetPPB_CharSet_Dev_Thunk() {
  return &g_ppb_char_set_thunk;
}

}  // namespace thunk
}  // namespace ppapi
