/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Native Client support for thread local storage
 *
 * Support for TLS in NaCl depends upon the cooperation of the
 * compiler's code generator, the linker (and linker script), the
 * startup code (_start), and the CPU-specific routines in
 * src/untrusted/stubs/tls.c.
 *
 * Each thread is associated with both a TLS region, comprising the
 * .tdata and .tbss (zero-initialized) sections of the ELF file, and a
 * thread descriptor block (TDB), a structure containing information
 * about the thread.  The details of the TLS and TDB regions vary by
 * platform; this module provides a generic abstraction which may be
 * supported by any platform.
 *
 * The "combined area" is an opaque region of memory associated with a
 * thread, containing its TDB and TLS and sufficient internal padding
 * so that it can be allocated anywhere without regard to alignment.
 * The src/untrusted/stubs/tls.c module provides CPU-specific routines
 * for operating on the combined area: determining how much space is
 * needed, finding the address of the TLS data, initializing the TLS
 * area from the template (i.e. ELF image's .tdata and .tbss
 * sections).
 *
 * Each time a thread is created (including the main thread via
 * _start), a combined area is allocated and initialized for it.
 *
 * Once the main thread's TDB area is initialized, a syscall to
 * tls_init() is made to save its location somewhere in the
 * architectural state of the thread, e.g. a segment register or
 * general purpose register; the details vary by platform.  How this
 * is done determines what code the compiler/linker should generate to
 * access a TLS location.  Threads other than the main one are created
 * in this state and have no need for tls_init().
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * Example 1: x86-32.  This diagram shows the combined area:
 *
 *  +-----+-----------------+------+-----+
 *  | pad |  TLS data, bss  | TDB  | pad |
 *  +-----+-----------------+------+-----+
 *                          ^
 *                          |
 *                          +--- %gs
 *
 * The first word of the TDB is its own address, relative to the
 * default segment register (DS).  Negative offsets from %gs will not
 * work since NaCl enforces segmentation violations, so TLS references
 * explicitly retrieve this TLS base pointer and then indirect
 * relative to it (using DS)
 *
 * The padding is distributed so that the TLS and TDB sections are
 * both aligned to 32-bit boundaries.  (The linker ensures that the
 * TLS sections are always 32n bytes.)
 *
 *      mov     %gs:0, %eax              ; load TDB's (DS-)address from TDB.
 *      mov     -0x20(%eax), %ebx        ; load TLS object into ebx
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * Example 2: x86-64.
 *
 * The layout of the combined area is the same as for x86-32; the TDB
 * address is accessed via a intrinsic, __nacl_read_tp().
 *
 *      callq  __nacl_read_tp            ; load TDB address into eax.
 *      mov    -0x20(%r15,%rax,1),%eax   ; sandboxed load from r15 "segment".
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * Example 3: ARM.  The positions of TDB and TLS are reversed;
 * in fact the TDB is the first symbol (an artificially padded one) in
 * the .tdata section.
 *
 *  +-----------+-------+----------------+-----------+
 *  |  padding  |  TDB  , TLS data, bss  |  padding  |
 *  +-----------+-------+----------------+-----------+
 *              ^
 *              |
 *              +--- __aeabi_read_tp()
 *
 * ARM's TLS model is known as the "static (initial exec)" in the ARM
 * docs, e.g. "Addenda to the ABI for the ARM Architecture".  The
 * generated code calls __aeabi_read_tp() to locate the TLS area, then
 * uses positive offsets from this address.  Our implementation of
 * this function computes r9[32:12]-16.  [Not sure why the compiler
 * uses a 16-byte offset.]  The padding is distributed so that the TDB
 * falls on a 4KB page boundary, since with any finer alignment, there
 * aren't enough bits in r9 to avoid collisions
 *
 *      mov     r1, #192          ; offset of symbol within TLS
 *      bl      __aeabi_read_tp
 *      ldr     r2, [r0, r1]
 */

#include <stddef.h>

#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_NACL_TLS_H
#define NATIVE_CLIENT_SRC_UNTRUSTED_NACL_TLS_H

/************************************************************************/
/* Portable functions defined here:                                     */

/* Allocates (using sbrk) and initializes the combined area for the
 * main thread.  Always called, whether or not pthreads is in use.  */
int __pthread_initialize_minimal(size_t tdb_size);

/* Initializes the thread-local data of newlib (which achieves
 * reentrancy by making heavy use of TLS). */
void __newlib_thread_init();

void __newlib_thread_exit();


/* Returns the allocation size of the combined area, including TLS
 * (data + bss), TDB, and padding required to guarantee alignment.
 * Since the TDB layout is plaform-specific, its size is passed as a
 * parameter. */
size_t __nacl_tls_combined_size(size_t tdb_size);

/* Returns the address of the TDB within a combined area. */
void *__nacl_tls_tdb_start(void* combined_area);

/* Initializes the TLS of a combined area by copying the TLS data area
 * from the template (ELF image .tdata) and zeroing the TLS BSS area. */
void __nacl_tls_data_bss_initialize_from_template(void* combined_area);

/*
 * Fill in the memory of the size __nacl_tls_combined_size returned.
 * Returns the pointer that should go in the thread register.
 */
void *__nacl_tls_initialize_memory(void *combined_area);

/* Read the per-thread pointer.  */
void *__nacl_read_tp(void);

/************************************************************************/
/* Functions defined in libplatform.a (src/untrusted/stubs/tls.c):      */

/*
 * NOTE: PNaCl replaces these functions by intrinsics in the compiler.
 * If you make changes to these functions, please also change the code in
 * hg/llvm-gcc/.../gcc/builtins.def
 * hg/llvm/.../include/llvm/Intrinsics.td
 * hg/llvm/.../lib/Target/ARM/ARMInstrInfo.td
 * hg/llvm/.../lib/Target/X86/X86InstrNaCl.td
 */

/* Platform specific alignment for tls segment */
int __nacl_tls_aligment();

/* Offset in bytes of the tdb from beginning of tls segment, given tls size */
size_t __nacl_tdb_offset_in_tls(size_t tls_data_and_bss_size);

/* Size in bytes of the tdb when considered as part of the tls segment */
size_t __nacl_tdb_effective_payload_size(size_t tdb_size);

/* Size in bytes of the return address required at the top of the main
 * ELF thread's stack. */
size_t __nacl_return_address_size();

#endif /* NATIVE_CLIENT_SRC_UNTRUSTED_NACL_TLS_H */
