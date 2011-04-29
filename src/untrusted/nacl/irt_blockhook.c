/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>

#include "native_client/src/untrusted/irt/irt.h"

static void (*pre_block_hook)(void);
static void (*post_block_hook)(void);

void IRT_pre_irtcall_hook() {
  if (pre_block_hook != NULL) {
    pre_block_hook();
  }
}

void IRT_post_irtcall_hook() {
  if (post_block_hook != NULL) {
    post_block_hook();
  }
}

static int irt_register_block_hooks(void (*pre)(void), void (*post)(void)) {
  if (pre == NULL || post == NULL)
    return EINVAL;
  if (pre_block_hook != NULL || post_block_hook != NULL)
    return EBUSY;
  pre_block_hook = pre;
  post_block_hook = post;
  return 0;
}

const struct nacl_irt_blockhook nacl_irt_blockhook = {
  irt_register_block_hooks,
};
