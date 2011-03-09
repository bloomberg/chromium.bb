/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


/* NativeClient shared memory handle return test. */

#include <errno.h>
#include <nacl/nacl_log.h>
#include <nacl/nacl_srpc.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/nacl_syscalls.h>
#include <unistd.h>

/*
 * GetShmHandle creates and returns a shared memory region.
 */
void GetShmHandle(NaClSrpcRpc *rpc,
                  NaClSrpcArg **in_args,
                  NaClSrpcArg **out_args,
                  NaClSrpcClosure *done) {
  int desc;
  size_t size = in_args[0]->u.ival;
  /* Start up banner, clear errors. */
  NaClLog(LOG_INFO, "GetShmHandle: size %d\n", size);
  /* Create an IMC shared memory region. */
  desc = imc_mem_obj_create(size);
  if (desc >= 0) {
    char* map_addr;
    map_addr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, desc, 0);
    if (MAP_FAILED == map_addr) {
      NaClLog(LOG_ERROR, "GetShmHandle: mmap failed\n");
      if (close(desc) < 0) {
        NaClLog(LOG_ERROR, "GetShmHandle: close failed\n");
      }
      desc = -1;
    }
  } else {
    NaClLog(LOG_ERROR, "GetShmHandle: imc_mem_obj_create failed %d\n", desc);
  }
  /* Return the shm descriptor. */
  out_args[0]->u.hval = desc;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

/*
 * GetInvalidHandle returns an invalid shared memory descriptor.
 */
void GetInvalidHandle(NaClSrpcRpc *rpc,
                      NaClSrpcArg **in_args,
                      NaClSrpcArg **out_args,
                      NaClSrpcClosure *done) {
  /* Return an invalid shm descriptor. */
  out_args[0]->u.ival = -1;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

/*
 * MapHandle maps a SysV shared memory handle and prints its contents.
 */
void MapHandle(NaClSrpcRpc *rpc,
                   NaClSrpcArg **in_args,
                   NaClSrpcArg **out_args,
                   NaClSrpcClosure *done) {
  int desc = in_args[0]->u.hval;
  char* map_addr;
  struct stat st;
  size_t size;

  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  /* Get the region's size. */
  if (0 != fstat(desc, &st)) {
    NaClLog(LOG_ERROR, "MapHandle: fstat failed\n");
  }
  size = (size_t) st.st_size;
  /* Map it into the module's address space. */
  map_addr = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, desc, 0);
  if (MAP_FAILED == map_addr) {
    NaClLog(LOG_ERROR, "MapHandle: mmap failed\n");
    done->Run(done);
    return;
  }
  /* Print the region's contents. */
  NaClLog(LOG_INFO, "string '%s'\n", map_addr);
  /* TODO(sehr): Unmap the region. */
  /* Return the region's contents. */
  out_args[0]->arrays.str = strdup(map_addr);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

const struct NaClSrpcHandlerDesc srpc_methods[] = {
  { "get_shm_handle:i:h", GetShmHandle },
  { "get_invalid_handle::h", GetInvalidHandle },
  { "map_handle:h:s", MapHandle },
  { NULL, NULL },
};

int main() {
  NaClLogModuleInit();
  if (!NaClSrpcModuleInit()) {
    return 1;
  }
  if (!NaClSrpcAcceptClientConnection(srpc_methods)) {
    return 1;
  }
  NaClSrpcModuleFini();
  return 0;
}
