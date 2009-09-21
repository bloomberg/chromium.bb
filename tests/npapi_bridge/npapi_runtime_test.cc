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


#include <assert.h>
#if NACL_WINDOWS
#include <windows.h>
#endif
#if NACL_LINUX || NACL_OSX || defined(__native_client__)
#include <unistd.h>
#include <sys/stat.h>
#endif
#include <stdio.h>

#ifdef __native_client__
#include <nacl/nacl_imc.h>
#include <nacl/nacl_npapi.h>
#else
#include "native_client/src/shared/imc/nacl_imc.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#endif  // __native_client__

int main(int argc, char* argv[]) {
#ifndef __native_client__
  int debug = 0;
  while (debug) {
    fprintf(stderr, ".");
#if NACL_WINDOWS
    Sleep(1000);
#else
    sleep(1);
#endif
  }
#endif

#ifdef __native_client__
  printf("pid: %d %u\n", getpid(), sizeof(struct stat));

  NPVariant v;
  HANDLE_TO_NPVARIANT(0, v);
  assert(NPVARIANT_IS_HANDLE(v));
  if (NPVARIANT_IS_HANDLE(v)) {
    nacl::Handle handle = NPVARIANT_TO_HANDLE(v);
    assert(handle == 0);
  }
#endif

  NaClNP_Init(&argc, argv);
  NaClNP_MainLoop(0);
  return 0;
}
