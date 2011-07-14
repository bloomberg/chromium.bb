/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <sched.h>

#include "native_client/src/untrusted/pthread/pthread_types.h"

/*
 * A "token" is like a mutex which can only be acquired once.
 * Once a thread has acquired and released it, all later
 * calls to nc_token_acquire() return 0.
 *
 * If multiple threads call nc_token_acquire at the same time,
 * then nc_token_acquire will return "1" for one of the threads,
 * while the others are blocked until the winning thread
 * calls nc_token_release.
 */

#define NACL_TOKEN_FREE      0
#define NACL_TOKEN_ACQUIRED  1
#define NACL_TOKEN_RELEASED  2

#define TOKEN_EQUAL(_token, _value) \
    (__sync_val_compare_and_swap((_token), (_value), (_value)) == (_value))

#define EXCHANGE_TOKEN(_token, _old, _new) \
    (__sync_val_compare_and_swap((_token), (_old), (_new)))

void nc_token_init(volatile int *token) {
  *token = NACL_TOKEN_FREE;
  __sync_synchronize();
}

/* Returns 1 if the token was acquired, 0 otherwise */
int nc_token_acquire(volatile int *token) {
  int prev_val = EXCHANGE_TOKEN(token, NACL_TOKEN_FREE, NACL_TOKEN_ACQUIRED);

  if (prev_val == NACL_TOKEN_FREE)
    return 1;

  if (prev_val == NACL_TOKEN_RELEASED)
    return 0;

  /* Wait for the winner to release the token */
  while (!TOKEN_EQUAL(token, NACL_TOKEN_RELEASED))
    sched_yield();

  return 0;
}


void nc_token_release(volatile int *token) {
  EXCHANGE_TOKEN(token, NACL_TOKEN_ACQUIRED, NACL_TOKEN_RELEASED);
}
