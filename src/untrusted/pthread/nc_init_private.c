/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/untrusted/irt/irt_interfaces.h"
#include "native_client/src/untrusted/pthread/pthread_internal.h"

/*
 * The other interfaces have global __nc_irt_<name> variables.
 * Those are defined as aliases to nacl_irt_<name> tables, each
 * defined in ../irt/irt_<name>.c.
 */
void __nc_initialize_interfaces(struct nacl_irt_thread *irt_thread) {
  *irt_thread = nacl_irt_thread;
}
