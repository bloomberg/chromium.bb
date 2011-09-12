// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include "native_client/src/trusted/plugin/dylib_unittest.h"

#include <stdio.h>

#if NACL_WINDOWS
DylibHandle DylibOpen(const char* lib_path) {
  return LoadLibrary(lib_path);
}

bool DylibClose(DylibHandle dl_handle) {
  return FreeLibrary(dl_handle) == TRUE;
}

SymbolHandle GetSymbolHandle(DylibHandle dl_handle, const char* name) {
  return reinterpret_cast<SymbolHandle>(GetProcAddress(dl_handle, name));
}
#else
DylibHandle DylibOpen(const char* lib_path) {
  // By using RTLD_NOW we check that all symbols are resolved before the
  // dlopen completes, or it fails.
  return dlopen(lib_path, RTLD_NOW | RTLD_LOCAL);
}

bool DylibClose(DylibHandle dl_handle) {
  return dlclose(dl_handle) == 0;
}

SymbolHandle GetSymbolHandle(DylibHandle dl_handle, const char* name) {
  void* sym_handle = dlsym(dl_handle, name);
  char* error_string = dlerror();
  if (sym_handle == NULL || error_string != NULL) {
    fprintf(stderr, "Couldn't get symbol %s: %s\n", name, error_string);
    sym_handle = NULL;
  }
  return reinterpret_cast<SymbolHandle>(sym_handle);
}
#endif
