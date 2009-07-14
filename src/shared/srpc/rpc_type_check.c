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
 * NaCl service library.  a primitive rpc library
 */

#include <string.h>
#include "nacl_srpc.h"
#include "nacl_srpc_internal.h"

int NaClSrpcTypeCheckOne(char const          **pp,
                         struct NaClSrpcArg  **alist)
{
  char const  *p;

  for (p = *pp; *p != '\0' && *p != ':'; ++p, ++alist) {
    if (!*alist) {
      return 0;
    }
    /* this code could be more compact by using a 256 entry table */
    switch (*p) {
      case NACL_SRPC_ARG_TYPE_BOOL:
      case NACL_SRPC_ARG_TYPE_INT:
      case NACL_SRPC_ARG_TYPE_DOUBLE:
      case NACL_SRPC_ARG_TYPE_STRING:
      case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
      case NACL_SRPC_ARG_TYPE_INT_ARRAY:
      case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
      case NACL_SRPC_ARG_TYPE_HANDLE:
        if ((*alist)->tag != *p) {
          return 0;
        }
        break;
      /*
       * The two cases below are added to avoid warnings, they are only used
       * in the plugin code
       */
      case NACL_SRPC_ARG_TYPE_OBJECT:
      case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
      default:
        /* internal error! */
        return 0;
    }
  }
  if (0 != *alist) {
    /* too many arguments */
    return 0;
  }
  *pp = p;
  return 1;
}

int NaClSrpcTypeCheckArgs(char const         *fmt,
                          struct NaClSrpcArg **in_args,
                          struct NaClSrpcArg **out_args)
{
  char const  *p;

  /* skip over rpc name */
  p = strchr(fmt, ':');
  if (!p) {
    return 0;
  }
  ++p;  /* skip separator */
  if (!NaClSrpcTypeCheckOne(&p, in_args)) {
    return 0;
  }
  if (*p != ':') {
    return 0;
  }
  ++p;
  if (!NaClSrpcTypeCheckOne(&p, out_args)) {
    return 0;
  }
  return 1;
}
