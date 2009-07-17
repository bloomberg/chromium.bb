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
 * NaCl Runtime.
 */

#include <stdint.h>

#include "native_client/src/trusted/service_runtime/sel_rt.h"


uint16_t NaClGetCs(void) {
  uint16_t seg1;

  asm("mov %%cs, %0;" : "=r" (seg1) : );
  return seg1;
}

/* NOTE: there is no SetCS -- this is done via far jumps/calls */


uint16_t NaClGetDs(void) {
  uint16_t seg1;

  asm("mov %%ds, %0" : "=r" (seg1) : );
  return seg1;
}


void NaClSetDs(uint16_t   seg1) {
  asm("movw %0, %%ds;" : : "r" (seg1));
}


uint16_t NaClGetEs(void) {
  uint16_t seg1;

  asm("mov %%es, %0" : "=r" (seg1) : );
  return seg1;
}


void NaClSetEs(uint16_t   seg1) {
  asm("movw %0, %%es;" : : "r" (seg1));
}


uint16_t NaClGetFs(void) {
  uint16_t seg1;

  asm("mov %%fs, %0" : "=r" (seg1) : );
  return seg1;
}


void NaClSetFs(uint16_t seg1) {
  asm("movw %0, %%fs;" : : "r" (seg1));
}


uint16_t NaClGetGs(void) {
  uint16_t seg1;

  asm("mov %%gs, %0" : "=r" (seg1) : );
  return seg1;
}


void NaClSetGs(uint16_t seg1) {
  asm("movw %0, %%gs;" : : "r" (seg1));
}


uint16_t NaClGetSs(void) {
  uint16_t seg1;

  asm("mov %%ss, %0" : "=r" (seg1) : );
  return seg1;
}


uint32_t NaClGetEsp(void) {
  uint32_t esp;

  asm("movl %%esp, %0" : "=r" (esp) : );
  return esp;
}


uint32_t NaClGetEbx(void) {
  uint32_t  ebx;

  asm("movl %%ebx, %0" : "=r" (ebx) : );
  return ebx;
}
