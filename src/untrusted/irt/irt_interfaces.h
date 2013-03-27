/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_IRT_IRT_INTERFACES_H_
#define NATIVE_CLIENT_SRC_UNTRUSTED_IRT_IRT_INTERFACES_H_

#include <stddef.h>

extern size_t nacl_irt_interface(const char *interface_ident,
                                 void *table, size_t tablesize);

extern const struct nacl_irt_basic nacl_irt_basic;
extern const struct nacl_irt_fdio nacl_irt_fdio;
extern const struct nacl_irt_filename nacl_irt_filename;
extern const struct nacl_irt_memory_v0_2 nacl_irt_memory;
extern const struct nacl_irt_dyncode nacl_irt_dyncode;
extern const struct nacl_irt_thread nacl_irt_thread;
extern const struct nacl_irt_mutex nacl_irt_mutex;
extern const struct nacl_irt_cond nacl_irt_cond;
extern const struct nacl_irt_sem nacl_irt_sem;
extern const struct nacl_irt_tls nacl_irt_tls;
extern const struct nacl_irt_blockhook nacl_irt_blockhook;
extern const struct nacl_irt_ppapihook nacl_irt_ppapihook;
extern const struct nacl_irt_resource_open nacl_irt_resource_open;
extern const struct nacl_irt_random nacl_irt_random;
extern const struct nacl_irt_clock nacl_irt_clock;
extern const struct nacl_irt_dev_getpid nacl_irt_dev_getpid;
extern const struct nacl_irt_dev_exception_handling
    nacl_irt_dev_exception_handling;

#endif  /* NATIVE_CLIENT_SRC_UNTRUSTED_IRT_IRT_INTERFACES_H_ */
