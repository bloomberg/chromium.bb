// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_VAR_DEPRECATED_H_
#define NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_VAR_DEPRECATED_H_

#include <string>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/third_party/ppapi/c/dev/ppb_var_deprecated.h"
#include "native_client/src/third_party/ppapi/c/pp_var.h"

namespace ppapi_proxy {

// Implements the plugin side of the PPB_Var_Deprecated interface.
// This implementation also determines how PP_Vars are represented internally
// in the proxied implementation.
class PluginVarDeprecated {
 public:
  // Returns an interface pointer suitable to the PPAPI client.
  static const PPB_Var_Deprecated* GetInterface();

  // String helpers.
  static PP_Var StringToPPVar(PP_Module module_id, std::string str);
  static std::string PPVarToString(PP_Var var);

  // Printing and debugging.
  static void Print(PP_Var var);
  static std::string DebugString(PP_Var var);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(PluginVarDeprecated);
};

}  // namespace ppapi_proxy

#endif  // NATIVE_CLIENT_SRC_SHARED_PPAPI_PROXY_PLUGIN_VAR_DEPRECATED_H_

