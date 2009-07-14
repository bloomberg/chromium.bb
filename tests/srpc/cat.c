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


#include <stdio.h>
#include <stdlib.h>
#include <nacl/nacl_srpc.h>
#include <sys/stat.h>

int Cat(NaClSrpcChannel *channel,
        NaClSrpcArg **in_args,
        NaClSrpcArg **out_args);

NACL_SRPC_METHOD("cat:h:C", Cat);

int Cat(NaClSrpcChannel *channel,
        NaClSrpcArg **in_args,
        NaClSrpcArg **out_args) {
  int         fd;
  struct stat stb;
  FILE        *iob;
  int         ch;
  int         nchar;
  int         bufsize;

  fd = in_args[0]->u.hval;
  printf("CatFile: Got fd %d\n", fd);
  if (-1 != fstat(fd, &stb)) {
#define P(fmt, field) do { printf(#field " = " fmt "\n", (int) stb.field); } while (0)
    P("0x%04x", st_mode);
    P("0x%x", st_nlink);
    P("0x%x", st_size);
    P("%d", st_size);
#undef P
  }
  printf("fdopening\n");
  iob = fdopen(fd, "r");
  if (NULL == iob) {
    printf("fdopen failed");
    return NACL_SRPC_RESULT_APP_ERROR;
  }
  bufsize = out_args[0]->u.caval.count;
  printf("read loop, up to %d chars\n", bufsize);
  for (nchar = 0; EOF != (ch = getc(iob)) && nchar < bufsize-1; ++nchar) {
    out_args[0]->u.caval.carr[nchar] = ch;
    putchar(ch);
  }
  out_args[0]->u.caval.carr[nchar] = '\0';
  printf("EOF\n");
  fclose(iob);
  printf("got %d bytes\n", nchar);
  printf("out param: %.*s\n", nchar, out_args[0]->u.caval.carr);
  return NACL_SRPC_RESULT_OK;
}
