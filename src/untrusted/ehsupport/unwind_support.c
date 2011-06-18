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

/* This corresponds to: __builtin_init_dwarf_reg_size_table() */
/* TODO(robertm): this needs to be better documented */
void pnacl_unwind_init_dwarf_reg_size_table(unsigned char* table) {
  int i;
  int n = pnacl_unwind_dwarf_frame_registers();
#if defined(__x86_64__)
  /* All regs are 64bit = 8 bytes */
  for (i = 0; i < n; ++i ){
    table[i] = 8;
  }
#elif defined(__i386__)
  /* TODO(robetrm): find better documentation for these values */
  for (i = 0; i <= 9; ++i ){
    table[i] = 4;
  }
  table[10] = 0;   /* not a typo! register not used? */
  for (i = 11; i < n; ++i ){
    table[i] = 12;  /* fp registers ? */
  }
#elif defined(__arm__)
    /* All regs are 32bit = 4 bytes */
    for (i = 0; i < n; ++i ){
    table[i] = 4;
  }
#else
  #error "unknown platform"
#endif
}
