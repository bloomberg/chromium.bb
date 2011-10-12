// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_text_input_impl.h"

#include "base/logging.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/rect.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"

namespace webkit {
namespace ppapi {

PPB_TextInput_Impl::PPB_TextInput_Impl(PluginInstance* instance)
    : instance_(instance) {
}

::ppapi::thunk::PPB_TextInput_FunctionAPI*
PPB_TextInput_Impl::AsPPB_TextInput_FunctionAPI() {
  return this;
}

// Check PP_TextInput_Type and ui::TextInputType are kept in sync.
COMPILE_ASSERT(int(ui::TEXT_INPUT_TYPE_NONE) == \
    int(PP_TEXTINPUT_TYPE_NONE), mismatching_enums);
COMPILE_ASSERT(int(ui::TEXT_INPUT_TYPE_TEXT) == \
    int(PP_TEXTINPUT_TYPE_TEXT), mismatching_enums);
COMPILE_ASSERT(int(ui::TEXT_INPUT_TYPE_PASSWORD) == \
    int(PP_TEXTINPUT_TYPE_PASSWORD), mismatching_enums);
COMPILE_ASSERT(int(ui::TEXT_INPUT_TYPE_SEARCH) == \
    int(PP_TEXTINPUT_TYPE_SEARCH), mismatching_enums);
COMPILE_ASSERT(int(ui::TEXT_INPUT_TYPE_EMAIL) == \
    int(PP_TEXTINPUT_TYPE_EMAIL), mismatching_enums);
COMPILE_ASSERT(int(ui::TEXT_INPUT_TYPE_NUMBER) == \
    int(PP_TEXTINPUT_TYPE_NUMBER), mismatching_enums);
COMPILE_ASSERT(int(ui::TEXT_INPUT_TYPE_TELEPHONE) == \
    int(PP_TEXTINPUT_TYPE_TELEPHONE), mismatching_enums);
COMPILE_ASSERT(int(ui::TEXT_INPUT_TYPE_URL) == \
    int(PP_TEXTINPUT_TYPE_URL), mismatching_enums);

void PPB_TextInput_Impl::SetTextInputType(PP_Instance instance,
                                          PP_TextInput_Type type) {
  int itype = type;
  if (itype < 0 || itype > ui::TEXT_INPUT_TYPE_URL)
    itype = ui::TEXT_INPUT_TYPE_NONE;
  instance_->SetTextInputType(static_cast<ui::TextInputType>(itype));
}

void PPB_TextInput_Impl::UpdateCaretPosition(PP_Instance instance,
                                             const PP_Rect& caret,
                                             const PP_Rect& bounding_box) {
  instance_->UpdateCaretPosition(
      gfx::Rect(caret.point.x, caret.point.y,
                caret.size.width, caret.size.height),
      gfx::Rect(bounding_box.point.x, bounding_box.point.y,
                bounding_box.size.width, bounding_box.size.height));
}

void PPB_TextInput_Impl::CancelCompositionText(PP_Instance instance) {
  instance_->delegate()->PluginRequestedCancelComposition(instance_);
}

}  // namespace ppapi
}  // namespace webkit
