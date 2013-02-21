/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"

#if !defined(__pnacl__) && (defined(__x86_64__) || defined(__i386__))

typedef struct { char b[10]; } st_reg;
static const st_reg st_zero;

typedef struct {
  uint32_t cwd;
  uint32_t swd;
  uint32_t twd;
  uint32_t fip;
  uint32_t fcs;
  uint32_t foo;
  uint32_t fos;
  st_reg st[8];
} fsave_block;

/*
 * Not every x86-32 CPU supports SSE registers, but we're willing
 * to assume that machines we run this test on do.
 */
typedef struct { uint64_t b[2]; } xmm_reg __attribute__((aligned(16)));
static const xmm_reg xmm_zero;

static void infoleak_clear_state(void) {
  const uint32_t zero = 0;
  fsave_block fsave;
  __asm__ volatile("fninit; fsave %0" : "=m" (fsave));
  memset(fsave.st, 0, sizeof(fsave.st));
  __asm__ volatile("frstor %0" :: "m" (fsave));
  __asm__ volatile("movaps %0, %%xmm7" :: "m" (xmm_zero));
  __asm__ volatile("ldmxcsr %0" :: "m" (zero));
}

__attribute__((noinline)) static int infoleak_check_state(void) {
  int ok = 1;
  fsave_block fsave;
  uint64_t mm7;
  xmm_reg xmm7;
  uint32_t mxcsr;
  int i;
  __asm__("fsave %0" : "=m" (fsave));
  __asm__("movq %%mm7, %0" : "=m" (mm7));
  __asm__("movaps %%xmm7, %0" : "=m" (xmm7));
  __asm__("stmxcsr %0" : "=m" (mxcsr));
  for (i = 0; i < 8; ++i) {
    if (memcmp(&fsave.st[i], &st_zero, sizeof(st_zero)) != 0) {
      printf("x87 %%st(%d) leaked information!\n", i);
      ok = 0;
    }
  }
  if (mm7 != 0) {
    printf("MMX state leaked information!\n");
    ok = 0;
  }
  if (memcmp(&xmm7, &xmm_zero, sizeof(xmm7)) != 0) {
    printf("SSE state leaked information!\n");
    ok = 0;
  }
  if (mxcsr != 0) {
    printf("MXCSR state leaked information!\n");
    ok = 0;
  }
  return ok;
}

#define EXPECT_OK 1

#elif defined(__arm__)

union vfp_regs {
  double d[16];
  char s[sizeof(double) * 16];
};
const union vfp_regs vfp_zero;

/*
 * These are defined in test_infoleak_asm.S because LLVM
 * cannot handle them as inline assembly.
 */
uint32_t infoleak_store_state(const union vfp_regs *vfp, uint32_t fpscr);
uint32_t infoleak_fetch_state(union vfp_regs *vfp);

static void infoleak_clear_state(void) {
  infoleak_store_state(&vfp_zero, 0);
}

static int infoleak_check_state(void) {
  int ok = 1;
  union vfp_regs vfp_now;
  uint32_t fpscr = infoleak_fetch_state(&vfp_now);
  if (memcmp(&vfp_now, &vfp_zero, sizeof(vfp_now)) != 0) {
    printf("VFP registers leaked information!\n\t%.*s",
           sizeof(vfp_now), vfp_now.s);
    ok = 0;
  }
  if (fpscr != 0) {
    printf("VFP FPSCR leaked information! %#x\n", fpscr);
    ok = 0;
  }
  return ok;
}

#define EXPECT_OK 1

#else

static void infoleak_clear_state(void) {
}

static int infoleak_check_state(void) {
  return 1;
}

#define EXPECT_OK 1

#endif

int main(void) {
  int result;
  int ok;

  infoleak_clear_state();

  result = NACL_SYSCALL(test_infoleak)();
  if (result != -ENOSYS) {
    printf("test_infoleak syscall returned %d\n", result);
    return 1;
  }

  ok = infoleak_check_state();

  return ok == EXPECT_OK ? 0 : 1;
}
