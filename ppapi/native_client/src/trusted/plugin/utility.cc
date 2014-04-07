/*
 * Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/portability_process.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "ppapi/cpp/module.h"
#include "ppapi/native_client/src/trusted/plugin/utility.h"

namespace plugin {

int gNaClPluginDebugPrintEnabled = -1;

/*
 * Prints formatted message to the log.
 */
int NaClPluginPrintLog(const char *format, ...) {
  va_list arg;
  int out_size;

  static const int kStackBufferSize = 512;
  char stack_buffer[kStackBufferSize];
  va_start(arg, format);
  out_size = vsnprintf(stack_buffer, kStackBufferSize, format, arg);
  va_end(arg);
  if (out_size < kStackBufferSize) {
    GetNaClInterface()->Vlog(stack_buffer);
  } else {
    // Message too large for our stack buffer; we have to allocate memory
    // instead.
    char *buffer = static_cast<char*>(malloc(out_size + 1));
    va_start(arg, format);
    vsnprintf(buffer, out_size + 1, format, arg);
    va_end(arg);
    GetNaClInterface()->Vlog(buffer);
    free(buffer);
  }
  return out_size;
}

/*
 * Currently looks for presence of NACL_PLUGIN_DEBUG and returns
 * 0 if absent and 1 if present.  In the future we may include notions
 * of verbosity level.
 */
int NaClPluginDebugPrintCheckEnv() {
  char* env = getenv("NACL_PLUGIN_DEBUG");
  return (NULL != env);
}

bool IsValidIdentifierString(const char* strval, uint32_t* length) {
  // This function is supposed to recognize valid ECMAScript identifiers,
  // as described in
  // http://www.ecma-international.org/publications/files/ECMA-ST/ECMA-262.pdf
  // It is currently restricted to only the ASCII subset.
  // TODO(sehr): Recognize the full Unicode formulation of identifiers.
  // TODO(sehr): Make this table-driven if efficiency becomes a problem.
  if (NULL != length) {
    *length = 0;
  }
  if (NULL == strval) {
    return false;
  }
  static const char* kValidFirstChars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz$_";
  static const char* kValidOtherChars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz$_"
      "0123456789";
  if (NULL == strchr(kValidFirstChars, strval[0])) {
    return false;
  }
  uint32_t pos;
  for (pos = 1; ; ++pos) {
    if (0 == pos) {
      // Unsigned overflow.
      return false;
    }
    int c = strval[pos];
    if (0 == c) {
      break;
    }
    if (NULL == strchr(kValidOtherChars, c)) {
      return false;
    }
  }
  if (NULL != length) {
    *length = pos;
  }
  return true;
}

// We cache the NaCl interface pointer and ensure that its set early on, on the
// main thread. This allows GetNaClInterface() to be used from non-main threads.
static const PPB_NaCl_Private* g_nacl_interface = NULL;

const PPB_NaCl_Private* GetNaClInterface() {
  if (g_nacl_interface)
    return g_nacl_interface;

  pp::Module *module = pp::Module::Get();
  CHECK(module);
  CHECK(module->core()->IsMainThread());
  g_nacl_interface = static_cast<const PPB_NaCl_Private*>(
      module->GetBrowserInterface(PPB_NACL_PRIVATE_INTERFACE));
  return g_nacl_interface;
}

}  // namespace plugin
