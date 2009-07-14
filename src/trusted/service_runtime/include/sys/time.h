/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * NaCl Service Runtime API.  Time types.
 */

#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_INCLUDE_SYS_TIME_H_
#define NATIVE_CLIENT_SERVICE_RUNTIME_INCLUDE_SYS_TIME_H_

#include "native_client/src/trusted/service_runtime/include/machine/_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef long int  nacl_abi_suseconds_t;

#ifndef nacl_abi___clock_t_defined
#define nacl_abi___clock_t_defined
typedef long int  nacl_abi_clock_t;  /* to be deprecated */
#endif

struct nacl_abi_timeval {
  nacl_abi_time_t      nacl_abi_tv_sec;
  nacl_abi_suseconds_t nacl_abi_tv_usec;
};

struct nacl_abi_timespec {
  nacl_abi_time_t    tv_sec;
  long int           tv_nsec;
};

/* obsolete.  should not be used */
struct nacl_abi_timezone {
  int tz_minuteswest;
  int tz_dsttime;
};

/*
 * In some places (e.g., the linux man page) the second parameter is defined
 * as a struct timezone *.  The header file says this struct type should
 * never be used, and defines it by default as void *.  The Mac man page says
 * it is void *.
 */
extern int nacl_abi_gettimeofday (struct nacl_abi_timeval *tv, void *tz);

#ifdef __cplusplus
}
#endif

#endif
