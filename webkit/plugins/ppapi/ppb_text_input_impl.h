// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_TEXT_INPUT_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_TEXT_INPUT_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ppapi/shared_impl/function_group_base.h"
#include "ppapi/thunk/ppb_text_input_api.h"

namespace webkit {
namespace ppapi {

class PluginInstance;

class PPB_TextInput_Impl
    : public ::ppapi::FunctionGroupBase,
      public ::ppapi::thunk::PPB_TextInput_FunctionAPI {
 public:
  PPB_TextInput_Impl(PluginInstance* instance);

  // FunctionGroupBase overrides.
  virtual ::ppapi::thunk::PPB_TextInput_FunctionAPI*
    AsPPB_TextInput_FunctionAPI() OVERRIDE;

  // PPB_TextInput_FunctionAPI implementation.
  virtual void SetTextInputType(PP_Instance instance,
                                PP_TextInput_Type type) OVERRIDE;
  virtual void UpdateCaretPosition(PP_Instance instance,
                                   const PP_Rect& caret,
                                   const PP_Rect& bounding_box) OVERRIDE;
  virtual void CancelCompositionText(PP_Instance instance) OVERRIDE;

 private:
  PluginInstance* instance_;
  DISALLOW_COPY_AND_ASSIGN(PPB_TextInput_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_TEXT_INPUT_IMPL_H_
