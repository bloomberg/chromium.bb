/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Some POSIX calls are implemented in NaCl libraries in ways incompatible with
 * assumptions that programs make about the POSIX environment. To avoid naming
 * collisions all POSIX calls are preprocessed with the macros (below). Hence, a
 * NaCl module that uses libposix_over_srpc should be compiled with additional
 * parameters: -imacros <path to this header>
 */

#ifndef _UNTRUSTED_POSIX_OVER_SRPC_INCLUDE_CALLS_MACROS_H_
#define _UNTRUSTED_POSIX_OVER_SRPC_INCLUDE_CALLS_MACROS_H_

#pragma GCC system_header

/*
 * TODO(mikhailt): eliminate these macro definitions when nacl-glibc is
 * ready.
 */
#define open(...) nacl_open(__VA_ARGS__)
#define opendir(...) nacl_opendir(__VA_ARGS__)
#define close(...) nacl_close(__VA_ARGS__)
#define closedir(...) nacl_closedir(__VA_ARGS__)
#define readdir(...) nacl_readdir(__VA_ARGS__)

#endif  /* _UNTRUSTED_POSIX_OVER_SRPC_INCLUDE_CALLS_MACROS_H_ */
