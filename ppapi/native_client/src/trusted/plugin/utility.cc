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
#include "ppapi/native_client/src/trusted/plugin/utility.h"

namespace plugin {

int gNaClPluginDebugPrintEnabled = -1;
FILE* gNaClPluginLogFile = NULL;

/*
 * Prints formatted message to the log.
 */
int NaClPluginPrintLog(const char *format, ...) {
  if (NULL == gNaClPluginLogFile) {
    return 0;
  }
  va_list arg;
  int done;
  va_start(arg, format);
  done = vfprintf(gNaClPluginLogFile, format, arg);
  va_end(arg);
  fflush(gNaClPluginLogFile);
  return done;
}

/*
 * Opens file where plugin log should be written. The file name is
 * taken from NACL_PLUGIN_LOG environment variable and process pid.
 * If environment variable doesn't exist or file can't be opened,
 * the function returns stdout.
 */
FILE* NaClPluginLogFileEnv() {
  char* file = getenv("NACL_PLUGIN_LOG");
  if (NULL != file) {
    int pid = GETPID();
    /*
     * 11 characters for pid, 5 for a glue string and 1 for null terminator.
     */
    size_t filename_length = strlen(file) + 32;
    char* filename = new char[filename_length];
    int ret = SNPRINTF(filename, filename_length, "%s.%d.log", file, pid);
    if (ret <= 0 || static_cast<size_t>(ret) >= filename_length) {
      delete[] filename;
      return stdout;
    }
    FILE* log_file = fopen(filename, "w+");
    delete[] filename;
    if (NULL == log_file) {
        return stdout;
    }
    return log_file;
  }
  return stdout;
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

}  // namespace plugin
