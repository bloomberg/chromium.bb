/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>

#include "native_client/src/untrusted/irt/irt.h"

static struct nacl_irt_random random_interface = { NULL };

int nacl_secure_random_init(void) {
  struct nacl_irt_random interface;
  if (NULL != random_interface.get_random_bytes) {
    return 0;
  }
  if (sizeof(interface) != nacl_interface_query(
          NACL_IRT_RANDOM_v0_1, &interface, sizeof(interface))) {
    /*
     * Couldn't find the IRT interface.
     */
    return ENODEV;
  }
  if (!__sync_bool_compare_and_swap(
          &random_interface.get_random_bytes, NULL,
          interface.get_random_bytes)) {
    /*
     * We raced with another thread to initialize the random interface,
     * but there's nothing else to do: the implementation merely wrote
     * the same function pointer before we did.
     */
  }
  return 0;
}

int nacl_secure_random(void *dest, size_t bytes, size_t *bytes_written) {
  /*
   * Assume the interface was initialized, cause an untrusted crash if not.
   */
  return random_interface.get_random_bytes(&dest, bytes, bytes_written);
}
