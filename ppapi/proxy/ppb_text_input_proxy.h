// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PPB_TEXT_INPUT_PROXY_H_
#define PPAPI_PPB_TEXT_INPUT_PROXY_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/thunk/ppb_text_input_api.h"

struct PPB_TextInput_Dev;

namespace ppapi {
namespace proxy {

class PPB_TextInput_Proxy
    : public InterfaceProxy,
      public ppapi::thunk::PPB_TextInput_FunctionAPI {
 public:
  PPB_TextInput_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_TextInput_Proxy();

  // FunctionGroupBase overrides.
  ppapi::thunk::PPB_TextInput_FunctionAPI* AsPPB_TextInput_FunctionAPI()
      OVERRIDE;

  // PPB_TextInput_FunctionAPI implementation.
  virtual void SetTextInputType(PP_Instance instance,
                                PP_TextInput_Type type) OVERRIDE;
  virtual void UpdateCaretPosition(PP_Instance instance,
                                   const PP_Rect& caret,
                                   const PP_Rect& bounding_box) OVERRIDE;
  virtual void CancelCompositionText(PP_Instance instance) OVERRIDE;

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  static const InterfaceID kInterfaceID = INTERFACE_ID_PPB_TEXT_INPUT;

 private:
  // Message handlers.
  void OnMsgSetTextInputType(PP_Instance instance, PP_TextInput_Type type);
  void OnMsgUpdateCaretPosition(PP_Instance instance,
                                PP_Rect caret,
                                PP_Rect bounding_box);
  void OnMsgCancelCompositionText(PP_Instance instance);

  DISALLOW_COPY_AND_ASSIGN(PPB_TextInput_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PPB_TEXT_INPUT_PROXY_H_
