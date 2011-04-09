/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_IRT_IRT_ELF_UTILS_H_
#define NATIVE_CLIENT_SRC_UNTRUSTED_IRT_IRT_ELF_UTILS_H_ 1

#include <stdint.h>

struct auxv_entry {
  uint32_t a_type;
  uint32_t a_val;
};

void *__find_auxv(int argc, char **argv);
struct auxv_entry *__find_auxv_entry(struct auxv_entry *auxv, uint32_t key);

#endif
