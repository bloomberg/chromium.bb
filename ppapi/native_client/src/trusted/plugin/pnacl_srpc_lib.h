// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_SRPC_LIB_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_SRPC_LIB_H_

// Routines to simplify SRPC invocations.

#include <stdarg.h>
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability.h"

namespace plugin {

class BrowserInterface;
class NaClSubprocess;
class SrpcParams;

// TODO(jvoung): See if anything can be shared between this and
// src/shared/srpc/invoke.c

// This is just a namespace to collect methods for setting up and invoking
// SPRC methods against a NaClSubprocess.
class PnaclSrpcLib {
 public:
  // Invoke an Srpc Method on the NaCl subprocess |subprocess|.
  // |out_params| must be allocated and cleaned up outside of this function,
  // but it will be initialized by this function, and on success
  // any out-params (if any) will be placed in |out_params|.
  // Input types must be listed in |input_signature|, with the actual
  // arguments passed in as var-args.
  // Returns |true| on success.
  static bool InvokeSrpcMethod(BrowserInterface* browser_interface,
                               const NaClSubprocess* subprocess,
                               const nacl::string& method_name,
                               const nacl::string& input_signature,
                               SrpcParams* out_params,
                               ...);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PnaclSrpcLib);

  static bool SetupSrpcInvocation(BrowserInterface* browser_interface,
                                  const NaClSubprocess* subprocess,
                                  const nacl::string& method_name,
                                  SrpcParams* params,
                                  uintptr_t* kMethodIdent);

  static bool VInvokeSrpcMethod(BrowserInterface* browser_interface,
                                const NaClSubprocess* subprocess,
                                const nacl::string& method_name,
                                const nacl::string& signature,
                                SrpcParams* params,
                                va_list vl);
};

}  // namespace plugin;

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PNACL_SRPC_LIB_H_
