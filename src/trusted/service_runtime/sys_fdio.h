/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_NACL_SYS_FDIO_H_
#define NATIVE_CLIENT_SERVICE_RUNTIME_NACL_SYS_FDIO_H_ 1

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/service_runtime/include/machine/_types.h"

EXTERN_C_BEGIN

struct NaClApp;
struct NaClAppThread;
struct nacl_abi_stat;

int32_t NaClSysClose(struct NaClAppThread *natp,
                     int                  d);

int32_t NaClSysDup(struct NaClAppThread *natp,
                   int                  oldfd);

int32_t NaClSysDup2(struct NaClAppThread  *natp,
                    int                   oldfd,
                    int                   newfd);

int32_t NaClSysRead(struct NaClAppThread  *natp,
                    int                   d,
                    void                  *buf,
                    size_t                count);

int32_t NaClSysWrite(struct NaClAppThread *natp,
                     int                  d,
                     void                 *buf,
                     size_t               count);

int32_t NaClSysLseek(struct NaClAppThread *natp,
                     int                  d,
                     nacl_abi_off_t       *offp,
                     int                  whence);

int32_t NaClSysFstat(struct NaClAppThread *natp,
                     int                  d,
                     struct nacl_abi_stat *nasp);

int32_t NaClSysGetdents(struct NaClAppThread  *natp,
                        int                   d,
                        void                  *dirp,
                        size_t                count);

int32_t NaClSysIoctl(struct NaClAppThread *natp,
                     int                  d,
                     int                  request,
                     void                 *arg);

EXTERN_C_END

#endif
