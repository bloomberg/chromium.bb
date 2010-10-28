/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/* NativeClient shared memory handle return test. */

#include <errno.h>
#include <nacl/nacl_srpc.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/nacl_syscalls.h>
#include <unistd.h>

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
      if (close(desc) < 0) {
        printf("GetShmHandle: close failed\n");
      }
      desc = -1;
    }
  } else {
    printf("GetShmHandle: imc_mem_obj_create failed %d\n", desc);
  }
  /* Return the shm descriptor. */
  out_args[0]->u.hval = desc;
  return NACL_SRPC_RESULT_OK;
}

/*
 * GetInvalidHandle returns an invalid shared memory descriptor.
 */
NaClSrpcError GetInvalidHandle(NaClSrpcChannel *channel,
                               NaClSrpcArg **in_args,
                               NaClSrpcArg **out_args) {
  /* Return an invalid shm descriptor. */
  out_args[0]->u.ival = -1;
  return NACL_SRPC_RESULT_OK;
}

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

const struct NaClSrpcHandlerDesc srpc_methods[] = {
  { "get_shm_handle:i:h", GetShmHandle },
  { "get_invalid_handle::h", GetInvalidHandle },
  { "map_sysv_handle:h:", MapSysvHandle },
  { NULL, NULL },
};

int main() {
  if (!NaClSrpcModuleInit()) {
    return 1;
  }
  if (!NaClSrpcAcceptClientConnection(srpc_methods)) {
    return 1;
  }
  NaClSrpcModuleFini();
  return 0;
}
