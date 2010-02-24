/*
 * Copyright 2009 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_ELF_UTIL_H__
#define NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_ELF_UTIL_H__ 1

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/service_runtime/nacl_error_code.h"

struct NaClElfImage;
struct Gio;

uintptr_t NaClElfImageGetEntryPoint(struct NaClElfImage *image);

int NaClElfImageGetBundleSize(struct NaClElfImage *image);

struct NaClElfImage *NaClElfImageNew(struct Gio *gp, NaClErrorCode *err_code);

NaClErrorCode NaClElfImageValidateElfHeader(struct NaClElfImage *image);

NaClErrorCode NaClElfImageValidateProgramHeaders(
  struct NaClElfImage *image,
  uint8_t             addr_bits,
  uintptr_t           *static_text_end,
  uintptr_t           *rodata_start,
  uintptr_t           *rodata_end,
  uintptr_t           *data_start,
  uintptr_t           *data_end,
  uintptr_t           *max_vaddr);

NaClErrorCode NaClElfImageLoad(struct NaClElfImage *image,
                               struct Gio          *gp,
                               uint8_t             addr_bits,
                               uintptr_t           mem_start);


NaClErrorCode NaClElfImageValidateAbi(struct NaClElfImage *image);

void NaClElfImageDelete(struct NaClElfImage *image);


#endif  /* NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_ELF_UTIL_H__ */
