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

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_util.h"


uint16_t NaClGetCs(void) {
  uint16_t seg1;

  __asm mov seg1, cs;
  return seg1;
}

/* there is no SetCS -- this is done via far jumps/calls */

uint16_t NaClGetDs(void) {
  uint16_t seg1;

  __asm mov seg1, ds;
  return seg1;
}


void NaClSetDs(uint16_t  seg1) {
  __asm mov ds, seg1;
}


uint16_t NaClGetEs(void) {
  uint16_t seg1;

  __asm mov seg1, es;
  return seg1;
}


void NaClSetEs(uint16_t  seg1) {
  __asm mov es, seg1;
}


uint16_t NaClGetFs(void) {
  uint16_t seg1;

  __asm mov seg1, fs;
  return seg1;
}


void NaClSetFs(uint16_t  seg1) {
  __asm mov fs, seg1;
}


uint16_t NaClGetGs(void) {
  uint16_t seg1;

  __asm mov seg1, gs;
  return seg1;
}


void NaClSetGs(uint16_t seg1) {
  __asm mov gs, seg1;
}


uint16_t NaClGetSs(void) {
  uint16_t seg1;

  __asm mov seg1, ss;
  return seg1;
}


uint32_t NaClGetEsp(void) {
  uint32_t stack_ptr;

  _asm mov stack_ptr, esp;
  return stack_ptr;
}


uint32_t NaClGetEbx(void) {
  uint32_t result;

  _asm mov result, ebx;
  return result;
}


/*
 * this function is OS-independent as well as all above; however, because of
 * different assembly syntax it must be split into linux and win versions
 */
tick_t get_ticks() {
  tick_t  t = 0;
  uint32_t  t_high, t_low;

  __asm rdtsc;
  __asm mov t_high, edx;
  __asm mov t_low, eax;
  t = (((tick_t) t_high) << 32) | t_low;
  return t;
}

