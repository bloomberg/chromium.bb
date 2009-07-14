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
 * NaCl testing shell (ported to RPC library)
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/time.h>

#if !NACL_WINDOWS
#include <unistd.h>
#endif

#include "nacl_srpc.h"
#include "nacl_srpc_internal.h"

int __GetRpcNum(const NaClSrpcDesc* rpc_desc, int num_rpc, char const* name) {
  int i;
  for (i = 0; i < num_rpc;  ++i) {
    if (!strcmp(name, rpc_desc[i].rpc_name)) {
      return i;
    }
  }
  return -1;
}

/*  simple destructive tokenizer */
typedef struct {
  const char* start;
  int length;
} TOKEN;


/* expects *from to point to leading \" and returns pointer to trailing \" */
const char* __ScanEscapeString(char* to, const char* from) {
  assert (*from == '\"');
  from++;
  while (*from) {
    if (*from == '\"') {
      if (to) *to = '\0';
      return from;
    }else if (*from != '\\') {
      if (to) *to++ = *from;
      from++;
    } else {
      char next = *from++;
      switch(next) {
       case '\0':
        return 0;
       case '\\':
       case '\"':
        if (to) *to++ = next;
        break;
       case 'n':
        if (to) *to++ = '\n';
        break;
       default:
        return 0;
      }
    }
  }
  return 0;
}

int __Tokenize(char* line, TOKEN *array, int n) {
  int pos_start = 0;
  int count = 0;

  for( ; count < n; count++ ) {
    int pos_end;

    /* skip leading white space */
    while (line[pos_start]) {
      const char c = line[pos_start];
      if (isspace(c)) {
        pos_start++;
      } else {
        break;
      }
    }

    if (!line[pos_start]) break;

    /* find token end from current pos_start */
    pos_end = pos_start;

    while (line[pos_end]) {
      const char c = line[pos_end];

      if (isspace(c)) {
        break;
      } else if (c == '\"') {
        const char* end = __ScanEscapeString(0, &line[pos_end]);
        if (!end) return -1;
        pos_end = end - &line[0];
      }
      pos_end++;
    }

    /* save the token */
    array[count].start = &line[pos_start];
    array[count].length = pos_end - pos_start;

    if (line[pos_end]) {
      line[pos_end] = '\0';   /* DESTRUCTION!!! */
      pos_end++;
    }
    pos_start = pos_end;
    /* printf("TOKEN %s\n", array[count].start); */
  }

  return count;
}

/* NOTE: This is leaking memory left and right */
int __ParseArg(NaClSrpcArg* arg, const char* token) {
  long val;
  int dim;
  const char* comma;
  int         i;

  dprintf(("TOKEN %s\n", token));
  assert(token[1] == '(');

  switch (token[0]) {
   case NACL_SRPC_ARG_TYPE_INVALID:
    arg->tag = NACL_SRPC_ARG_TYPE_INVALID;
    break;
   case NACL_SRPC_ARG_TYPE_BOOL:
    val = strtol(&token[2], 0, 0);
    arg->tag = NACL_SRPC_ARG_TYPE_BOOL;
    arg->u.bval = val;
    break;
   case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
    dim = strtol(&token[2], 0, 0);
    arg->tag = NACL_SRPC_ARG_TYPE_CHAR_ARRAY;
    arg->u.caval.carr = (char*) calloc(dim, sizeof(char));
    arg->u.caval.count = dim;
    comma = strstr(token, ",");
    if (comma) {
      const char* p;
      for (p = comma+1, i = 0; *p != ')' && i < dim; ++p, ++i)
        arg->u.caval.carr[i] = *p;
    }
    break;
   case NACL_SRPC_ARG_TYPE_DOUBLE:
    arg->tag = NACL_SRPC_ARG_TYPE_DOUBLE;
    arg->u.dval = strtod(&token[2], 0);
    break;
   case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
    dim = strtol(&token[2], 0, 0);
    arg->tag = NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY;
    /* LEAK */
    arg->u.daval.darr = (double*) calloc(dim, sizeof(double));
    assert(arg->u.daval.darr);
    arg->u.daval.count = dim;
    comma = token;
    for (i = 0; i < dim; ++i) {
      comma = strstr(comma, ",");
      if (!comma) break;
      ++comma;
      arg->u.daval.darr[i] = strtod(comma, 0);
    }
    break;
   case NACL_SRPC_ARG_TYPE_HANDLE:
    /* TODO(sehr): get handle passing to work. */
    val = strtol(&token[2], 0, 0);
    arg->tag = NACL_SRPC_ARG_TYPE_HANDLE;
    arg->u.hval = val;
    break;
   case NACL_SRPC_ARG_TYPE_INT:
    val = strtol(&token[2], 0, 0);
    arg->tag = NACL_SRPC_ARG_TYPE_INT;
    arg->u.ival = val;
    break;
   case NACL_SRPC_ARG_TYPE_INT_ARRAY:
    dim = strtol(&token[2], 0, 0);
    arg->tag = NACL_SRPC_ARG_TYPE_INT_ARRAY;
    /* LEAK */
    arg->u.iaval.iarr = (int*) calloc(dim, sizeof(int));
    assert(arg->u.iaval.iarr);
    arg->u.iaval.count = dim;
    comma = token;
    for (i = 0; i < dim; ++i) {
      comma = strstr(comma, ",");
      if (!comma) break;
      ++comma;
      arg->u.iaval.iarr[i] = strtol(comma, 0, 0);
    }
    break;
   case NACL_SRPC_ARG_TYPE_STRING:
    arg->tag = NACL_SRPC_ARG_TYPE_STRING;
    /* this is a conservative estimate */
    arg->u.sval = malloc(strlen(token));
    if (NULL == arg->u.sval) {
      return -1;
    }
    __ScanEscapeString(arg->u.sval, token + 2);
    break;
    /*
     * The two cases below are added to avoid warnings, they are only used
     * in the plugin code
     */
   case NACL_SRPC_ARG_TYPE_OBJECT:
   case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
   default:
    return -1;
  }

  return 1;
}

int __ParseArgs(NaClSrpcArg* arg, const TOKEN* token, int n) {
  int i;
  for (i = 0; i < n; ++i) {
    if (__ParseArg(&arg[i], token[i].start) < 0)
      return -1;
  }
  return n;
}

void __DumpInterfaceDescription(const NaClSrpcDesc* rpc_desc, int num_rpc) {
  int i;
  printf("RPC %-20s %-10s %-10s\n", "Name", "Input args", "Output args");
  for (i = 0; i < num_rpc; ++i) {
    printf("%3d %-20s %-10s %-10s\n",
           i, rpc_desc[i].rpc_name,
           rpc_desc[i].in_args, rpc_desc[i].out_args);
  }
}


void __DumpArg(const NaClSrpcArg* arg) {
  int count;
  int i;

  switch(arg->tag) {
   case NACL_SRPC_ARG_TYPE_INVALID:
    printf("X()");
    break;
   case NACL_SRPC_ARG_TYPE_BOOL:
    printf("b(%d)", arg->u.bval);
    break;
   case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
    for (i = 0; i < arg->u.caval.count; i++)
      putchar(arg->u.caval.carr[i]);
    break;
   case NACL_SRPC_ARG_TYPE_DOUBLE:
    printf("d(%f)", arg->u.dval);
    break;
   case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
    count = arg->u.daval.count;
    printf("D(%d", count);
    for (i=0; i < count; ++i)
      printf(",%f", arg->u.daval.darr[i]);
    printf(")");
    break;
   case NACL_SRPC_ARG_TYPE_HANDLE:
    printf("h(%d)", arg->u.hval);
    break;
   case NACL_SRPC_ARG_TYPE_INT:
    printf("i(%d)", arg->u.ival);
    break;
   case NACL_SRPC_ARG_TYPE_INT_ARRAY:
    count = arg->u.iaval.count;
    printf("I(%d", count);
    for (i=0; i < count; ++i)
      printf(",%d", arg->u.iaval.iarr[i]);
    printf(")");
    break;
   case NACL_SRPC_ARG_TYPE_STRING:
    /* TODO: do proper escaping */
    printf("s(\"%s\")", arg->u.sval);
    break;
    /*
     * The two cases below are added to avoid warnings, they are only used
     * in the plugin code
     */
   case NACL_SRPC_ARG_TYPE_OBJECT:
   case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
   default:
    break;
  }
}

void __DumpArgs(const NaClSrpcArg* arg, int n) {
  int i;
  for (i=0; i<n; ++i) {
    printf("  ");
    __DumpArg(&arg[i]);
  }
  printf("\n");
}

NaClSrpcError __CommandLoop() {
  int errcode;
  const NaClSrpcDesc* rpc_desc;
  struct NaClSrpcHandlerDesc* handlers;
  char* service_str;
  uint32_t num_rpc;
  int command_count = 0;
  size_t length;

  /* Build the complete method table */
  handlers = __NaClSrpcCompleteMethodTable(__kNaClSrpcHandlers, &num_rpc);
  /* Build the service discovery string */
  service_str = __NaClSrpcBuildSDString(handlers, num_rpc, &length);
  /* Build the descriptor table from the string */
  rpc_desc = __NaClSrpcBuildSrpcDesc(service_str, &num_rpc);

  /* process commands from stdin and dispatch */
  for (;;) {
    char        buffer[4096];
    TOKEN       tokens[64];
    int         n;
    const char  *command;

    fprintf(stderr, "%d> ", command_count);
    ++command_count;

    if (!fgets(buffer, sizeof(buffer), stdin))
      break;

    n = __Tokenize(buffer, tokens, 64);

    if (n < 1) {
      if (n < 0)
        fprintf(stderr, "bad line\n");
      continue;
    }

    command =  tokens[0].start;
    if (0 == strcmp("#", command)) {
      continue;
    } else if (0 == strcmp("service", command)) {
      __DumpInterfaceDescription(rpc_desc, num_rpc);
    } else if (0 == strcmp("quit", command)) {
      break;
    } else if (0 == strcmp("rpc", command)) {
      int          int_out_sep;
      int          n_in;
      NaClSrpcArg  in[NACL_SRPC_MAX_ARGS];
      NaClSrpcArg  *inv[NACL_SRPC_MAX_ARGS+1];
      int          i;
      int          n_out;
      NaClSrpcArg  out[NACL_SRPC_MAX_ARGS];
      NaClSrpcArg  *outv[NACL_SRPC_MAX_ARGS+1];
      int          rpc_num;

      if (n < 2) {
        fprintf(stderr, "bad rpc command\n");
        continue;
      }

      for (int_out_sep = 2; int_out_sep < n; ++int_out_sep) {
        if (0 == strcmp(tokens[int_out_sep].start, "*"))
          break;
      }

      if (int_out_sep == n) {
        fprintf(stderr, "no in out arg separator for rpc command\n");
        continue;
      }

      n_in = int_out_sep - 2;
      dprintf(("parsing in args %d\n", n_in));
      assert(n_in < NACL_SRPC_MAX_ARGS);
      for (i = 0; i < n_in; ++i)  inv[i] = &in[i];
      inv[n_in] = 0;

      if (__ParseArgs(in, &tokens[2], n_in) < 0) {
        fprintf(stderr, "bad input args for rpc\n");
        continue;
      }

      n_out =  n - int_out_sep - 1;
      dprintf(("parsing out args %d\n", n_out));
      assert(n_out < NACL_SRPC_MAX_ARGS);
      for (i = 0; i < n_out; ++i)  outv[i] = &out[i];
      outv[n_out] = 0;

      if (__ParseArgs(out, &tokens[int_out_sep + 1], n_out) < 0) {
        fprintf(stderr, "bad output args for rpc\n");
        continue;
      }

      rpc_num = __GetRpcNum(rpc_desc, num_rpc, tokens[1].start);
      if (rpc_num < 0) {
        fprintf(stderr, "unknown rpc\n");
        continue;
      }

      fprintf(stderr, "using rpc %s no %d\n", tokens[1].start, rpc_num);
      errcode =  __NaClSrpcMain(rpc_num, handlers, num_rpc, NULL, inv, outv);
      if (NACL_SRPC_RESULT_OK != errcode) {
        fprintf(stderr, "rpc call failed %d\n", errcode);
        continue;
      }

      /* dump result vector */
      printf("%s RESULTS: ", tokens[1].start);
      __DumpArgs(outv[0], n_out);
    } else {
        fprintf(stderr, "unknown command\n");
        continue;
    }
  }
  return NACL_SRPC_RESULT_OK;
}
