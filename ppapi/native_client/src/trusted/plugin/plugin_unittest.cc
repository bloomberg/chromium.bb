// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

// Unit tests for ppGoogleNaClPlugin

#include <stdio.h>

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/plugin/nexe_arch.h"
#include "native_client/src/trusted/plugin/dylib_unittest.h"

// Verify that the ISA string returned by the plugin is the correct one for
// this platform.
bool TestGetNexeArch(DylibHandle dl_handle, const nacl::string& expected_isa) {
  typedef const char* (*GetSandboxISAFunc)();
  GetSandboxISAFunc get_sandbox_isa_sym = reinterpret_cast<GetSandboxISAFunc>(
      GetSymbolHandle(dl_handle, "NaClPluginGetSandboxISA"));
  if (get_sandbox_isa_sym == NULL)
    return false;
  nacl::string sandbox_isa(get_sandbox_isa_sym());
  if (sandbox_isa != expected_isa) {
    fprintf(stderr, "TestGetNexeArch ERROR: expeced ISA %s, got %s\n",
            expected_isa.c_str(), sandbox_isa.c_str());
    return false;
  }
  return true;
}

int main(int argc, char** argv) {
  DylibHandle dl_handle = NULL;

  if (3 != argc) {
    fprintf(stderr, "Usage: %s <plugin_name> <ISA_string>\n", argv[0]);
    return 1;
  }
  // Test opening the dynamic library
  dl_handle = DylibOpen(argv[1]);
  if (NULL == dl_handle) {
    fprintf(stderr, "Couldn't open: %s\n", argv[1]);
    return 1;
  }

  // Exercise some bare minimum functionality for PPAPI plugins.
  bool success = TestGetNexeArch(dl_handle, argv[2]);

  // Test closing the dynamic library
  if (!DylibClose(dl_handle)) {
    fprintf(stderr, "Couldn't close: %s\n", argv[1]);
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
