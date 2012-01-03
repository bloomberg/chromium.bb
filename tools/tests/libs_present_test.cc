/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
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

#include "native_client/src/shared/imc/nacl_imc.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"


// Dummy variables used to hold return values.
bool bool_value;
double double_value;
pthread_t pthread_t_value;
const char* char_ptr_value;
char char_array_value[128];

static void TestLibsPresent() {
  // This code should invoke one method from each exported library to
  // ensure the library was built correctly.

  // Test that libm is present.
  if (run_tests)
    double_value = sin(0.0);

  // Test that libimc is present.
  if (run_tests)
    nacl::Close(nacl::kInvalidHandle);

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
