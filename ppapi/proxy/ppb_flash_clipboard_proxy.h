// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_FLASH_CLIPBOARD_PROXY_H_
#define PPAPI_PROXY_PPB_FLASH_CLIPBOARD_PROXY_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/interface_proxy.h"

struct PPB_Flash_Clipboard;

namespace ppapi {
namespace proxy {

class SerializedVarReceiveInput;
class SerializedVarReturnValue;

class PPB_Flash_Clipboard_Proxy : public InterfaceProxy {
 public:
  PPB_Flash_Clipboard_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_Flash_Clipboard_Proxy();

  static const Info* GetInfo();

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgIsFormatAvailable(PP_Instance instance_id,
                              int clipboard_type,
                              int format,
                              bool* result);
  void OnMsgReadPlainText(PP_Instance instance_id,
                          int clipboard_type,
                          SerializedVarReturnValue result);
  void OnMsgWritePlainText(PP_Instance instance_id,
                           int clipboard_type,
                           SerializedVarReceiveInput text);

  // When this proxy is in the host side, this value caches the interface
  // pointer so we don't have to retrieve it from the dispatcher each time.
  // In the plugin, this value is always NULL.
  const PPB_Flash_Clipboard* ppb_flash_clipboard_impl_;
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_FLASH_CLIPBOARD_PROXY_H_
