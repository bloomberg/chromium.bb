/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/*
 * Native Client stub for SRPC initialization.
 */

/*
 * This stub provides a definition for the function that initializes srpc.
 * Since it is declared as a weak symbol, it is overridden if libsrpc is used.
 */
void __srpc_init() __attribute__((weak));

void __srpc_init() {
}
