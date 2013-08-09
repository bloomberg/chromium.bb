/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/untrusted/nacl/nacl_irt.h"

void __libnacl_irt_filename_init(void) {
  /* Attempt to load the 'dev-filename' interface */
  if (!__libnacl_irt_query(NACL_IRT_DEV_FILENAME_v0_2,
                           &__libnacl_irt_dev_filename,
                           sizeof(__libnacl_irt_dev_filename))) {
    /*
     * Fall back to old 'filename' interface if the dev interface is
     * not found.  In this case only two members of the
     * __libnacl_irt_dev_filename will be non-NULL.
     */
    struct nacl_irt_filename old_irt_filename;
    if (__libnacl_irt_query(NACL_IRT_FILENAME_v0_1, &old_irt_filename,
                            sizeof(old_irt_filename))) {
      __libnacl_irt_dev_filename.open = old_irt_filename.open;
      __libnacl_irt_dev_filename.stat = old_irt_filename.stat;
    }
  }
}
