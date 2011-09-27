// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_text_input_api.h"

namespace ppapi {
namespace thunk {

namespace {

void SetTextInputType(PP_Instance instance, PP_TextInput_Type type) {
  EnterFunction<PPB_TextInput_FunctionAPI> enter(instance, true);
  if (enter.succeeded())
    enter.functions()->SetTextInputType(instance, type);
}

void UpdateCaretPosition(PP_Instance instance,
                         const PP_Rect* caret,
                         const PP_Rect* bounding_box) {
  EnterFunction<PPB_TextInput_FunctionAPI> enter(instance, true);
  if (enter.succeeded() && caret && bounding_box)
    enter.functions()->UpdateCaretPosition(instance, *caret, *bounding_box);
}

void CancelCompositionText(PP_Instance instance) {
  EnterFunction<PPB_TextInput_FunctionAPI> enter(instance, true);
  if (enter.succeeded())
    enter.functions()->CancelCompositionText(instance);
}

const PPB_TextInput_Dev g_ppb_textinput_thunk = {
  &SetTextInputType,
  &UpdateCaretPosition,
  &CancelCompositionText,
};

}  // namespace

const PPB_TextInput_Dev* GetPPB_TextInput_Dev_Thunk() {
  return &g_ppb_textinput_thunk;
}

}  // namespace thunk
}  // namespace ppapi
