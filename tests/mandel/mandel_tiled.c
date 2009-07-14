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
  simple demo for nacl client, computed mandelbrot set
*/


#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <nacl/nacl_srpc.h>

/*
 * Variables to keep track of the shared memory region.
 */
static size_t shm_length = 0;
static char*  shm_addr = NULL;
static int    shm_desc = -1;

/*
 * The routine to set up the shared memory region to be used by all the calls.
 */

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


int MandelTiled(NaClSrpcChannel *channel,
                NaClSrpcArg** in_args,
                NaClSrpcArg** out_args) {
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
  return NACL_SRPC_RESULT_OK;
}

NACL_SRPC_METHOD("tiled:dddd:", MandelTiled);
