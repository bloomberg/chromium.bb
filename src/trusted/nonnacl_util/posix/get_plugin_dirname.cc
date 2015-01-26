/*
 * Copyright (c) 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


/* TODO: add comment why this is needed */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <dlfcn.h>
#include <libgen.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "native_client/src/include/build_config.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"

namespace nacl {

// First, try looking a symbol up in the dynamic loader's records.
static void PathFromSymbol(char* buffer, size_t len, void* sym) {
#if NACL_ANDROID
  UNREFERENCED_PARAMETER(sym);
  // We do not have dladdr() on Android.
  if (len > 0)
    buffer[0] = '\0';
#else
  Dl_info info;

  if (0 != dladdr(sym, &info)) {
    strncpy(buffer, info.dli_fname, len - 1);
  } else {
    buffer[0] = '\0';
  }
#endif
}

// Path of the current executable
static void PathFromExe(char* buffer, size_t len) {
  int bin_dir_size = readlink("/proc/self/exe", buffer, len - 1);
  if (bin_dir_size < 0 || (size_t) bin_dir_size >= len) {
    buffer[0] = '\0';
  } else {
    buffer[len - 1] = '\0';
  }
}

// This code tries to determine the plugin directory which contains
// the sel_ldr and possibly other libraries
static void GetPluginDirectory(char* buffer, size_t len) {
  // C++ really does not like to convert function pointer to regular pointers.
  // This is apparently the only way to do it without compiler warnings
  void* sym_addr = reinterpret_cast<void*>(
    reinterpret_cast<uintptr_t>(GetPluginDirectory));

  // We try 2 heuristics before giving up
  PathFromSymbol(buffer, len, sym_addr);

  if (0 == strlen(buffer)) {
    PathFromExe(buffer, len);
  }

  if (0 == strlen(buffer)) {
    // TOOD(robertm): maybe emit error message - but these file seem to not
    //                use NaClLog
    return;
  }

  char* path_end = strrchr(buffer, '/');
  if (NULL != path_end) {
    *path_end = '\0';
  }
}

void PluginSelLdrLocator::GetDirectory(char* buffer, size_t len) {
  GetPluginDirectory(buffer, len);
}


}  // namespace nacl
