// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/trusted/plugin/pnacl_srpc_lib.h"

#include <stdarg.h>

#include "native_client/src/trusted/plugin/browser_interface.h"
#include "native_client/src/trusted/plugin/method_map.h"
#include "native_client/src/trusted/plugin/nacl_subprocess.h"

namespace plugin {

bool PnaclSrpcLib::InvokeSrpcMethod(BrowserInterface* browser_interface,
                                    const NaClSubprocess* subprocess,
                                    const nacl::string& method_name,
                                    const nacl::string& input_signature,
                                    SrpcParams* params,
                                    ...) {
  va_list vl;
  va_start(vl, params);
  bool result = VInvokeSrpcMethod(browser_interface,
                                  subprocess,
                                  method_name,
                                  input_signature,
                                  params,
                                  vl);
  va_end(vl);
  return result;
}

bool PnaclSrpcLib::VInvokeSrpcMethod(BrowserInterface* browser_interface,
                                     const NaClSubprocess* subprocess,
                                     const nacl::string& method_name,
                                     const nacl::string& input_signature,
                                     SrpcParams* params,
                                     va_list vl) {
  uintptr_t kMethodIdent;
  if (!SetupSrpcInvocation(browser_interface,
                           subprocess,
                           method_name,
                           params,
                           &kMethodIdent)) {
    return false;
  }

  // Set up inputs.
  for (size_t i = 0; i < input_signature.length(); ++i) {
    char c = input_signature[i];
    // Only handle the limited number of SRPC types used for PNaCl.
    // Add more as needed.
    switch (c) {
      default:
        PLUGIN_PRINTF(("PnaclSrpcLib::InvokeSrpcMethod unhandled type: %c\n",
                       c));
        return false;
      case NACL_SRPC_ARG_TYPE_BOOL: {
        int input = va_arg(vl, int);
        params->ins()[i]->u.bval = input;
        break;
      }
      case NACL_SRPC_ARG_TYPE_DOUBLE: {
        double input = va_arg(vl, double);
        params->ins()[i]->u.dval = input;
        break;
      }
      case NACL_SRPC_ARG_TYPE_CHAR_ARRAY: {
        // SrpcParam's destructor *should* free the dup'ed string.
        const char* orig_str = va_arg(vl, const char*);
        char* input = strdup(orig_str);
        params->ins()[i]->arrays.str = input;
        break;
      }
      case NACL_SRPC_ARG_TYPE_HANDLE: {
        NaClSrpcImcDescType input = va_arg(vl, NaClSrpcImcDescType);
        params->ins()[i]->u.hval = input;
        break;
      }
      case NACL_SRPC_ARG_TYPE_INT: {
        int32_t input = va_arg(vl, int32_t);
        params->ins()[i]->u.ival = input;
        break;
      }
      case NACL_SRPC_ARG_TYPE_LONG: {
        int64_t input = va_arg(vl, int64_t);
        params->ins()[i]->u.lval = input;
        break;
      }
    }
  }

  return subprocess->Invoke(kMethodIdent, params);
}


bool PnaclSrpcLib::SetupSrpcInvocation(BrowserInterface* browser_interface,
                                       const NaClSubprocess* subprocess,
                                       const nacl::string& method_name,
                                       SrpcParams* params,
                                       uintptr_t* kMethodIdent) {
  *kMethodIdent = browser_interface->StringToIdentifier(method_name);
  if (!(subprocess->HasMethod(*kMethodIdent))) {
    PLUGIN_PRINTF(("SetupSrpcInvocation (no %s method found)\n",
                   method_name.c_str()));
    return false;
  }
  return subprocess->InitParams(*kMethodIdent, params);
}

}  // namespace plugin
