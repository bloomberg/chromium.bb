/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
  simple demo for nacl client, computed mandelbrot set
*/

#include <string.h>

#include "native_client/src/shared/srpc/nacl_srpc.h"

/*
 * Sample application-specific RPC code.
 */

void Mandel(NaClSrpcRpc *rpc,
            NaClSrpcArg **in_args,
            NaClSrpcArg **out_args,
            NaClSrpcClosure *done) {
  double i = in_args[0]->u.dval;
  double j = in_args[1]->u.dval;
  double xsteps = in_args[2]->u.dval;
  double ysteps = in_args[3]->u.dval;

  double cx = (4.0/xsteps) * i - 3.0;
  double cy = 1.5 - (3.0/ysteps) * j;

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

  out_args[0]->u.ival = r;
  out_args[1]->u.ival = g;
  out_args[2]->u.ival = b;
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

const struct NaClSrpcHandlerDesc srpc_methods[] = {
  { "mandel:dddd:iii", Mandel },
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
