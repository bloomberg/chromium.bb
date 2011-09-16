// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/dev/text_input_dev.h"

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/rect.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_TextInput_Dev>() {
  return PPB_TEXTINPUT_DEV_INTERFACE;
}

}  // namespace


TextInput_Dev::TextInput_Dev(Instance* instance) : instance_(instance) {
}

TextInput_Dev::~TextInput_Dev() {
}

void TextInput_Dev::SetTextInputType(PP_TextInput_Type type) {
  if (!has_interface<PPB_TextInput_Dev>())
    return;
  get_interface<PPB_TextInput_Dev>()->SetTextInputType(
      instance_->pp_instance(), type);
}

void TextInput_Dev::UpdateCaretPosition(const Rect& caret,
                                        const Rect& bounding_box) {
  if (!has_interface<PPB_TextInput_Dev>())
    return;
  get_interface<PPB_TextInput_Dev>()->UpdateCaretPosition(
      instance_->pp_instance(), &caret.pp_rect(), &bounding_box.pp_rect());
}

void TextInput_Dev::ConfirmCompositionText() {
  if (!has_interface<PPB_TextInput_Dev>())
    return;
  get_interface<PPB_TextInput_Dev>()->ConfirmCompositionText(
      instance_->pp_instance());
}

void TextInput_Dev::CancelCompositionText() {
  if (!has_interface<PPB_TextInput_Dev>())
    return;
  get_interface<PPB_TextInput_Dev>()->CancelCompositionText(
      instance_->pp_instance());
}

}  // namespace pp
