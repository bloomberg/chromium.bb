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
 * SRPC utility functions.
 */

#ifndef __native_client__
#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"
#endif  /* __native_client__ */

#if defined(__native_client__) || !NACL_WINDOWS
#include <unistd.h>
#include <sys/time.h>
#else
#include <time.h>
#include <windows.h>
#endif

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/shared/srpc/nacl_srpc_internal.h"

int gNaClSrpcDebugPrintEnabled = -1;

/*
 * Currently only looks for presence of NACL_SRPC_DEBUG and returns 0
 * if absent and 1 if present.  In the future we may include notions
 * of verbosity level.
 */
int __NaClSrpcDebugPrintCheckEnv() {
  char *env = getenv("NACL_SRPC_DEBUG");
  return (NULL != env);
}

/*
 * Get the printable form of an error code.
 */
char* NaClSrpcErrorString(NaClSrpcError error_code) {
  switch (error_code) {
   case NACL_SRPC_RESULT_OK:
     return "No error";
   case NACL_SRPC_RESULT_BREAK:
     return "Break out of server RPC loop";
   case NACL_SRPC_RESULT_MESSAGE_TRUNCATED:
     return "Received message was shorter than expected";
   case NACL_SRPC_RESULT_NO_MEMORY:
     return "Out of memory";
   case NACL_SRPC_RESULT_PROTOCOL_MISMATCH:
     return "Client and server have different protocol versions";
   case NACL_SRPC_RESULT_BAD_RPC_NUMBER:
     return "No method for the given rpc number";
   case NACL_SRPC_RESULT_BAD_ARG_TYPE:
     return "Bad argument type received";
   case NACL_SRPC_RESULT_TOO_MANY_ARGS:
     return "Too many arguments (more than NACL_SRPC_MAX_ARGS or declared)";
   case NACL_SRPC_RESULT_TOO_FEW_ARGS:
     return "Too few arguments (fewer than declared)";
   case NACL_SRPC_RESULT_IN_ARG_TYPE_MISMATCH:
     return "Input argument type mismatch";
   case NACL_SRPC_RESULT_OUT_ARG_TYPE_MISMATCH:
     return "Output argument type mismatch";
   case NACL_SRPC_RESULT_INTERNAL:
     return "Internal error in rpc method";
   case NACL_SRPC_RESULT_APP_ERROR:
     return "Rpc application returned an error";
   default:
     break;
  }
  return "Unrecognized NaClSrpcError value";
}

#if !defined(__native_client__) && NACL_WINDOWS

double __NaClSrpcGetUsec() {
  /* TODO(sehr): need to add a Windows specific version of gettimeofday. */
  /* TODO(sehr): one version of this code. */
  unsigned __int64 timer = 0;
  long sec;
  long usec;
  FILETIME filetime;
  GetSystemTimeAsFileTime(&filetime);
  timer |= filetime.dwHighDateTime;
  timer <<= 32;
  timer |= filetime.dwLowDateTime;
  /* FILETIME has 100ns precision.  Convert to usec. */
  timer /= 10;
  sec = (long) (timer / 1000000UL);
  usec = (long) (timer % 1000000UL);
  return (double) sec * 1.0e6 + (double) usec;
}

#else

double __NaClSrpcGetUsec() {
  int retval;
  struct timeval tv;
  retval = gettimeofday(&tv, NULL);
  return (double) tv.tv_sec * 1.0e6 + (double) tv.tv_usec;
}

#endif  /* NACL_WINDOWS */
