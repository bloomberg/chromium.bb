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
 * NaCl kernel / service run-time system call ABI.
 * This file defines nacl target machine dependent types.
 */

#ifndef SERVICE_RUNTIME_INCLUDE_MACHINE__TYPES_H_
#define SERVICE_RUNTIME_INCLUDE_MACHINE__TYPES_H_

#ifdef __native_client__
# include <stdint.h>
# include <machine/_default_types.h>
#else
# include "native_client/src/include/portability.h"
#endif

#define __need_size_t
#include <stddef.h>

#ifndef NULL
#define NULL 0
#endif

/*
 * Some of these use (unsigned) int/long versus int<size>_t.  For
 * reasons why, see the discussion in <bits/types.h> and values in
 * <bits/typesizes.h>
 */
#ifndef nacl_abi___dev_t_defined
#define nacl_abi___dev_t_defined
typedef int64_t       nacl_abi___dev_t;
#ifndef __native_client__
typedef nacl_abi___dev_t nacl_abi_dev_t;
#endif
#endif

#ifndef nacl_abi___ino_t_defined
#define nacl_abi___ino_t_defined
typedef unsigned long nacl_abi___ino_t;
#ifndef __native_client__
typedef nacl_abi___ino_t nacl_abi_ino_t;
#endif
#endif

#ifndef nacl_abi___mode_t_defined
#define nacl_abi___mode_t_defined
typedef uint32_t      nacl_abi___mode_t;
#ifndef __native_client__
typedef nacl_abi___mode_t nacl_abi_mode_t;
#endif
#endif

#ifndef nacl_abi___nlink_t_defined
#define nacl_abi___nlink_t_defined
typedef unsigned int  nacl_abi___nlink_t;
#ifndef __native_client__
typedef nacl_abi___nlink_t nacl_abi_nlink_t;
#endif
#endif

#ifndef nacl_abi___uid_t_defined
#define nacl_abi___uid_t_defined
typedef uint32_t      nacl_abi___uid_t;
#ifndef __native_client__
typedef nacl_abi___uid_t nacl_abi_uid_t;
#endif
#endif

#ifndef nacl_abi___gid_t_defined
#define nacl_abi___gid_t_defined
typedef uint32_t      nacl_abi___gid_t;
#ifndef __native_client__
typedef nacl_abi___gid_t nacl_abi_gid_t;
#endif
#endif

#ifndef nacl_abi___off_t_defined
#define nacl_abi___off_t_defined
typedef long int      nacl_abi__off_t;
#ifndef __native_client__
typedef nacl_abi__off_t nacl_abi_off_t;
#endif
#endif

#ifndef nacl_abi___blksize_t_defined
#define nacl_abi___blksize_t_defined
typedef long int      nacl_abi___blksize_t;
typedef nacl_abi___blksize_t nacl_abi_blksize_t;
#endif

#ifndef nacl_abi___blkcnt_t_defined
#define nacl_abi___blkcnt_t_defined
typedef long int      nacl_abi___blkcnt_t;
typedef nacl_abi___blkcnt_t nacl_abi_blkcnt_t;
#endif

#ifndef nacl_abi___time_t_defined
#define nacl_abi___time_t_defined
typedef int32_t       nacl_abi___time_t;
typedef nacl_abi___time_t nacl_abi_time_t;
#endif

/*
 * stddef.h defines size_t, and we cannot export another definition.
 * see __need_size_t above and stddef.h
 * (BUILD/gcc-4.2.2/gcc/ginclude/stddef.h) contents.
 */
#define NACL_NO_STRIP(t) nacl_ ## abi_ ## t

#ifndef nacl_abi_size_t_defined
#define nacl_abi_size_t_defined
typedef uint32_t NACL_NO_STRIP(size_t);
#endif

#ifndef nacl_abi_ssize_t_defined
#define nacl_abi_ssize_t_defined
typedef int32_t NACL_NO_STRIP(ssize_t);
#endif

#undef NACL_NO_STRIP

#endif
