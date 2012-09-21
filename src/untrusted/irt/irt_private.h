/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_IRT_IRT_PRIVATE_H_
#define NATIVE_CLIENT_SRC_UNTRUSTED_IRT_IRT_PRIVATE_H_

extern __thread int g_is_main_thread;
extern __thread int g_is_irt_internal_thread;

int irt_nameservice_lookup(const char *name, int oflag, int *out_fd);

#endif  /* NATIVE_CLIENT_SRC_UNTRUSTED_IRT_IRT_PRIVATE_H_ */
