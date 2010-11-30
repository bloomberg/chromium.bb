/*
 * Copyright 2010 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Tests that the PNaCl frontend does not spuriously emit warnings about
 * inline asm when the asm is actually portable.
 */

int llvm_cas(volatile int*, int, int) asm("llvm.atomic.cmp.swap.i32");

volatile int x = 0;
int y = 0;
int z = 1;

/* TODO: can longer blocks of llvm asm be added? */

int main(int argc, char *argv[]) {
  llvm_cas(&x, y, z);
  return x * 55;
}
