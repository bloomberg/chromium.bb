// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_FLASH_CLIPBOARD_PROXY_H_
#define PPAPI_PROXY_PPB_FLASH_CLIPBOARD_PROXY_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/thunk/ppb_flash_clipboard_api.h"

namespace ppapi {
namespace proxy {

class SerializedVarReceiveInput;
class SerializedVarReturnValue;

class PPB_Flash_Clipboard_Proxy
    : public InterfaceProxy,
      public thunk::PPB_Flash_Clipboard_FunctionAPI {
 public:
  PPB_Flash_Clipboard_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_Flash_Clipboard_Proxy();

  // FunctionGroupBase overrides.
  thunk::PPB_Flash_Clipboard_FunctionAPI* AsPPB_Flash_Clipboard_FunctionAPI()
      OVERRIDE;

  // PPB_Flash_Clipboard_FunctionAPI implementation.
  virtual PP_Bool IsFormatAvailable(PP_Instance instance,
                                    PP_Flash_Clipboard_Type clipboard_type,
                                    PP_Flash_Clipboard_Format format) OVERRIDE;
  virtual PP_Var ReadPlainText(PP_Instance instance,
                               PP_Flash_Clipboard_Type clipboard_type) OVERRIDE;
  virtual int32_t WritePlainText(PP_Instance instance,
                                 PP_Flash_Clipboard_Type clipboard_type,
                                 const PP_Var& text) OVERRIDE;

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  static const ApiID kApiID = API_ID_PPB_FLASH_CLIPBOARD;

 private:
  // Message handlers.
  void OnMsgIsFormatAvailable(PP_Instance instance,
                              int clipboard_type,
                              int format,
                              bool* result);
  void OnMsgReadPlainText(PP_Instance instance,
                          int clipboard_type,
                          SerializedVarReturnValue result);
  void OnMsgWritePlainText(PP_Instance instance,
                           int clipboard_type,
                           SerializedVarReceiveInput text);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_FLASH_CLIPBOARD_PROXY_H_
