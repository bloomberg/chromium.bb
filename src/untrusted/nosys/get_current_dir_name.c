/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Stub routine for `get_current_dir_name' for porting support.
 */

#include <unistd.h>
#include <errno.h>

char *get_current_dir_name(void) {
  errno = ENOSYS;
  return NULL;
}

#include "native_client/src/untrusted/nosys/warning.h"
stub_warning(get_current_dir_name);
