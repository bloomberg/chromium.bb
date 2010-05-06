/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// Routines for determining the most appropriate NaCl executable for
// the current CPU's architecture.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_NEXE_ARCH_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_NEXE_ARCH_H_

#include <string>

namespace nacl_srpc {

// On success, Parses the <embed nexes="..."> attribute and determines
// the returns the URL of the nexe module appropriate for the
// NaCl sandbox implemented by the installed sel_ldr.
//
// On success, true is returned and |*result| is updated with the URL.
// On failure, false is returned, and |*result| is updated with an
// informative error message.
extern bool GetNexeURL(const char* nexes_attr, std::string* result);

}  // namespace nacl_srpc

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_SRPC_NEXE_ARCH_H_
