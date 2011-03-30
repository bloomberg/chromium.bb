/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <unistd.h>
#include "native_client/src/untrusted/nacl/gc_hooks.h"

static TYPE_nacl_gc_hook pre_gc_hook = (TYPE_nacl_gc_hook) NULL;
static TYPE_nacl_gc_hook post_gc_hook = (TYPE_nacl_gc_hook) NULL;

void IRT_pre_irtcall_hook() {
  if (pre_gc_hook != NULL) {
    pre_gc_hook();
  }
}

void IRT_post_irtcall_hook() {
  if (post_gc_hook != NULL) {
    post_gc_hook();
  }
}

void nacl_register_gc_hooks(TYPE_nacl_gc_hook prehook,
                            TYPE_nacl_gc_hook posthook) {
  if (pre_gc_hook != NULL || post_gc_hook != NULL) {
    (void)write(2, "nacl_register_gc_hooks: double call\n", 36);
  }
  pre_gc_hook = prehook;
  post_gc_hook = posthook;
}
