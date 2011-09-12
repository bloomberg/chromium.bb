// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPP_MESSAGING_PROXY_H_
#define PPAPI_PROXY_PPP_MESSAGING_PROXY_H_

#include "ppapi/c/pp_instance.h"
#include "ppapi/proxy/interface_proxy.h"

struct PPP_Messaging;

namespace ppapi {
namespace proxy {

class SerializedVarReceiveInput;

class PPP_Messaging_Proxy : public InterfaceProxy {
 public:
  PPP_Messaging_Proxy(Dispatcher* dispatcher);
  virtual ~PPP_Messaging_Proxy();

  static const Info* GetInfo();

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

 private:
  // Message handlers.
  void OnMsgHandleMessage(PP_Instance instance,
                          SerializedVarReceiveInput data);

  // When this proxy is in the plugin side, this value caches the interface
  // pointer so we don't have to retrieve it from the dispatcher each time.
  // In the host, this value is always NULL.
  const PPP_Messaging* ppp_messaging_impl_;

  DISALLOW_COPY_AND_ASSIGN(PPP_Messaging_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPP_MESSAGING_PROXY_H_
