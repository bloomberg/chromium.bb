/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// A collection of debugging related interfaces.

#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_UTILITY_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_UTILITY_H_

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_threads.h"

#define SRPC_PLUGIN_DEBUG 1

namespace plugin {

// Tests that a string is a valid JavaScript identifier.  According to the
// ECMAScript spec, this should be done in terms of unicode character
// categories.  For now, we are simply limiting identifiers to the ASCII
// subset of that spec.  If successful, it returns the length of the
// identifier in the location pointed to by length (if it is not NULL).
// TODO(sehr): add Unicode identifier support.
bool IsValidIdentifierString(const char* strval, uint32_t* length);

// Debugging print utility
extern int gNaClPluginDebugPrintEnabled;
extern FILE* gNaClPluginLogFile;
extern int NaClPluginPrintLog(const char *format, ...);
extern int NaClPluginDebugPrintCheckEnv();
extern FILE* NaClPluginLogFileEnv();
#if SRPC_PLUGIN_DEBUG
#  define PLUGIN_PRINTF(args) do {                                    \
    if (-1 == ::plugin::gNaClPluginDebugPrintEnabled) {               \
      ::plugin::gNaClPluginDebugPrintEnabled =                        \
          ::plugin::NaClPluginDebugPrintCheckEnv();                   \
      ::plugin::gNaClPluginLogFile = ::plugin::NaClPluginLogFileEnv();\
    }                                                                 \
    if (0 != ::plugin::gNaClPluginDebugPrintEnabled) {                \
      ::plugin::NaClPluginPrintLog("%08"NACL_PRIx32": ",              \
                                   NaClThreadId());                   \
      ::plugin::NaClPluginPrintLog args;                              \
    }                                                                 \
  } while (0)
#else
#  define PLUGIN_PRINTF(args) do { if (0) { printf args; } } while (0)
/* allows DCE but compiler can still do format string checks */
#endif

}  // namespace plugin

#endif  // NATIVE_CLIENT_SRC_TRUSTED_PLUGIN_UTILITY_H_
