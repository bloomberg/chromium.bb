// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_text_input_impl.h"

#include "base/logging.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebTextInputType.h"
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

COMPILE_ASSERT(int(WebKit::WebTextInputTypeNone) == \
    int(PP_TEXTINPUT_TYPE_NONE), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebTextInputTypeText) == \
    int(PP_TEXTINPUT_TYPE_TEXT), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebTextInputTypePassword) == \
    int(PP_TEXTINPUT_TYPE_PASSWORD), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebTextInputTypeSearch) == \
    int(PP_TEXTINPUT_TYPE_SEARCH), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebTextInputTypeEmail) == \
    int(PP_TEXTINPUT_TYPE_EMAIL), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebTextInputTypeNumber) == \
    int(PP_TEXTINPUT_TYPE_NUMBER), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebTextInputTypeTelephone) == \
    int(PP_TEXTINPUT_TYPE_TELEPHONE), mismatching_enums);
COMPILE_ASSERT(int(WebKit::WebTextInputTypeURL) == \
    int(PP_TEXTINPUT_TYPE_URL), mismatching_enums);

void PPB_TextInput_Impl::SetTextInputType(PP_Instance instance,
                                          PP_TextInput_Type type) {
  // TODO(kinaba) the implementation is split to another CL for reviewing.
  NOTIMPLEMENTED();
}

void PPB_TextInput_Impl::UpdateCaretPosition(PP_Instance instance,
                                             const PP_Rect& caret,
                                             const PP_Rect& bounding_box) {
  // TODO(kinaba) the implementation is split to another CL for reviewing.
  NOTIMPLEMENTED();
}

void PPB_TextInput_Impl::CancelCompositionText(PP_Instance instance) {
  // TODO(kinaba) the implementation is split to another CL for reviewing.
  NOTIMPLEMENTED();
}

}  // namespace ppapi
}  // namespace webkit
