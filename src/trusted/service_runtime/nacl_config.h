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
 * NaCl Simple/secure ELF loader (NaCl SEL).
 */
#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_NACL_CONFIG_H_
#define NATIVE_CLIENT_SERVICE_RUNTIME_NACL_CONFIG_H_

#include "native_client/src/include/nacl_base.h"

/*
 * this value must be consistent with NaCl compiler flags
 * -falign-functions -falign-labels -and nacl-align.
 */
#if defined(NACL_BLOCK_SHIFT)
# define NACL_INSTR_BLOCK_SHIFT       (NACL_BLOCK_SHIFT)
#else
# error "NACL_BLOCK_SHIFT should be defined by CFLAGS"
#endif
#define NACL_INSTR_BLOCK_SIZE         (1 << NACL_INSTR_BLOCK_SHIFT)

/* this must be a multiple of the system page size */
#define NACL_PAGESHIFT                12
#define NACL_PAGESIZE                 (1U << NACL_PAGESHIFT)

#define NACL_MAP_PAGESHIFT            16
#define NACL_MAP_PAGESIZE             (1U << NACL_MAP_PAGESHIFT)

#if NACL_MAP_PAGESHIFT < NACL_PAGESHIFT
# error "NACL_MAP_PAGESHIFT smaller than NACL_PAGESHIFT"
#endif

/* NACL_MAP_PAGESIFT >= NACL_PAGESHIFT must hold */
#define NACL_PAGES_PER_MAP            (1 << (NACL_MAP_PAGESHIFT-NACL_PAGESHIFT))

#define NACL_MEMORY_ALLOC_RETRY_MAX   256 /* see win/sel_memory.c */

/*
 * NACL_KERN_STACK_SIZE: The size of the secure stack allocated for
 * use while a NaCl thread is in kernel mode, i.e., during
 * startup/exit and during system call processing.
 */
#define NACL_KERN_STACK_SIZE          (32 << 10)

/*
 * NACL_CONFIG_PATH_MAX: What is the maximum file path allowed in the
 * NaClSysOpen syscall?  This is on the kernel stack for validation
 * during the processing of the syscall, so beware running into
 * NACL_KERN_STACK_SIZE above.
 */
#define NACL_CONFIG_PATH_MAX          1024

/*
 * Macro for the start address of the trampolines.
 * The first 64KB (16 pages) are inaccessible to prevent addr16/data16 attacks.
 */
#define NACL_SYSCALL_START_ADDR       (16 << NACL_PAGESHIFT)
/* Macro for the start address of a specific trampoline.  */
#define NACL_SYSCALL_ADDR(syscall_number) \
    (NACL_SYSCALL_START_ADDR + (syscall_number << NACL_INSTR_BLOCK_SHIFT))

/*
 * Syscall trampoline code have a block size that may differ from the
 * alignment restrictions of the executable.  The ELF executable's
 * alignment restriction (16 or 32) defines what are potential entry
 * points, so the trampoline region must still respect that.  We
 * prefill the trampoline region with HLT, so non-syscall entry points
 * will not cause problems as long as our trampoline instruction
 * sequence never grows beyond 16 bytes, so the "odd" potential entry
 * points for a 16-byte aligned ELF will not be able to jump into the
 * middle of the trampoline code.
 */
#define NACL_SYSCALL_BLOCK_SHIFT      5
#define NACL_SYSCALL_BLOCK_SIZE       (1 << NACL_SYSCALL_BLOCK_SHIFT)

/*
 * the extra space for the trampoline syscall code and the thread
 * contexts must be a multiple of the page size.
 *
 * The address space begins with a 64KB region that is inaccessible to
 * handle NULL pointers and also to reinforce protection agasint abuse of
 * addr16/data16 prefixes.
 * NACL_TRAMPOLINE_START gives the address of the first trampoline.
 * NACL_TRAMPOLINE_END gives the address of the first byte after the
 * trampolines.
 */
#define NACL_NULL_REGION_SHIFT  16
#define NACL_TRAMPOLINE_START   (1 << NACL_NULL_REGION_SHIFT)
#define NACL_TRAMPOLINE_SHIFT   16
#define NACL_TRAMPOLINE_SIZE    (1 << NACL_TRAMPOLINE_SHIFT)
#define NACL_TRAMPOLINE_END     (NACL_TRAMPOLINE_START + NACL_TRAMPOLINE_SIZE)
/*
 * macros to provide uniform access to identifiers from assembly due
 * to different C -> asm name mangling conventions and other platform-specific
 * requirements
 */
#if NACL_WINDOWS || NACL_OSX
# define IDENTIFIER(n)  _##n
#else /* Linux */
# define IDENTIFIER(n)  n
#endif

#if NACL_OSX
# define HIDDEN(n)  .private_extern IDENTIFIER(n)
#elif NACL_LINUX
# define HIDDEN(n)  .hidden IDENTIFIER(n)
#else /* Windows */
/* On Windows, symbols are hidden by default. */
# define HIDDEN(n)
#endif

#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86

#if NACL_BUILD_SUBARCH == 32
#define NACL_USERRET_FIX  0x8
#elif NACL_BUILD_SUBARCH == 64
#define NACL_USERRET_FIX  0xc
#else /* NACL_BUILD_SUBARCH */
#error Unknown platform!
#endif /* NACL_BUILD_SUBARCH */

#elif NACL_ARCH(NACL_BUILD_ARCH) == NACL_arm

#define NACL_HALT         mov pc, #0
/* 16-byte bundles, 256MB code segment*/
#define NACL_CONTROL_FLOW_MASK      0xF000000F

#define NACL_USERRET_FIX  0x4
/* TODO(robertm): unify this with NACL_BLOCK_SHIFT */
/* 16 byte bundles */
#define  NACL_ARM_BUNDLE_SIZE_LOG 4
#else /* NACL_ARCH(NACL_BUILD_ARCH) */

#error Unknown platform!

#endif /* NACL_ARCH(NACL_BUILD_ARCH) */

#define NACL_SYSARGS_FIX  NACL_USERRET_FIX + 0x4

#endif  /* NATIVE_CLIENT_SERVICE_RUNTIME_NACL_CONFIG_H_ */
