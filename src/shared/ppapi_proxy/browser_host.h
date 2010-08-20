/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_BROWSER_HOST_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_BROWSER_HOST_H_

#include "native_client/src/include/nacl_macros.h"
#include "ppapi/c/ppp.h"

namespace ppapi_proxy {

// Implements the browser side of starting up and shutting down a plugin.
// These methods are in one to one correspondence to the exported functions
// from a plugin, but may be invoked via a proxy, or a dll import, or...
// See ppapi/c/ppp.h for details.
class BrowserHost {
 public:
  BrowserHost() {}
  virtual ~BrowserHost() {}

  virtual int32_t InitializeModule(PP_Module module,
                                   PPB_GetInterface get_interface_function) = 0;
  virtual void ShutdownModule() = 0;
  virtual const void* GetInterface(const char* interface_name) = 0;

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(BrowserHost);
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_BROWSER_HOST_H_
