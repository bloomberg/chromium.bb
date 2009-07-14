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
 * cdomshm.h - some helper stuff for sharing C-generated HTML with JavaScript
 *
 * These structs are used to manage a shared memory buffer as a big text
 * buffer that grows through append operations.
 */
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <nacl/nacl_srpc.h>
#include "native_client/tests/cdom_perf/cdomshm.h"

/* Helper variable for NaCl shared memory link */
BigBuffer g_BigBuffer = { NULL, 0, 0, 0 };

/* BBWrite() appends text to end of the BigBuffer. bb will commonly be
 * a pointer to g_BigBuffer.
 */
void BBWrite(const char *s, BigBuffer *bb) {
  int len = strlen(s);

  if (bb->index + len > bb->max) {
    bb->failed = 1;
  } else {
    memcpy(&(bb->buf[bb->index]), s, len);
    bb->index += len;
    bb->buf[bb->index] = '\0';
  }
}

/* This shared memory code was lifed from tests/mandel/mandel_tiled.c */

/* Variables to keep track of the shared memory region. */
static size_t shm_length = 0;
static char*  shm_addr = NULL;
static int    shm_desc = -1;

int SetupSharedMemory(NaClSrpcChannel *channel,
                      NaClSrpcArg **in_args,
                      NaClSrpcArg **out_args) {
  struct stat st;
  shm_desc = in_args[0]->u.hval;

  /* Determine the size of the region. */
  if (fstat(shm_desc, &st)) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  /* Map the shared memory region into the NaCl module's address space. */
  shm_length = (size_t) st.st_size;
  shm_addr = (char*) mmap(NULL,
                          shm_length,
                          PROT_READ | PROT_WRITE,
                          MAP_SHARED,
                          shm_desc,
                          0);
  if (MAP_FAILED == shm_addr) {
    return NACL_SRPC_RESULT_APP_ERROR;
  } else {
    g_BigBuffer.buf = shm_addr;
    g_BigBuffer.max = shm_length;
  }
  return NACL_SRPC_RESULT_OK;
}
NACL_SRPC_METHOD("setup:h:", SetupSharedMemory);

int ShutdownSharedMemory(NaClSrpcChannel *channel,
                         NaClSrpcArg **in_args,
                         NaClSrpcArg **out_args) {
  /* Unmap the memory region. */
  if (munmap((void*) shm_addr, shm_length)) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  /* Close the passed-in descriptor. */
  if (close(shm_desc)) {
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  /* Return success. */
  return NACL_SRPC_RESULT_OK;
}
NACL_SRPC_METHOD("shutdown::", ShutdownSharedMemory);
