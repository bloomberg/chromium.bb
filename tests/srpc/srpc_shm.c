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


/* NativeClient shared memory handle return test. */

#include <errno.h>
#include <nacl/nacl_srpc.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/nacl_syscalls.h>

/*
 * GetShmHandle creates and returns a shared memory region.
 */
NaClSrpcError GetShmHandle(NaClSrpcChannel *channel,
                           NaClSrpcArg **in_args,
                           NaClSrpcArg **out_args) {
  int desc;
  size_t size = in_args[0]->u.ival;
  /* Start up banner, clear errors. */
  printf("GetShmHandle: size %d\n", size);
  /* Create an IMC shared memory region. */
  desc = imc_mem_obj_create(size);
  if (desc >= 0) {
    char* map_addr;
    map_addr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, desc, 0);
    if (MAP_FAILED == map_addr) {
      printf("GetShmHandle: mmap failed\n");
      /* TODO(sehr): close handle */
      desc = -1;
    }
  } else {
    printf("GetShmHandle: imc_mem_obj_create failed %d\n", desc);
  }
  /* Return the shm descriptor. */
  out_args[0]->u.hval = desc;
  return NACL_SRPC_RESULT_OK;
}

NACL_SRPC_METHOD("get_shm_handle:i:h", GetShmHandle);

/*
 * GetInvalidHandle creates and returns a shared memory region.
 */
NaClSrpcError GetInvalidHandle(NaClSrpcChannel *channel,
                               NaClSrpcArg **in_args,
                               NaClSrpcArg **out_args) {
  /* Return an invalid shm descriptor. */
  out_args[0]->u.ival = -1;
  return NACL_SRPC_RESULT_OK;
}

NACL_SRPC_METHOD("get_invalid_handle::h", GetInvalidHandle);

/*
 * MapSysvHandle maps a SysV shared memory handle and prints its contents.
 */
NaClSrpcError MapSysvHandle(NaClSrpcChannel *channel,
                            NaClSrpcArg **in_args,
                            NaClSrpcArg **out_args) {
  int desc = in_args[0]->u.hval;
  char* map_addr;
  struct stat st;
  size_t size;

  /* Get the region's size. */
  if (0 != fstat(desc, &st)) {
    printf("MapSysvHandle: fstat failed\n");
  }
  size = (size_t) st.st_size;
  /* Map it into the module's address space. */
  map_addr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, desc, 0);
  if (MAP_FAILED == map_addr) {
    printf("MapSysvHandle: mmap failed\n");
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  /* Print the region's contents. */
  printf("string '%s'\n", map_addr);
  /* Unmap the region. */
  return NACL_SRPC_RESULT_OK;
}

NACL_SRPC_METHOD("map_sysv_handle:h:", MapSysvHandle);
