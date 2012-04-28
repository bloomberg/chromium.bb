/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "native_client/src/untrusted/irt/irt.h"
#include "native_client/tests/inbrowser_test_runner/test_runner.h"

int TestMain(void) {
  struct nacl_irt_random random_interface;

  if (nacl_interface_query(
          NACL_IRT_RANDOM_v0_1, &random_interface, sizeof(random_interface)) !=
      sizeof(random_interface)) {
    fprintf(stderr, "Cannot get random interface!\n");
    return 1;
  }

  int result = 0;

  uint8_t byte1, byte2;
  size_t nread;
  int error = random_interface.get_random_bytes(&byte1, sizeof(byte1), &nread);
  if (error != 0) {
    fprintf(stderr, "get_random_bytes failed for size %u: %s\n",
            sizeof(byte1), strerror(error));
    result = 1;
  } else if (nread != sizeof(byte1)) {
    fprintf(stderr, "get_random_bytes read %u != %u\n", nread, sizeof(byte1));
    result = 1;
  } else {
    error = random_interface.get_random_bytes(&byte2, sizeof(byte2), &nread);
    if (error != 0) {
      fprintf(stderr, "get_random_bytes failed for size %u: %s\n",
              sizeof(byte2), strerror(error));
      result = 1;
    } else if (nread != sizeof(byte2)) {
      fprintf(stderr, "get_random_bytes read %u != %u\n", nread, sizeof(byte2));
      result = 1;
    }

    /*
     * Reading the same byte value twice is not really that unlikely.  So
     * we don't test the values.  We've just tested that the code doesn't
     * freak out for single-byte reads.
     */
  }

  int int1, int2;
  error = random_interface.get_random_bytes(&int1, sizeof(int1), &nread);
  if (error != 0) {
    fprintf(stderr, "get_random_bytes failed for size %u: %s\n",
            sizeof(int1), strerror(error));
    result = 1;
  } else if (nread != sizeof(int1)) {
    fprintf(stderr, "get_random_bytes read %u != %u\n", nread, sizeof(int1));
    result = 1;
  } else {
    error = random_interface.get_random_bytes(&int2, sizeof(int2), &nread);
    if (error != 0) {
      fprintf(stderr, "get_random_bytes failed for size %u: %s\n",
              sizeof(int2), strerror(error));
      result = 1;
    } else if (nread != sizeof(int2)) {
      fprintf(stderr, "get_random_bytes read %u != %u\n", nread, sizeof(int2));
      result = 1;
    } else {
      if (int1 == int2) {
        fprintf(stderr, "Read the same %u-byte value twice!  (%#x)\n",
                sizeof(int1), int1);
        result = 1;
      }
      /*
       * We got two different values, so they must be random!
       * (No, that's not really true.  But probably close enough
       * with a 32-bit value.)
       */
    }
  }

  return result;
}

int main(void) {
  return RunTests(TestMain);
}
