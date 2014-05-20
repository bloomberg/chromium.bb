/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>

void __pnacl_start(uint32_t *info);

void __pnacl_start_linux_c(void *stack) {
  /*
   * TODO(mseaborn): Convert the Linux/ELF startup parameters in |stack| to
   * the NaCl layout.  For now, we just assume an empty argv, environment
   * and auxv.
   */
  uint32_t args[] = {
    0, /* cleanup_func pointer */
    0, /* envc */
    0, /* argc */
    0, /* argv terminator */
    0, /* env terminator */
    0, 0, /* empty auxv */
  };
  __pnacl_start(args);
}
