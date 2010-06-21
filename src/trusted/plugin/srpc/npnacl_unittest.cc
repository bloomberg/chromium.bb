/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <dlfcn.h>
#include <stdio.h>
#include "native_client/src/shared/npruntime/nacl_npapi.h"

void* g_DlHandle;

void* GetSymbolHandle(const char* name) {
  void*  sym_handle = dlsym(g_DlHandle, name);
  char*  error_string = dlerror();
  if (!sym_handle || error_string) {
    fprintf(stderr, "Couldn't get symbol %s: %s\n", name, error_string);
    return NULL;
  }
  return sym_handle;
}

bool TestMIMEDescription() {
  typedef char* (*FP)();
  FP func = reinterpret_cast<FP>(GetSymbolHandle("NP_GetMIMEDescription"));
  if (!func) {
    return false;
  }
  char* mime_string = (*func)();
  if (!mime_string) {
    fprintf(stderr, "ERROR: NP_GetMIMEDescriptor returned no string\n");
    return false;
  }
  return true;
}

bool TestInitialize() {
  typedef NPError (*FP)(NPNetscapeFuncs*, NPPluginFuncs*);
  FP func = reinterpret_cast<FP>(GetSymbolHandle("NP_Initialize"));
  if (!func) {
    return false;
  }
  NPNetscapeFuncs browser_funcs;
  NPPluginFuncs plugin_funcs;
  browser_funcs.version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
  browser_funcs.size = sizeof(NPNetscapeFuncs);
  if (NPERR_NO_ERROR != (*func)(&browser_funcs, &plugin_funcs)) {
    fprintf(stderr, "ERROR: NP_Initialize returned error\n");
    return false;
  }
  return true;
}

bool TestEntryPoints() {
  typedef NPError (*FP)(NPPluginFuncs* funcs);
  FP func = reinterpret_cast<FP>(GetSymbolHandle("NP_GetEntryPoints"));
  if (NULL == func) {
    return false;
  }
  NPPluginFuncs plugin_funcs;
  if (NPERR_NO_ERROR != (*func)(&plugin_funcs)) {
    fprintf(stderr, "ERROR: NP_GetEntryPoints returned error\n");
    return false;
  }
  return true;
}

bool TestShutdown() {
  typedef NPError (*FP)(void);
  FP func = reinterpret_cast<FP>(GetSymbolHandle("NP_Shutdown"));
  if (NULL == func) {
    return false;
  }
  if (NPERR_NO_ERROR != (*func)()) {
    fprintf(stderr, "ERROR: NP_Shutdown returned error\n");
    return false;
  }
  return true;
}

int main(int argc, char** argv) {
  if (argc != 2) {
    fprintf(stderr, "Usage: %s <soname>\n", argv[0]);
    return 1;
  }
  // Test opening the .so
  g_DlHandle = dlopen(argv[1], RTLD_NOW);
  if (NULL == g_DlHandle) {
    fprintf(stderr, "Couldn't open: %s\n", dlerror());
    return 1;
  }

  bool success =
    (TestMIMEDescription() &&
     TestInitialize() &&
     TestEntryPoints() &&
     TestShutdown());

  // Test closing the .so
  if (dlclose(g_DlHandle)) {
    fprintf(stderr, "Couldn't close: %s\n", dlerror());
    return 1;
  }

  if (success) {
    printf("PASS\n");
    return 0;
  } else {
    printf("FAIL\n");
    return 1;
  }
}
