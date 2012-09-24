/*
 * Copyright 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef SERVICE_RUNTIME_ARCH_MIPS_SEL_LDR_H__
#define SERVICE_RUNTIME_ARCH_MIPS_SEL_LDR_H__ 1

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"

#define NACL_MAX_ADDR_BITS      30

#define NACL_ADDRSPACE_LOWER_GUARD_SIZE 0
#define NACL_ADDRSPACE_UPPER_GUARD_SIZE 0x2000

/* Must be synced with irt_compatible_rodata_addr in SConstruct */
#define NACL_DATA_SEGMENT_START 0x10000000

#define NACL_THREAD_MAX         8192

#define NACL_NOOP_OPCODE        0x00000000  /* nop */
#define NACL_HALT_OPCODE        0x00000008  /* jr $0 */
#define NACL_HALT_LEN           4           /* length of halt instruction */
#define NACL_HALT_WORD          NACL_HALT_OPCODE

#endif /* SERVICE_RUNTIME_ARCH_MIPS_SEL_LDR_H__ */
