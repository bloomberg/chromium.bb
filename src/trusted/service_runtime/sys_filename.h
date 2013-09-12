/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_NACL_SYS_FILENAME_H_
#define NATIVE_CLIENT_SERVICE_RUNTIME_NACL_SYS_FILENAME_H_ 1

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/portability.h"

EXTERN_C_BEGIN

struct NaClApp;
struct NaClAppThread;
struct nacl_abi_stat;

int32_t NaClOpenAclCheck(struct NaClApp *nap,
                         char const     *path,
                         int            flags,
                         int            mode);

int32_t NaClStatAclCheck(struct NaClApp *nap,
                         char const     *path);

int32_t NaClSysOpen(struct NaClAppThread  *natp,
                    char                  *pathname,
                    int                   flags,
                    int                   mode);

int32_t NaClSysStat(struct NaClAppThread *natp,
                    const char           *path,
                    struct nacl_abi_stat *nasp);

int32_t NaClSysMkdir(struct NaClAppThread *natp,
                     uint32_t             path,
                     int                  mode);

int32_t NaClSysRmdir(struct NaClAppThread *natp,
                     uint32_t             path);

int32_t NaClSysChdir(struct NaClAppThread *natp,
                     uint32_t             path);

int32_t NaClSysGetcwd(struct NaClAppThread *natp,
                      uint32_t             buffer,
                      int                  len);

int32_t NaClSysUnlink(struct NaClAppThread *natp,
                      uint32_t             path);

EXTERN_C_END

#endif
