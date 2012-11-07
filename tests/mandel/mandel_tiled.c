/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
  simple demo for nacl client, computed mandelbrot set
*/


#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "native_client/src/shared/srpc/nacl_srpc.h"

/*
 * Variables to keep track of the shared memory region.
 */
static size_t shm_length = 0;
static char*  shm_addr = NULL;
static int    shm_desc = -1;

/*
 * The routine to set up the shared memory region to be used by all the calls.
 */

void SetupSharedMemory(NaClSrpcRpc *rpc,
                       NaClSrpcArg **in_args,
                       NaClSrpcArg **out_args,
                       NaClSrpcClosure *done) {
  struct stat st;
  shm_desc = in_args[0]->u.hval;

  rpc->result = NACL_SRPC_RESULT_APP_ERROR;
  /* Determine the size of the region. */
  if (fstat(shm_desc, &st)) {
    done->Run(done);
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
    rpc->result = NACL_SRPC_RESULT_OK;
  }
  done->Run(done);
}

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

/*
 * Sample application-specific RPC code.
 */
static inline int mandel(char* rgb,
                         double i,
                         double j,
                         double points_per_tile,
                         double tiles_per_row) {
  double cx = (4.0 / (points_per_tile * tiles_per_row)) * i - 3.0;
  double cy = 1.5 - (3.0 / (points_per_tile * tiles_per_row)) * j;

  double re = cx;
  double im = cy;

  const double threshold = 1.0e8;

  int r;
  int g;
  int b;

  int count = 0;
  while (count < 256 && re * re + im * im < threshold) {
    double new_re = re * re - im * im + cx;
    double new_im = 2 * re * im + cy;
    re = new_re;
    im = new_im;
    count++;
  }

  if (count < 8) {
    r = 128;
    g = 0;
    b = 0;
  } else if (count < 16) {
    r = 255;
    g = 0;
    b = 0;
  } else if (count < 32) {
    r = 255;
    g = 255;
    b = 0;
  } else if (count < 64) {
    r = 0;
    g = 255;
    b = 0;
  } else if (count < 128) {
    r = 0;
    g = 255;
    b = 255;
  } else if (count < 256) {
    r = 0;
    g = 0;
    b = 255;
  } else {
    r = 0;
    g = 0;
    b = 0;
  }

  return sprintf(rgb, "rgb(%03u,%03u,%03u)", r, g, b);
}


void MandelTiled(NaClSrpcRpc *rpc,
                 NaClSrpcArg **in_args,
                 NaClSrpcArg **out_args,
                 NaClSrpcClosure *done) {
  double xlow = in_args[0]->u.dval;
  double ylow = in_args[1]->u.dval;
  double points_per_tile = in_args[2]->u.dval;
  double tiles_per_row = in_args[3]->u.dval;
  char* p;
  double x;
  double y;

  /*
   * Compute square tiles of the mandelbrot set, leaving the result in
   * the shared memory region.
   */
  p = shm_addr;
  for (x = xlow; x < xlow + points_per_tile; ++x) {
    for (y = ylow; y < ylow + points_per_tile; ++y) {
      p += mandel(p, x, y, points_per_tile, tiles_per_row);
    }
  }
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

const struct NaClSrpcHandlerDesc srpc_methods[] = {
  { "setup:h:", SetupSharedMemory },
  { "shutdown::", ShutdownSharedMemory },
  { "tiled:dddd:", MandelTiled },
  { NULL, NULL },
};

int main(void) {
  if (!NaClSrpcModuleInit()) {
    return 1;
  }
  if (!NaClSrpcAcceptClientConnection(srpc_methods)) {
    return 1;
  }
  NaClSrpcModuleFini();
  return 0;
}
