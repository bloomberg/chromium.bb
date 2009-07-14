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
 * Test code for NaCl local descriptor table (LDT) managment
 */
#include <stdio.h>
#include "native_client/src/trusted/service_runtime/nacl_ldt.h"

#if defined (HAVE_SDL)
#include <SDL.h>
#endif

int main(int argc, char* argv[]) {
  uint16_t a, b, c, d, e;
  /* Initialize LDT services. */
  NaClLdtInit();

  /* Data, not read only */
  a = NaClLdtAllocatePageSelector(NACL_LDT_DESCRIPTOR_DATA, 0, 0, 0x000ff);
  printf("a = %0x\n", a);
  NaClLdtPrintSelector(a);

  /* Data, read only */
  b = NaClLdtAllocatePageSelector(NACL_LDT_DESCRIPTOR_DATA, 1, 0, 0x000ff);
  printf("b = %0x\n", b);
  NaClLdtPrintSelector(b);

  /* Data, read only */
  c = NaClLdtAllocatePageSelector(NACL_LDT_DESCRIPTOR_DATA, 1, 0, 0x000ff);
  printf("c = %0x\n", c);
  NaClLdtPrintSelector(c);

  /* Delete b */
  NaClLdtDeleteSelector(b);
  printf("b (after deletion) = %0x\n", b);
  NaClLdtPrintSelector(b);

  /* Since there is only one thread, d should grab slot previously holding b */
  d = NaClLdtAllocatePageSelector(NACL_LDT_DESCRIPTOR_DATA, 1, 0, 0x000ff);
  printf("d = %0x\n", d);
  NaClLdtPrintSelector(d);

  /* Code selector */
  e = NaClLdtAllocatePageSelector(NACL_LDT_DESCRIPTOR_CODE, 1, 0, 0x000ff);
  printf("e (code) = %0x\n", e);
  NaClLdtPrintSelector(e);

  /* Shut down LDT services. */
  NaClLdtFini();
  return 0;
}
