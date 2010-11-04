/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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

void SetupSharedMemory(NaClSrpcRpc *rpc,
                       NaClSrpcArg **in_args,
                       NaClSrpcArg **out_args,
                       NaClSrpcClosure* done) {
  struct stat st;
  shm_desc = in_args[0]->u.hval;

  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  /* Determine the size of the region. */
  if (fstat(shm_desc, &st)) {
    done->Rpc(done);
    return;
  }
  /* Map the shared memory region into the NaCl module's address space. */
  shm_length = (size_t) st.st_size;
  shm_addr = (char*) mmap(NULL,
                          shm_length,
                          PROT_READ | PROT_WRITE,
                          MAP_SHARED,
                          shm_desc,
                          0);
  if (MAP_FAILED != shm_addr) {
    g_BigBuffer.buf = shm_addr;
    g_BigBuffer.max = shm_length;
    rpc->result = NACL_SRPC_RESULT_OK;
  }
  done->Rpc(done);
}
NACL_SRPC_METHOD("setup:h:", SetupSharedMemory);

void ShutdownSharedMemory(NaClSrpcRpc *rpc,
                          NaClSrpcArg **in_args,
                          NaClSrpcArg **out_args,
                          NaClSrpcClosure *done) {
  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  /* Unmap the memory region. */
  if (munmap((void*) shm_addr, shm_length)) {
    done->Run(done);
    return;
  }
  /* Close the passed-in descriptor. */
  if (close(shm_desc)) {
    done->Run(done);
    return;
  }
  /* Return success. */
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}
NACL_SRPC_METHOD("shutdown::", ShutdownSharedMemory);
