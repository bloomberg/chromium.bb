/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Unit tests for libnpGoogleNaClPlugin.so
// This test is specifically targeted at finding undefined external references
// prior to when loading the plugin in the browser.

#include <dlfcn.h>
#include <stdio.h>
#include <inttypes.h>
#include "native_client/src/third_party/npapi/files/include/npapi.h"
#include "native_client/src/third_party/npapi/files/include/npruntime.h"
#include "native_client/src/third_party/npapi/files/include/npupp.h"

void* dl_handle;

uintptr_t GetSymbolHandle(const char* name) {
  void*  sym_handle = dlsym(dl_handle, name);
  char*  error_string = dlerror();
  if (NULL == sym_handle || error_string) {
    fprintf(stderr, "Couldn't get symbol %s: %s\n", name, error_string);
    return NULL;
  }
  return reinterpret_cast<uintptr_t>(sym_handle);
}

bool TestMIMEDescription() {
  typedef char* (*FP)();
  FP func = reinterpret_cast<FP>(GetSymbolHandle("NP_GetMIMEDescription"));
  if (NULL == func) {
    return false;
  }
  char* mime_string = (*func)();
  if (NULL == mime_string) {
    fprintf(stderr, "ERROR: NP_GetMIMEDescriptor returned no string\n");
    return false;
  }
  return true;
}

bool TestInitialize() {
  typedef NPError (*FP)(NPNetscapeFuncs*, NPPluginFuncs*);
  FP func = reinterpret_cast<FP>(GetSymbolHandle("NP_Initialize"));
  if (NULL == func) {
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
  if (2 != argc) {
    fprintf(stderr, "Usage: %s <soname>\n", argv[0]);
    return 1;
  }
  // Test opening the .so
  // By using RTLD_NOW we check that all symbols are resolved before the
  // dlopen completes, or it fails.
  dl_handle = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
  if (NULL == dl_handle) {
    fprintf(stderr, "Couldn't open: %s\n", dlerror());
    return 1;
  }

  // Exercise some bare minimum functionality for NPAPI plugins.
  bool success =
    (TestMIMEDescription() &&
     TestInitialize() &&
     TestShutdown());

  // Test closing the .so
  if (0 != dlclose(dl_handle)) {
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
