/*
 * Copyright 2009, Google Inc.
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
 * NaCl tests for simple syscalls not using newlib
 */


#include <bits/mman.h>
#include <sys/nacl_syscalls.h>


int mystrlen(const char* s) {
  int count = 0;
  while(*s++) ++count;
  return count;
}

#define myprint(s) write(1, s, mystrlen(s))

void hextochar(int n, char buffer[9]) {
  int i;
  buffer[8] = 0;

  for (i=0; i < 8; ++i) {
    int nibble = 0xf & (n >> (4 * (7 - i)));
    if (nibble <= 9) {
      buffer[i] = nibble + '0';
    } else {
      buffer[i] = nibble - 10 + 'A';
    }
  }
}


void Error(const char* message) {
  myprint("ERROR: ");
  myprint(message);
  _exit(-1);
}

const int kExitOk = 69;
const int kSysbreakBase = 0x100000;
const int kMmapBase = 0x200000;
const int kMmapSize = 0x10000;

int main() {
  int  i;
  char buffer[16];

  myprint("before null_syscall\n");
  null_syscall();

  myprint("before sysbreak(0)\n");
  i = (int) sysbrk(0);
  hextochar(i, buffer);
  myprint(buffer);
  myprint("\n");

  myprint("before sysbrk(0x100000)\n");
  i = (int) sysbrk((void *) kSysbreakBase);
  hextochar(i, buffer);
  myprint(buffer);
  myprint("\n");
  if (kSysbreakBase != i) Error("bad sysbrk() value\n");

  myprint("before clock()\n");
  myprint("# ");   /* add comment file so we can filter it out in golden file */
  i = clock();
  hextochar(i, buffer);
  myprint(buffer);
  myprint("\n");

  myprint("before srpc_get_fd()\n");
  i = srpc_get_fd();
  hextochar(i, buffer);
  myprint(buffer);
  myprint("\n");
  if (69 != i) Error("bad  srpc_get_fd() value\n");

  myprint("mmap()\n");
  i = (int) mmap(0, kMmapSize,  PROT_READ, 0, 0, 0);
  hextochar(i, buffer);
  myprint(buffer);
  myprint("\n");
#if 0
  if (-1 == i) Error("bad mmap() value\n");
#endif

  myprint("mmap()\n");
  i = (int) mmap((void *)kMmapBase, kMmapSize,  PROT_READ, 0, 0, 0);
  hextochar(i, buffer);
  myprint(buffer);
  myprint("\n");
#if 0
  if (kMmapBase != i ) Error("bad mmap() value\n");
#endif

  myprint("before _exit()\n");
  _exit(kExitOk);

  return 0;
}
