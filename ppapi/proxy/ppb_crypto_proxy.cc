// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_crypto_proxy.h"

#include "ppapi/c/dev/ppb_crypto_dev.h"
#include "ppapi/proxy/interface_id.h"
#include "ppapi/shared_impl/crypto_impl.h"

namespace ppapi {
namespace proxy {

namespace {

const PPB_Crypto_Dev crypto_interface = {
  &ppapi::CryptoImpl::GetRandomBytes
};

InterfaceProxy* CreateCryptoProxy(Dispatcher* dispatcher,
                                  const void* target_interface) {
  return new PPB_Crypto_Proxy(dispatcher, target_interface);
}

}  // namespace

PPB_Crypto_Proxy::PPB_Crypto_Proxy(Dispatcher* dispatcher,
                                   const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
  NOTREACHED();  // See comment in the header file.
}

PPB_Crypto_Proxy::~PPB_Crypto_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_Crypto_Proxy::GetInfo() {
  static const Info info = {
    &crypto_interface,
    PPB_CRYPTO_DEV_INTERFACE,
    INTERFACE_ID_PPB_CRYPTO,
    false,
    &CreateCryptoProxy,
  };
  return &info;
}

bool PPB_Crypto_Proxy::OnMessageReceived(const IPC::Message& msg) {
  NOTREACHED();
  return false;
}

}  // namespace proxy
}  // namespace ppapi
