/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// Utility functions for handling PPAPI Var's


#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PPAPI_VAR_UTILS_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_PPAPI_VAR_UTILS_H_

#include <string>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/cpp/var.h"

namespace plugin {

nacl::string VarToString(const pp::Var& var);

void PPVarToNaClSrpcArg(const pp::Var& var, NaClSrpcArg* arg);

pp::Var NaClSrpcArgToPPVar(const NaClSrpcArg* arg);

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_VAR_UTILS_H_
