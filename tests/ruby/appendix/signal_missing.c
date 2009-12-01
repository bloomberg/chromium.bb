/* Copyright 2009 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file. */

#include <stdio.h>
#include "ruby.h"
#include "rubysig.h"

typedef void (*sighandler_t)(int signum);

sighandler_t ruby_nativethread_signal(int signum, sighandler_t handler) {
  rb_notimplement();
  return 0;
}
