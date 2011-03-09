// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_FLASH_CLIPBOARD_PROXY_H_
#define PPAPI_PROXY_PPB_FLASH_CLIPBOARD_PROXY_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/interface_proxy.h"

struct PPB_Flash_Clipboard;

namespace pp {
namespace proxy {

class SerializedVarReceiveInput;
class SerializedVarReturnValue;

class PPB_Flash_Clipboard_Proxy : public InterfaceProxy {
 public:
  PPB_Flash_Clipboard_Proxy(Dispatcher* dispatcher,
                            const void* target_interface);
  virtual ~PPB_Flash_Clipboard_Proxy();

  static const Info* GetInfo();

  const PPB_Flash_Clipboard* ppb_flash_clipboard_target() const {
    return reinterpret_cast<const PPB_Flash_Clipboard*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgReadPlainText(PP_Instance instance_id,
                          int clipboard_type,
                          SerializedVarReturnValue result);
  void OnMsgWritePlainText(PP_Instance instance_id,
                           int clipboard_type,
                           SerializedVarReceiveInput text);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_PPB_FLASH_CLIPBOARD_PROXY_H_
