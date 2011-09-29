// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_TEXT_INPUT_API_H_
#define PPAPI_THUNK_PPB_TEXT_INPUT_API_H_

#include "ppapi/c/dev/ppb_text_input_dev.h"
#include "ppapi/proxy/interface_id.h"

namespace ppapi {
namespace thunk {

class PPB_TextInput_FunctionAPI {
 public:
  virtual ~PPB_TextInput_FunctionAPI() {}

  virtual void SetTextInputType(PP_Instance instance,
                                PP_TextInput_Type type) = 0;
  virtual void UpdateCaretPosition(PP_Instance instance,
                                   const PP_Rect& caret,
                                   const PP_Rect& bounding_box) = 0;
  virtual void CancelCompositionText(PP_Instance instance) = 0;

  static const proxy::InterfaceID interface_id =
      proxy::INTERFACE_ID_PPB_TEXT_INPUT;
};

}  // namespace thunk
}  // namespace ppapi

#endif  // PPAPI_THUNK_PPB_TEXT_INPUT_API_H_
