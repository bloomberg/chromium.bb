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
 * NaCl testing shell
 */

#include "native_client/src/include/portability.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <signal.h>

#if NACL_WINDOWS
#include <process.h>
static int gettimeofday(struct timeval *tv, struct timezone *tz);
#else
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "native_client/src/shared/imc/nacl_imc_c.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/shared/platform/nacl_host_desc.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher_c.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/nacl_check.h"

#define MAX_ARRAY_SIZE                  4096
#define MAX_COMMAND_LINE_ARGS           256
#define SEL_LDR_CMD_LINE_EXEC_POS       4

static int timed_rpc_count;
static uint32_t timed_rpc_method = 1;
static int timed_rpc_bytes = 4000;

void blackhole(char *s, ...) {}
/* #define DEBUG printf */
#define DEBUG blackhole

/* Table for keeping track of descriptors passed to/from sel_universal */
typedef struct DescList DescList;
struct DescList {
  DescList* next;
  int number;
  const char* print_name;
  struct NaClDesc* desc;
};
static DescList* descriptors = NULL;

int AddDescToList(struct NaClDesc* new_desc, const char* print_name) {
  static int next_desc = 0;
  DescList* new_list_element;
  DescList** list_pointer;

  /* Return an error if passed a null descriptor */
  if (NULL == new_desc) {
    return -1;
  }
  /* Create a descriptor list node. */
  new_list_element = (DescList*) malloc(sizeof(DescList));
  if (NULL == new_list_element) {
    return -1;
  }
  new_list_element->number = next_desc++;
  new_list_element->desc = new_desc;
  new_list_element->print_name = print_name;
  new_list_element->next = NULL;
  /* Find the end of the list of descriptors*/
  for (list_pointer = &descriptors;
       NULL != *list_pointer;
       list_pointer = &(*list_pointer)->next) {
  }
  *list_pointer = new_list_element;
  return new_list_element->number;
}

struct NaClDesc* DescFromPlatformDesc(int fd, int mode) {
  return (struct NaClDesc*) NaClDescIoDescMake(NaClHostDescPosixMake(fd, mode));
}

void BuildDefaultDescList() {
  AddDescToList(DescFromPlatformDesc(0, NACL_ABI_O_RDONLY), "stdin");
  AddDescToList(DescFromPlatformDesc(1, NACL_ABI_O_WRONLY), "stdout");
  AddDescToList(DescFromPlatformDesc(2, NACL_ABI_O_WRONLY), "stderr");
}

void PrintDescList() {
  DescList* list;

  printf("Descriptors:\n");
  for (list = descriptors; NULL != list; list = list->next) {
    printf("  %d: %s\n", list->number, list->print_name);
  }
}

struct NaClDesc* LookupDesc(int num) {
  DescList* list;

  for (list = descriptors; NULL != list; list = list->next) {
    if (num == list->number) {
      return list->desc;
    }
  }
  return NULL;
}

/*  simple destructive tokenizer */
typedef struct {
  const char* start;
  int length;
} TOKEN;

/* expects *from to point to leading \" and returns pointer to trailing \" */
const char* ScanEscapeString(char* to, const char* from) {
  CHECK(*from == '\"');
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

int Tokenize(char* line, TOKEN *array, int n) {
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
        const char* end = ScanEscapeString(0, &line[pos_end]);
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
int ParseArg(NaClSrpcArg* arg, const char* token) {
  long val;
  int dim;
  const char* comma;
  int         i;

  DEBUG("TOKEN %s\n", token);
  CHECK(token[1] == '(');

  /* TODO(sehr,robertm): All the array versions leak memory.  Fix them. */
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
    arg->u.daval.darr = (double*) calloc(dim, sizeof(double));
    CHECK(arg->u.daval.darr);
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
    val = strtol(&token[2], 0, 0);
    arg->tag = NACL_SRPC_ARG_TYPE_HANDLE;
    arg->u.hval = LookupDesc(val);
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
    CHECK(arg->u.iaval.iarr);
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
    ScanEscapeString(arg->u.sval, token + 2);
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

int ParseArgs(NaClSrpcArg* arg, const TOKEN* token, int n) {
  int i;
  for (i = 0; i < n; ++i) {
    if (ParseArg(&arg[i], token[i].start) < 0)
      return -1;
  }
  return n;
}

void DumpArg(const NaClSrpcArg* arg) {
  unsigned int count;
  unsigned int i;

  switch(arg->tag) {
   case NACL_SRPC_ARG_TYPE_INVALID:
    printf("X()");
    break;
   case NACL_SRPC_ARG_TYPE_BOOL:
    printf("b(%d)", arg->u.bval);
    break;
   case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
    for (i = 0; i < arg->u.caval.count; ++i)
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
    printf("h(%"PRIuPTR")", (uintptr_t) AddDescToList(arg->u.hval, "imported"));
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
    /* TODO(robertm): do proper escaping */
    printf("s(\"%s\")", arg->u.sval);
    break;
   case NACL_SRPC_ARG_TYPE_OBJECT:
     /* this is a pointer that NaCl module can do nothing with */
     printf("o(%p)", arg->u.oval);
     break;
   case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
     count = arg->u.vaval.count;
     printf("A(%d", count);
     for (i=0; i < count; ++i) {
       printf(",");
       DumpArg(&arg->u.vaval.varr[i]);
     }
     printf(")");
     break;
   default:
    break;
  }
}

void DumpArgs(const NaClSrpcArg* arg, int n) {
  int i;
  for (i=0; i<n; ++i) {
    printf("  ");
    DumpArg(&arg[i]);
  }
  printf("\n");
}

void BuildArgVec(NaClSrpcArg* argv[], NaClSrpcArg arg[], int count) {
  int i;
  CHECK(count < NACL_SRPC_MAX_ARGS);
  for (i = 0; i < count; ++i) {
    argv[i] = &arg[i];
  }
  argv[count] = NULL;
}

void FreeArrayArgs(NaClSrpcArg arg[], int count) {
  int i;
  for (i = 0; i < count; ++i) {
    switch(arg[i].tag) {
     case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
      free(arg[i].u.caval.carr);
      break;
     case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
      free(arg[i].u.daval.darr);
      break;
     case NACL_SRPC_ARG_TYPE_INT_ARRAY:
      free(arg[i].u.iaval.iarr);
      break;
     case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
       FreeArrayArgs(arg[i].u.vaval.varr, arg[i].u.vaval.count);
       break;
     case NACL_SRPC_ARG_TYPE_INVALID:
     case NACL_SRPC_ARG_TYPE_BOOL:
     case NACL_SRPC_ARG_TYPE_DOUBLE:
     case NACL_SRPC_ARG_TYPE_HANDLE:
     case NACL_SRPC_ARG_TYPE_INT:
     case NACL_SRPC_ARG_TYPE_STRING:
     case NACL_SRPC_ARG_TYPE_OBJECT:
     default:
      break;
    }
  }
}

static void PrintHelp() {
  printf("Commands:\n");
  printf("  # <anything>\n");
  printf("    comment\n");
  printf("  descs\n");
  printf("    print the table of known descriptors (handles)\n");
  printf("  rpc method_name <in_args> * <out_args>\n");
  printf("    -- invoke method_name\n");
  printf("  service\n");
  printf("    print the methods found by service_discovery\n");
  printf("  quit\n");
  printf("    quit the program\n");
  printf("  help\n");
  printf("    print this menu\n");
  printf("  ?\n");
  printf("    print this menu\n");
  /* TODO(sehr,robertm): we should have a syntax description option */
}

static void CommandLoop(NaClSrpcChannel* channel) {
  NaClSrpcError    errcode;
  int              command_count = 0;

  /*
   * Process rpcs.
   */
  for (;;) {
    char        buffer[4096];
    TOKEN       tokens[NACL_SRPC_MAX_ARGS];
    int         n;
    const char  *command;

    fprintf(stderr, "%d> ", command_count);
    ++command_count;

    if (!fgets(buffer, sizeof(buffer), stdin))
      break;

    n = Tokenize(buffer, tokens, NACL_SRPC_MAX_ARGS);

    if (n < 1) {
      if (n < 0)
        fprintf(stderr, "bad line\n");
      continue;
    }

    command =  tokens[0].start;
    if ('#' == command[0]) {
      continue;
    } else if (0 == strcmp("help", command) ||
               0 == strcmp("?", command)) {
      PrintHelp();
    } else if (0 == strcmp("service", command)) {
      NaClSrpcDumpInterfaceDesc(channel);
    } else if (0 == strcmp("descs", command)) {
      PrintDescList();
    } else if (0 == strcmp("quit", command)) {
      break;
    } else if (0 == strcmp("rpc", command)) {
      int          int_out_sep;
      int          n_in;
      NaClSrpcArg  in[NACL_SRPC_MAX_ARGS];
      NaClSrpcArg* inv[NACL_SRPC_MAX_ARGS + 1];
      int          n_out;
      NaClSrpcArg  out[NACL_SRPC_MAX_ARGS];
      NaClSrpcArg* outv[NACL_SRPC_MAX_ARGS + 1];
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

      /*
       * Build the input parameter values.
       */

      n_in = int_out_sep - 2;
      DEBUG("parsing in args %d\n", n_in);
      BuildArgVec(inv, in, n_in);

      if (ParseArgs(in, &tokens[2], n_in) < 0) {
        fprintf(stderr, "bad input args for rpc\n");
        continue;
      }

      /*
       * Build the output (rpc return) values.
       */

      n_out =  n - int_out_sep - 1;
      DEBUG("parsing out args %d\n", n_out);
      BuildArgVec(outv, out, n_out);

      if (ParseArgs(out, &tokens[int_out_sep + 1], n_out) < 0) {
        fprintf(stderr, "bad output args for rpc\n");
        continue;
      }

      rpc_num = NaClSrpcGetRpcNum(channel, tokens[1].start);
      if (rpc_num < 0) {
        fprintf(stderr, "unknown rpc\n");
        continue;
      }

      fprintf(stderr,"using rpc %s no %d\n", tokens[1].start, rpc_num);
      errcode = NaClSrpcInvokeV(channel, rpc_num, inv, outv);
      if (NACL_SRPC_RESULT_OK != errcode) {
        fprintf(stderr, "rpc call failed %s\n", NaClSrpcErrorString(errcode));
        continue;
      }

      /*
       * dump result vector
       */
      printf("%s RESULTS: ", tokens[1].start);
      DumpArgs(outv[0], n_out);

      /*
       * Free the storage allocated for array valued parameters and returns.
       */
      FreeArrayArgs(in, n_in);
      FreeArrayArgs(out, n_out);
    } else {
        fprintf(stderr, "unknown command\n");
        continue;
    }
  }
}


/*
 * This function works with the rpc services in tests/srpc to test the
 * interfaces.
 */
static void TestRandomRpcs(NaClSrpcChannel* channel) {
  NaClSrpcArg        in;
  NaClSrpcArg*       inv[2];
  NaClSrpcArg        out;
  NaClSrpcArg*       outv[2];
  NaClSrpcError      errcode;
  int                argument_count;
  int                return_count;
  int                i;

  inv[0] = &in;
  inv[1] = NULL;

  outv[0] = &out;
  outv[1] = NULL;

  /*
   * TODO(sehr): set up timing on both ends of the IMC channel.
   */

  do {
    if (timed_rpc_method < 1 ||
        timed_rpc_method >= channel->client.rpc_count) {
      fprintf(stderr, "method number must be between 1 and %d (inclusive)\n",
              channel->client.rpc_count - 1);
      break;
    }

    argument_count =
        strlen(channel->client.rpc_descr[timed_rpc_method].in_args);
    return_count =
        strlen(channel->client.rpc_descr[timed_rpc_method].out_args);

    if (argument_count != return_count) {
      fprintf(stderr, "method argument and return count must match\n");
      break;
    }

    if (1 < argument_count) {
      fprintf(stderr, "method must take zero or one argument\n");
      break;
    }

    if (argument_count == 0) {
      inv[0] = NULL;
      outv[0] = NULL;
    } else {
      enum NaClSrpcArgType  type;
      static char           c_in[MAX_ARRAY_SIZE];
      static char           c_out[MAX_ARRAY_SIZE];
      static double         d_in[MAX_ARRAY_SIZE / sizeof(double)];
      static double         d_out[MAX_ARRAY_SIZE / sizeof(double)];
      static int            i_in[MAX_ARRAY_SIZE / sizeof(int)];
      static int            i_out[MAX_ARRAY_SIZE / sizeof(int)];
      char*                 macbeth =
                              "She should have died hereafter;"
                              "There would have been a time for such a word."
                              "To-morrow, and to-morrow, and to-morrow,"
                              "Creeps in this petty pace from day to day"
                              "To the last syllable of recorded time,"
                              "And all our yesterdays have lighted fools"
                              "The way to dusty death.  Out, out, brief candle!"
                              "Life's but a walking shadow, a poor player"
                              "That struts and frets his hour upon the stage"
                              "And then is heard no more: it is a tale"
                              "Told by an idiot, full of sound and fury,"
                              "Signifying nothing";

      in.tag = channel->client.rpc_descr[timed_rpc_method].in_args[0];
      out.tag = channel->client.rpc_descr[timed_rpc_method].out_args[0];

      type = channel->client.rpc_descr[timed_rpc_method].in_args[0];
      switch (type) {
        case NACL_SRPC_ARG_TYPE_BOOL:
          in.u.bval = 1;
          break;
        case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
          in.u.caval.count = timed_rpc_bytes;
          in.u.caval.carr = c_in;
          out.u.caval.count = in.u.iaval.count;
          out.u.caval.carr = c_out;
          break;
        case NACL_SRPC_ARG_TYPE_DOUBLE:
          in.u.dval = 3.1415926;
          break;
        case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
          in.u.daval.count = timed_rpc_bytes / sizeof(double);
          in.u.daval.darr = d_in;
          out.u.daval.count = in.u.iaval.count;
          out.u.daval.darr = d_out;
          break;
        case NACL_SRPC_ARG_TYPE_INT:
          in.u.ival = -1;
          break;
        case NACL_SRPC_ARG_TYPE_INT_ARRAY:
          in.u.iaval.count = timed_rpc_bytes / sizeof(int);
          in.u.iaval.iarr = i_in;
          out.u.iaval.count = in.u.iaval.count;
          out.u.iaval.iarr = i_out;
          break;
        case NACL_SRPC_ARG_TYPE_STRING:
          /* TODO(sehr): needs length variation */
          in.u.sval = macbeth;
          break;
        case NACL_SRPC_ARG_TYPE_INVALID:
        case NACL_SRPC_ARG_TYPE_HANDLE:
        case NACL_SRPC_ARG_TYPE_OBJECT:
        case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
          continue;
      }
    }

    /*
     * Do the rpcs.
     */
    for (i = 0; i < timed_rpc_count; ++i) {
      errcode = NaClSrpcInvokeV(channel, timed_rpc_method, inv, outv);
      if (NACL_SRPC_RESULT_OK != errcode) {
        fprintf(stderr, "rpc call failed %s\n", NaClSrpcErrorString(errcode));
        break;
      }
    }

    if (i == timed_rpc_count) {
      NaClSrpcArg* timer_inv[] = { NULL };
      NaClSrpcArg  tm[4];
      NaClSrpcArg* timer_outv[5];
      double       total_server_usec;
      double       dummy_receive;
      double       dummy_imc_read;
      double       dummy_imc_write;

      timer_outv[0] =  &tm[0];
      timer_outv[1] =  &tm[1];
      timer_outv[2] =  &tm[2];
      timer_outv[3] =  &tm[3];
      timer_outv[4] =  NULL;

      NaClSrpcGetTimes(channel,
                       &total_server_usec,
                       &dummy_receive,
                       &dummy_imc_read,
                       &dummy_imc_write);
      printf("server time observed by client: %.6f sec\n",
             total_server_usec / 1000000.0);
      tm[0].tag = NACL_SRPC_ARG_TYPE_DOUBLE;
      tm[1].tag = NACL_SRPC_ARG_TYPE_DOUBLE;
      tm[2].tag = NACL_SRPC_ARG_TYPE_DOUBLE;
      tm[3].tag = NACL_SRPC_ARG_TYPE_DOUBLE;
      errcode = NaClSrpcInvokeV(channel,
                                NACL_SRPC_GET_TIMES_METHOD,
                                timer_inv,
                                timer_outv);
      printf("server send time:    %.6f sec\n", tm[0].u.dval / 1000000.0);
      printf("server receive time: %.6f sec\n", tm[1].u.dval / 1000000.0);
      printf("imc read time:       %.6f sec\n", tm[2].u.dval / 1000000.0);
      printf("imc write time:      %.6f sec\n", tm[3].u.dval / 1000000.0);
      printf("PASS\n");
    }
  } while (0);
}


#if defined(HAVE_SDL)
#include <SDL.h>
#endif

int main(int  argc, char *argv[]) {
  struct NaClSelLdrLauncher* launcher;
  int                        opt;
  static char*               application_name = NULL;
  char*                      nextp;
  int                        i;
  int                        sel_ldr_argc;
  char**                     tmp_ldr_argv;
  const char**               sel_ldr_argv;
  int                        module_argc;
  const char**               module_argv;
  struct NaClSrpcChannel     command_channel;
  struct NaClSrpcChannel     untrusted_command_channel;
  struct NaClSrpcChannel     channel;
  static const char*         kFixedArgs[] = { "-P", "5", "-X", "5" };
  static const int           kFixedArgCount = sizeof(kFixedArgs) /
                                              sizeof(kFixedArgs[1]);

  /* Descriptor transfer requires the following. */
  NaClNrdAllModulesInit();

  /* We are only testing if this count is not zero. */
  timed_rpc_count = 0;

  /* command line parsing */
  while ((opt = getopt(argc, argv, "f:r:v")) != -1) {
    switch (opt) {
      case 'f':
        application_name = optarg;
        break;
      case 'r':
        timed_rpc_count = strtol(optarg, &nextp, 0);
        if (':' == *nextp) {
          timed_rpc_method = strtol(nextp + 1, &nextp, 0);
          if (':' == *nextp) {
            timed_rpc_bytes = strtol(nextp + 1, 0, 0);
          }
        }
        printf("Testing: %d iterations, method %d, %d bytes\n",
               timed_rpc_count,
               timed_rpc_method,
               timed_rpc_bytes);
        break;
      case 'v':
        NaClLogIncrVerbosity();
        break;
      default:
        fprintf(stderr,
                "Usage: sel_universal -f nacl_file\n"
                "                     [-r count:method:bytes]\n");
        return -1;
    }
  }

  if (NULL == application_name) {
    fprintf(stderr, "-f nacl_file must be specified\n");
    return 1;
  }


  /*
   * Collect any extra arguments on to sel_ldr or the application.
   * sel_ldr_argv contains the options to be passed to sel_ldr.
   * module_argv contains the options to be passed to the application.
   */
  tmp_ldr_argv = NULL;
  sel_ldr_argc = 0;
  module_argv = NULL;
  module_argc = 0;
  for (i = 0; i < argc; ++i) {
    if (tmp_ldr_argv == NULL) {
      /* sel_universal arguments come first. */
      if (!strcmp("--", argv[i])) {
        tmp_ldr_argv = &argv[i + 1];
      }
    } else if (module_argv == NULL) {
      /* sel_ldr arguments come next. */
      if (!strcmp("--", argv[i])) {
        module_argv = (const char**) &argv[i + 1];
      } else {
        ++sel_ldr_argc;
      }
    } else {
      /* application arguments come last. */
      ++module_argc;
    }
  }

  /*
   * Prepend the fixed arguments to the command line.
   */
  sel_ldr_argv =
      (const char**) malloc((sel_ldr_argc + kFixedArgCount) *
                            sizeof(*sel_ldr_argv));
  for (i = 0; i < kFixedArgCount; ++i) {
    sel_ldr_argv[i] = kFixedArgs[i];
  }
  for (i = 0; i < sel_ldr_argc; ++i) {
    sel_ldr_argv[i + kFixedArgCount] = tmp_ldr_argv[i];
  }

  /*
   * Start sel_ldr with the given application and arguments.
   */
  launcher = NaClSelLdrStart(application_name,
                             5,
                             sel_ldr_argc + kFixedArgCount,
                             (const char**) sel_ldr_argv,
                             module_argc,
                             (const char**) module_argv);

  /*
   * Open the communication channels to the service runtime.
   */
  if (!NaClSelLdrOpenSrpcChannels(launcher,
                                  &command_channel,
                                  &untrusted_command_channel,
                                  &channel)) {
    return 1;
  }
  BuildDefaultDescList();
  AddDescToList(NaClSelLdrGetSockAddr(launcher), "module socket address");

  if (timed_rpc_count == 0) {
    CommandLoop(&channel);
  } else {
    TestRandomRpcs(&channel);
  }

  /*
   * Close the connections to sel_ldr.
   */
  NaClSrpcDtor(&command_channel);
  NaClSrpcDtor(&untrusted_command_channel);
  NaClSrpcDtor(&channel);

  /*
   * And shut it down.
   */
  NaClSelLdrShutdown(launcher);

  NaClNrdAllModulesFini();
  return 0;
}

#if NACL_WINDOWS
/* TODO(sehr): create one utility function to get usec times. */
static int gettimeofday(struct timeval *tv, struct timezone *tz) {
  unsigned __int64 timer = 0;
  FILETIME filetime;
  GetSystemTimeAsFileTime(&filetime);
  timer |= filetime.dwHighDateTime;
  timer <<= 32;
  timer |= filetime.dwLowDateTime;
  /* FILETIME has 100ns precision.  Convert to usec. */
  timer /= 10;
  tv->tv_sec = (long) (timer / 1000000UL);
  tv->tv_usec = (long) (timer % 1000000UL);
  return 0;
}
#endif
