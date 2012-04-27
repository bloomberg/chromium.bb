// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/thunk.h"
#include "ppapi/thunk/ppb_instance_api.h"

namespace ppapi {
namespace thunk {

namespace {

void SetTextInputType(PP_Instance instance, PP_TextInput_Type type) {
  EnterInstance enter(instance);
  if (enter.succeeded())
    enter.functions()->SetTextInputType(instance, type);
}

void UpdateCaretPosition(PP_Instance instance,
                         const PP_Rect* caret,
                         const PP_Rect* bounding_box) {
  EnterInstance enter(instance);
  if (enter.succeeded() && caret && bounding_box)
    enter.functions()->UpdateCaretPosition(instance, *caret, *bounding_box);
}

void CancelCompositionText(PP_Instance instance) {
  EnterInstance enter(instance);
  if (enter.succeeded())
    enter.functions()->CancelCompositionText(instance);
}

void UpdateSurroundingText(PP_Instance instance, const char* text,
                           uint32_t caret, uint32_t anchor) {
  EnterInstance enter(instance);
  if (enter.succeeded())
    enter.functions()->UpdateSurroundingText(instance, text, caret, anchor);
}

void SelectionChanged(PP_Instance instance) {
  EnterInstance enter(instance);
  if (enter.succeeded())
    enter.functions()->SelectionChanged(instance);
}

const PPB_TextInput_Dev_0_1 g_ppb_textinput_0_1_thunk = {
  &SetTextInputType,
  &UpdateCaretPosition,
  &CancelCompositionText,
};

const PPB_TextInput_Dev g_ppb_textinput_0_2_thunk = {
  &SetTextInputType,
  &UpdateCaretPosition,
  &CancelCompositionText,
  &UpdateSurroundingText,
  &SelectionChanged,
};

}  // namespace

const PPB_TextInput_Dev_0_1* GetPPB_TextInput_Dev_0_1_Thunk() {
  return &g_ppb_textinput_0_1_thunk;
}

const PPB_TextInput_Dev_0_2* GetPPB_TextInput_Dev_0_2_Thunk() {
  return &g_ppb_textinput_0_2_thunk;
}

}  // namespace thunk
}  // namespace ppapi
