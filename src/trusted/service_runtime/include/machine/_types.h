/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl kernel / service run-time system call ABI.
 * This file defines nacl target machine dependent types.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_INCLUDE_MACHINE__TYPES_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SERVICE_RUNTIME_INCLUDE_MACHINE__TYPES_H_

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

#ifndef nacl_abi___dev_t_defined
#define nacl_abi___dev_t_defined
typedef int64_t nacl_abi___dev_t;
#ifndef __native_client__
typedef nacl_abi___dev_t nacl_abi_dev_t;
#endif
#endif

#ifndef nacl_abi___ino_t_defined
#define nacl_abi___ino_t_defined
typedef uint64_t nacl_abi___ino_t;
#ifndef __native_client__
typedef nacl_abi___ino_t nacl_abi_ino_t;
#endif
#endif

#ifndef nacl_abi___mode_t_defined
#define nacl_abi___mode_t_defined
typedef uint32_t nacl_abi___mode_t;
#ifndef __native_client__
typedef nacl_abi___mode_t nacl_abi_mode_t;
#endif
#endif

#ifndef nacl_abi___nlink_t_defined
#define nacl_abi___nlink_t_defined
typedef uint32_t nacl_abi___nlink_t;
#ifndef __native_client__
typedef nacl_abi___nlink_t nacl_abi_nlink_t;
#endif
#endif

#ifndef nacl_abi___uid_t_defined
#define nacl_abi___uid_t_defined
typedef uint32_t nacl_abi___uid_t;
#ifndef __native_client__
typedef nacl_abi___uid_t nacl_abi_uid_t;
#endif
#endif

#ifndef nacl_abi___gid_t_defined
#define nacl_abi___gid_t_defined
typedef uint32_t nacl_abi___gid_t;
#ifndef __native_client__
typedef nacl_abi___gid_t nacl_abi_gid_t;
#endif
#endif

#ifndef nacl_abi___off_t_defined
#define nacl_abi___off_t_defined
typedef int64_t nacl_abi__off_t;
#ifndef __native_client__
typedef nacl_abi__off_t nacl_abi_off_t;
#endif
#endif

#ifndef nacl_abi___off64_t_defined
#define nacl_abi___off64_t_defined
typedef int64_t nacl_abi__off64_t;
#ifndef __native_client__
typedef nacl_abi__off64_t nacl_abi_off64_t;
#endif
#endif


#if !(defined(__GLIBC__) && defined(__native_client__))

#ifndef nacl_abi___blksize_t_defined
#define nacl_abi___blksize_t_defined
typedef int32_t nacl_abi___blksize_t;
typedef nacl_abi___blksize_t nacl_abi_blksize_t;
#endif

#endif


#ifndef nacl_abi___blkcnt_t_defined
#define nacl_abi___blkcnt_t_defined
typedef int32_t nacl_abi___blkcnt_t;
typedef nacl_abi___blkcnt_t nacl_abi_blkcnt_t;
#endif

#ifndef nacl_abi___time_t_defined
#define nacl_abi___time_t_defined
typedef int64_t       nacl_abi___time_t;
typedef nacl_abi___time_t nacl_abi_time_t;
#endif

#ifndef nacl_abi___timespec_defined
#define nacl_abi___timespec_defined
struct nacl_abi_timespec {
  nacl_abi_time_t tv_sec;
#ifdef __native_client__
  long int        tv_nsec;
#else
  int32_t         tv_nsec;
#endif
};
#endif

#endif
