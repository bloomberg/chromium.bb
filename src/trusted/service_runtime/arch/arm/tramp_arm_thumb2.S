/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/nacl_config.h"

        /*
         * This is code, but it is not code that is actually part of the
         * program/library being linked.  Marking it as read-only data
         * instead ensures that nothing like linker code-rewriting will
         * be applied to this code.
         */
        NACL_RODATA

.thumb
.syntax unified
/*
 * Assembly code template.
 * This is linked into the service runtime but is unused as code -- it is used
 * as data to be patched into a NaCl app's address space.
 *
 * Trampoline to transfer control from native client module to
 * sel_ldr's NaClSyscallSeg residing in the service runtime portion of address
 * space. Trampolines are patched into nacl module's address space in the
 * trampoline region. They are patched by NaClLoadTrampoline() code (sel_ldr.c).
 * Each trampoline code segment corresponds to a system call, so the trampoline
 * region is full of identical trampoline code segments. Service runtime
 * distinguish which system call is requested using the address of an executed
 * trampoline (it is saved on stack in NaClSyscallSeg()).
 *
 * ARM passes parameters to a callee in registers r0-r3. If there are more
 * than 4 parameters, the first four args are passed in registers the rest are
 * placed on the stack. This code pushes all parameters from registers into the
 * stack; thus, we keep all parameters on the stack as follows:
 * top - arg0, arg1, arg2, arg3 .... argN
 *
 * On top of that we push the return address, so we will know where to return
 * after the system call.
 *
 * After this code the stack layout must look like:
 *
 * sp+0: retaddr to user module
 * sp+4: arg0 (system call argument)
 * sp+8: arg1
 * ..........
 * sp+X: argN
 *
 * When service runtime serves a system call, it first creates a structure which
 * utilizes these arguments. The structure is created by Decoder functions in
 * nacl_syscall_handlers.c. (nacl_syscall_handlers.c is an automatically
 * generated file and placed in
 * scons-out/gen/native_client/src/trusted/service_runtime).
 *
 */

/*
 * We will load the trampoline to be aligned 14mod16, as a thumb2 branch
 * target must be.  To ensure that the instructions in the trampoline are
 * properly aligned, we misalign by 2 here.
 */
.p2align 4
.skip 2
DEFINE_GLOBAL_HIDDEN_IDENTIFIER(NaCl_trampoline_seg_code):
  push {r0, r1, r2, r3}
  push {lr}

  /* this accesses the data item immediately after the halt */
  ldr r0, NaCl_trampoline_syscall_seg_addr
  /* NOTE: we use the blx instead of bx because the return address */
  /*       is used to determine which trampoline was used */

  blx r0
  /* NORETURN */
  /* 4-byte constants need to be properly aligned for loading. */
.p2align 2
DEFINE_GLOBAL_HIDDEN_IDENTIFIER(NaCl_trampoline_syscall_seg_addr):
  /* a DATA item will be patched in here */
  .word   0

DEFINE_GLOBAL_HIDDEN_IDENTIFIER(NaCl_trampoline_seg_end):
