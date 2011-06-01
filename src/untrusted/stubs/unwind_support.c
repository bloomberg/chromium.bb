/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This file contains primitives used by libgcc_eh.
 * They have been moved here so that libgcc_eh can be build an
 * in architecture neutral way, i.e. bitcode.
 *
 * Eventually these functions should become intrinsics handled by llc.
 */

/* This corresponds to: __builtin_dwarf_sp_column */
/* TODO(robertm): this needs to be better documented */
int pnacl_unwind_dwarf_sp_column() {
#if defined(__x86_64__)
  return 7;
#elif defined(__i386__)
  return 4;
#elif defined(__arm__)
  return 13;
#else
  #error "unknown platform"
  return -1;
#endif
}

/* This corresponds to:  __builtin_eh_return_data_regno(0)  */
/* TODO(robertm): this needs to be better documented */
int pnacl_unwind_result0_reg() {
#if defined(__x86_64__)
  return 0;
#elif defined(__i386__)
  return 0;
#elif defined(__arm__)
  return 4;
#else
  #error "unknown platform"
  return -1;
#endif
}

/* This corresponds to:   __builtin_eh_return_data_regno(1) */
/* TODO(robertm): this needs to be better documented */
int pnacl_unwind_result1_reg() {
#if defined(__x86_64__)
  return 1;
#elif defined(__i386__)
  return 2;
#elif defined(__arm__)
  return 5;
#else
  #error "unknown platform"
  return -1;
#endif
}

/* This corresponds to: DWARF_FRAME_REGISTERS */
/* TODO(robertm): this needs to be better documented */
int pnacl_unwind_dwarf_frame_registers() {
#if defined(__x86_64__)
  return 17;
#elif defined(__i386__)
  return 17;
#elif defined(__arm__)
  return 16;
#else
  #error "unknown platform"
  return -1;
#endif
}
