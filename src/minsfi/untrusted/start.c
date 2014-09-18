/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <unistd.h>

void __newlib_thread_init(void);

int main(int argc, char **argv);

/*
 * This is the true entry point for untrusted code.
 * See comments in native_client/src/include/minsfi_priv.h for the layout of
 * the 'info' data structure.
 */
void _start(uint32_t info[]) {
  int argc = info[0];
  char **argv = (char**) (info + 1);

  __newlib_thread_init();

  _exit(main(argc, argv));
}
