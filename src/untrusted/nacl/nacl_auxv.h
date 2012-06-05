/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_NACL_AUXV_H_
#define NATIVE_CLIENT_SRC_UNTRUSTED_NACL_AUXV_H_

#include "native_client/src/include/elf32.h"

/*
 * This is stashed in _start for convenient later use.
 */
extern Elf32_auxv_t *__nacl_auxv;

#endif  /* NATIVE_CLIENT_SRC_UNTRUSTED_NACL_AUXV_H_ */
