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

// This file tests for the presence of libraries and .o files in the SDK
// it does not actually execute any of the library code.


#include "libs_present_stub.h"

// This list should include all exported header files (directly or indirectly)
// to ensure they were properly included in the SDK.
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <iostream>
#include <nacl/nacl_av.h>
#include <nacl/nacl_htp.h>
#include <nacl/nacl_npapi.h>
#include <nacl/nacl_srpc.h>
#include <nacl/npruntime.h>
#include <nacl/npupp.h>


// Dummy variables used to hold return values.
bool bool_value;
double double_value;
pthread_t pthread_t_value;
const char* char_ptr_value;
char char_array_value[128];

extern "C" {
  // For npruntime support.
NPClass *GetNPSimpleClass() {
  return NULL;
}
}

static void TestLibsPresent() {
  // This code should invoke one method from each exported library to
  // ensure the library was built correctly.

  // Test that libm is present.
  if (run_tests)
    double_value = sin(0.0);

  // Test that libav is present.
  if (run_tests)
    nacl_multimedia_init(NACL_SUBSYSTEM_VIDEO);

  // Test that libgoogle_nacl_imc is present.
  if (run_tests)
    nacl::CreateIoDesc(NULL);

  // Test that libgoogle_nacl_npruntime is present.
  if (run_tests) {
    NPN_SetException(NULL, "This exception should never be raised.");
  }

  // Test that libpthread is present.
  if (run_tests)
    pthread_t_value = pthread_self();

  // Test that libsrpc is present.
  if (run_tests)
    char_ptr_value = NaClSrpcErrorString(NACL_SRPC_RESULT_OK);

  // Test that libunimpl is present.
  if (run_tests)
    char_ptr_value = getcwd(char_array_value, sizeof(char_array_value));
}

int main(int argc, char **argv) {
  // EH tests that libsupc++ is present.
  try {
    TestLibsPresent();
  } catch(...) {
    // iotream tests that libstdc++ is present.
    std::cout << "FAIL" << std::endl;
    return 1;
  }
  // printf tests that libc is present.
  printf("PASS\n");
  return 0;
}

extern "C" NPError NP_Initialize(NPNetscapeFuncs* browser_funcs,
                                 NPPluginFuncs* plugin_funcs) {
  return NPERR_NO_ERROR;
}
