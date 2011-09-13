// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_CRYPTO_PROXY_H_
#define PPAPI_PROXY_PPB_CRYPTO_PROXY_H_

#include "ppapi/proxy/interface_proxy.h"

namespace ppapi {
namespace proxy {

class PPB_Crypto_Proxy : public InterfaceProxy {
 public:
  // This class should not normally be instantiated since there's only one
  // function that's implemented entirely within the plugin. However, we need
  // to support this so the machinery for automatically handling interfaces
  // works. As a result, this constructor will assert if it's actually used.
  PPB_Crypto_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_Crypto_Proxy();

  static const Info* GetInfo();

 private:
  virtual bool OnMessageReceived(const IPC::Message& msg);

  DISALLOW_COPY_AND_ASSIGN(PPB_Crypto_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_CRYPTO_PROXY_H_
