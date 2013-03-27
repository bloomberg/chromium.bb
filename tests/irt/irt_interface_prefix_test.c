/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/untrusted/irt/irt.h"

/*
 * Check that the old version of the memory interface is
 * a prefix of the new version.
 */
void test_memory_interface_prefix(void) {
  struct nacl_irt_memory_v0_1 m1;
  struct nacl_irt_memory_v0_2 m2;
  int rc;

  rc = nacl_interface_query(NACL_IRT_MEMORY_v0_1, &m1, sizeof(m1));
  assert(rc == sizeof(m1));

  rc = nacl_interface_query(NACL_IRT_MEMORY_v0_2, &m2, sizeof(m2));
  assert(rc == sizeof(m2));

  assert(memcmp(&m1, &m2, sizeof(m1)) == 0);
}

int main(void) {
  test_memory_interface_prefix();

  return 0;
}
