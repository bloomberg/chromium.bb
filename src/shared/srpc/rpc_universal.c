/*
 * Copyright (c) 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl testing shell
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "native_client/src/include/portability_string.h"
#if NACL_WINDOWS
#include <float.h>
#define NACL_ISNAN(d) _isnan(d)
#else
/* Windows doesn't have the following header files. */
# include <unistd.h>
# include <sys/types.h>
# include <sys/time.h>
# include <math.h>
#define NACL_ISNAN(d) isnan(d)
#endif  /* NACL_WINDOWS */
#include "native_client/src/include/nacl_base.h"
#ifdef __native_client__
# include <fcntl.h>
# include <nacl/nacl_inttypes.h>
#else
# include "native_client/src/shared/platform/nacl_host_desc.h"
# include "native_client/src/trusted/desc/nacl_desc_base.h"
# include "native_client/src/trusted/desc/nacl_desc_invalid.h"
# include "native_client/src/trusted/desc/nacl_desc_io.h"
# include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#endif  /* __native_client__ */
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/shared/srpc/nacl_srpc_internal.h"

#ifndef __native_client__
/* TODO(robertm): for ARM both __native_client__ and NACL_LINUX are true
   this needs to be fixed */
#if NACL_LINUX
# include <sys/ipc.h>
# include <sys/mman.h>
# include <sys/shm.h>
# include <sys/stat.h>
# include "native_client/src/trusted/desc/nacl_desc_base.h"
# include "native_client/src/trusted/desc/nacl_desc_io.h"
# include "native_client/src/trusted/desc/linux/nacl_desc_sysv_shm.h"
# include "native_client/src/trusted/service_runtime/include/sys/mman.h"
#endif  /* NACL_LINUX */
#endif  /* __native_client__ */

#define kMaxCommandLineLength 4096

/* Table for keeping track of descriptors passed to/from sel_universal */
typedef struct DescList DescList;
struct DescList {
  DescList* next;
  int number;
  const char* print_name;
  NaClSrpcImcDescType desc;
};
static DescList* descriptors = NULL;

static int AddDescToList(NaClSrpcImcDescType new_desc, const char* print_name) {
  static int next_desc = -1;
  DescList* new_list_element;
  DescList** list_pointer;

  /* Return an error if passed a null descriptor */
  if (kNaClSrpcInvalidImcDesc == new_desc) {
    return -1;
  }
  /* Find the descriptor in the list (for the known invalid descriptor), or
   * find the end of the list for insertion of a new node. */
  for (list_pointer = &descriptors;
       NULL != *list_pointer && (*list_pointer)->desc != new_desc;
       list_pointer = &(*list_pointer)->next) {
  }
  if (NULL != *list_pointer) {
    /* It was in the list. */
    return (*list_pointer)->number;
  }
  /* Create a new descriptor list node. */
  new_list_element = (DescList*) malloc(sizeof(DescList));
  if (NULL == new_list_element) {
    return -1;
  }
  new_list_element->number = next_desc++;
  new_list_element->desc = new_desc;
  new_list_element->print_name = print_name;
  new_list_element->next = NULL;
  *list_pointer = new_list_element;
  return new_list_element->number;
}

static NaClSrpcImcDescType DescFromPlatformDesc(int fd, int mode) {
#ifdef __native_client__
  return fd;
#else
  return
      (NaClSrpcImcDescType) NaClDescIoDescMake(NaClHostDescPosixMake(fd, mode));
#endif  /* __native_client__ */
}

/* TODO(robertm): for ARM both __native_client__ and NACL_LINUX are true
   this needs to be fixed */
#ifndef __native_client__
#if NACL_LINUX
static void* kSysvShmAddr = (void*) (intptr_t) -1;
static struct NaClDescSysvShm* shm_desc = NULL;

static void RemoveShmId() {
  if (NULL != shm_desc) {
    shmctl(shm_desc->id, IPC_RMID, NULL);
  }
}

static NaClSrpcImcDescType SysvShmDesc() {
  const size_t k64KBytes = 0x10000;
  const size_t kShmSize = k64KBytes;
  const char* kInitString = "Hello SysV shared memory.";
  void* mapaddr = MAP_FAILED;
  uintptr_t aligned;
  void* aligned_mapaddr = MAP_FAILED;

  /* Allocate a descriptor node. */
  shm_desc = malloc(sizeof *shm_desc);
  if (NULL == shm_desc) {
    goto cleanup;
  }
  /* Construct the descriptor with a new shared memory region. */
  if (!NaClDescSysvShmCtor(shm_desc, (nacl_off64_t) kShmSize)) {
    free(shm_desc);
    shm_desc = NULL;
    goto cleanup;
  }
  /* register free of shm id */
  atexit(RemoveShmId);
  /*
   * Find a hole to map the shared memory into.  Since we need a 64K aligned
   * address, we begin by allocating 64K more than we need, then we map to
   * an aligned address within that region.
   */
  mapaddr = mmap(NULL,
                 kShmSize + k64KBytes,
                 PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS,
                 -1,
                 0);
  if (MAP_FAILED == mapaddr) {
    goto cleanup;
  }
  /* Round mapaddr to next 64K. */
  aligned = ((uintptr_t) mapaddr + kShmSize - 1) & kShmSize;
  /* Map the aligned region. */
  aligned_mapaddr = mmap((void*) aligned,
                         kShmSize,
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE |MAP_ANONYMOUS,
                         -1,
                         0);
  if (MAP_FAILED == aligned_mapaddr) {
    goto cleanup;
  }
  /*
   * Attach to the region.  There is no explicit detach, because the Linux
   * man page says one will be done at process exit.
   */
  kSysvShmAddr = (void *) (*((struct NaClDescVtbl *)
                             ((struct NaClRefCount *) shm_desc)->vtbl)->Map)(
      (struct NaClDesc *) shm_desc,
      (struct NaClDescEffector*) NULL,
      aligned_mapaddr,
      kShmSize,
      NACL_ABI_PROT_READ | NACL_ABI_PROT_WRITE,
      NACL_ABI_MAP_SHARED | NACL_ABI_MAP_FIXED,
      0);
  if (aligned_mapaddr != kSysvShmAddr) {
    goto cleanup;
  }
  /* Initialize the region with a string for comparisons. */
  strcpy(kSysvShmAddr, kInitString);
  /* Return successfully created descriptor. */
  return (NaClSrpcImcDescType) shm_desc;

cleanup:
  if (MAP_FAILED != aligned_mapaddr) {
    munmap(aligned_mapaddr, kShmSize);
  }
  if (MAP_FAILED != mapaddr) {
    munmap(mapaddr, kShmSize + k64KBytes);
  }
  NaClDescUnref((struct NaClDesc*) shm_desc);
  return kNaClSrpcInvalidImcDesc;
}
#endif  /* NACL_LINUX */
#endif  /* __native_client__ */

static void BuildDefaultDescList() {
#ifdef __native_client__
  const int kRdOnly = O_RDONLY;
  const int kWrOnly = O_WRONLY;
#else
  const int kRdOnly = NACL_ABI_O_RDONLY;
  const int kWrOnly = NACL_ABI_O_WRONLY;
#endif  /* __native_client__ */
#ifndef __native_client__
  AddDescToList((struct NaClDesc*) NaClDescInvalidMake(), "invalid");
#endif  /* __native_client__ */
  AddDescToList(DescFromPlatformDesc(0, kRdOnly), "stdin");
  AddDescToList(DescFromPlatformDesc(1, kWrOnly), "stdout");
  AddDescToList(DescFromPlatformDesc(2, kWrOnly), "stderr");
}

static void PrintDescList() {
  DescList* list;

  printf("Descriptors:\n");
  for (list = descriptors; NULL != list; list = list->next) {
    printf("  %d: %s\n", list->number, list->print_name);
  }
}

static NaClSrpcImcDescType LookupDesc(int num) {
  DescList* list;

  for (list = descriptors; NULL != list; list = list->next) {
    if (num == list->number) {
      return list->desc;
    }
  }
  return kNaClSrpcInvalidImcDesc;
}

/*  simple destructive tokenizer */
typedef struct {
  const char*     start;
  nacl_abi_size_t length;
} TOKEN;

static int HandleEscapedChar(const char** p) {
  int ival;
  int count;

  switch (**p) {
   case '\\':
    ++*p;
    return '\\';
   case '\"':
    ++*p;
    return '\"';
   case 'b':
    ++*p;
    return '\b';
   case 'f':
    ++*p;
    return '\f';
   case 'n':
    ++*p;
    return '\n';
   case 't':
    ++*p;
    return '\t';
   case 'v':
    ++*p;
    return '\v';

   /* Octal sequences. */
   case '0':
   case '1':
   case '2':
   case '3':
   case '4':
   case '5':
   case '6':
   case '7':
    ival = 0;
    count = 0;
    while ('0' <= **p && '7' >= **p && 3 > count) {
      ival = ival * 8 + **p - '0';
      ++*p;
      ++count;
    }
    return ival;

   /* Hexadecimal sequences. */
   case 'x':
   case 'X':
    ival = 0;
    count = 0;
    ++*p;
    while (isxdigit((unsigned char)**p) && 2 > count) {
      if (isdigit((unsigned char)**p)) {
        ival = ival * 16 + **p - '0';
      } else {
        ival = ival * 16 + toupper((unsigned char)**p) - 'A' + 10;
      }
      ++*p;
      ++count;
    }
    return ival;

   default:
    /* Unrecognized token. Return the '\\' and let the caller come back */
    return '\\';
  }
  return -1;
}

/*
 * Reads one char from *p and advances *p to the next read point in the input.
 * This handles some meta characters ('\\', '\"', '\b', '\f', '\n', '\t',
 * and '\v'), \ddd, where ddd is interpreted as an octal character value, and
 * \[xX]dd, where dd is interpreted as a hexadecimal character value.
 */
static int ReadOneChar(const char** p) {
  int ival;

  switch (**p) {
   case '\0':
    /* End of string is returned as -1 so that we can embed '\0' in strings. */
    return -1;
   case '\\':
    ++*p;
    return HandleEscapedChar(p);
   default:
    ival = **p;
    ++*p;
    return ival;
  }
  return -1;
}

/* expects *from to point to leading \" and returns pointer to trailing \" */
static const char* ScanEscapeString(char* to, const char* from) {
  int ival;
  from++;
  ival = ReadOneChar(&from);
  while (-1 != ival) {
    if ('\"' == ival) {
      if (NULL != to) {
        *to = '\0';
      }
      return from;
    }
    if (NULL != to) {
      *to++ = ival;
    }
    ival = ReadOneChar(&from);
  }
  if (NULL != to) {
    *to = ival;
  }
  return 0;
}

static int Tokenize(char* line, TOKEN *array, int n) {
  ssize_t pos_start = 0;
  int count = 0;

  for( ; count < n; count++ ) {
    ssize_t pos_end;

    /* skip leading white space */
    while (line[pos_start]) {
      const char c = line[pos_start];
      if (isspace((unsigned char)c)) {
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

      if (isspace((unsigned char)c)) {
        break;
      } else if (c == '\"') {
        /* TBD(sehr): quotes are only really relevant in s("..."). */
        const char* end = ScanEscapeString(0, &line[pos_end]);
        if (!end) return -1;
        pos_end = end - &line[0];
      }
      pos_end++;
    }

    /* save the token */
    array[count].start = &line[pos_start];
    array[count].length = nacl_abi_size_t_saturate(pos_end - pos_start);

    if (line[pos_end]) {
      line[pos_end] = '\0';   /* DESTRUCTION!!! */
      pos_end++;
    }
    pos_start = pos_end;
  }

  return count;
}

/*
 * Create an argument from a token string.  Returns 1 if successful or
 * 0 if not (out of memory or bad type).
 */
static int ParseArg(NaClSrpcArg* arg, const char* token) {
  int32_t val;
  int64_t lval;
  int dim;
  int i;
  const char* comma;

  /* Initialize the argument slot.  This enables freeing on failures. */
  memset(arg, 0, sizeof(*arg));

  dprintf((SIDE "SRPC: TOKEN %s\n", token));
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
    if (NULL == arg->u.caval.carr) {
      return 0;
    }
    arg->u.caval.count = dim;
    comma = strstr(token, ",");
    if (comma) {
      const char* p = comma + 1;
      for (i = 0; *p != ')' && i < dim; ++i) {
        int ival = ReadOneChar(&p);
        if (-1 == ival || ')' == ival) {
          break;
        }
        arg->u.caval.carr[i] = ival;
      }
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
    if (NULL == arg->u.daval.darr) {
      return 0;
    }
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
    arg->u.iaval.iarr = (int32_t*) calloc(dim, sizeof(int32_t));
    if (NULL == arg->u.iaval.iarr) {
      return 0;
    }
    arg->u.iaval.count = dim;
    comma = token;
    for (i = 0; i < dim; ++i) {
      comma = strstr(comma, ",");
      if (!comma) break;
      ++comma;
      arg->u.iaval.iarr[i] = strtol(comma, 0, 0);
    }
    break;
   case NACL_SRPC_ARG_TYPE_LONG:
    lval = STRTOLL(&token[2], 0, 0);
    arg->tag = NACL_SRPC_ARG_TYPE_LONG;
    arg->u.lval = lval;
    break;
   case NACL_SRPC_ARG_TYPE_LONG_ARRAY:
    /*
     * dim is the count of the elements in the array, which is a 32-bit value.
     */
    dim = strtol(&token[2], 0, 0);
    arg->tag = NACL_SRPC_ARG_TYPE_LONG_ARRAY;
    arg->u.laval.larr = (int64_t*) calloc(dim, sizeof(int64_t));
    if (NULL == arg->u.laval.larr) {
      return 0;
    }
    arg->u.laval.count = dim;
    comma = token;
    for (i = 0; i < dim; ++i) {
      comma = strstr(comma, ",");
      if (!comma) break;
      ++comma;
      arg->u.laval.larr[i] = STRTOLL(comma, 0, 0);
    }
    break;
   case NACL_SRPC_ARG_TYPE_STRING:
    arg->tag = NACL_SRPC_ARG_TYPE_STRING;
    /*
     * This is a conservative estimate of the length, as it includes the
     * quotes and possibly some escape characters.
     */
    arg->u.sval = malloc(strlen(token));
    if (NULL == arg->u.sval) {
      return 0;
    }
    ScanEscapeString(arg->u.sval, token + 2);
    break;
    /*
     * The two cases below are added to avoid warnings, they are only used
     * in the plugin code
     */
   case NACL_SRPC_ARG_TYPE_OBJECT:
   case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
   default:
    return 0;
  }

  return 1;
}

/*
 * Read n arguments from the tokens array.  Returns n if successful or
 * -1 if not.
 */
static int ParseArgs(NaClSrpcArg* arg, const TOKEN* token, int n) {
  int i;

  for (i = 0; i < n; ++i) {
    if (!ParseArg(&arg[i], token[i].start)) {
      /* TODO(sehr): reclaim memory here on failure. */
      return -1;
    }
  }
  return n;
}

static void PrintOneChar(char c) {
  switch (c) {
   case '\"':
    printf("\\\"");
    break;
   case '\b':
    printf("\\b");
    break;
   case '\f':
    printf("\\f");
    break;
   case '\n':
    printf("\\n");
    break;
   case '\t':
    printf("\\t");
    break;
   case '\v':
    printf("\\v");
    break;
   case '\\':
    printf("\\\\");
    break;
   default:
    if (' ' > c || 127 == c) {
      printf("\\x%02x", c);
    } else {
      printf("%c", c);
    }
  }
}

static void DumpDouble(const double* dval) {
  if (NACL_ISNAN(*dval)) {
    printf("NaN");
  } else {
    printf("%f", *dval);
  }
}

static void DumpArg(const NaClSrpcArg* arg) {
  uint32_t count;
  uint32_t i;
  char* p;

  switch(arg->tag) {
   case NACL_SRPC_ARG_TYPE_INVALID:
    printf("X()");
    break;
   case NACL_SRPC_ARG_TYPE_BOOL:
    printf("b(%d)", arg->u.bval);
    break;
   case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
    for (i = 0; i < arg->u.caval.count; ++i)
      PrintOneChar(arg->u.caval.carr[i]);
    break;
   case NACL_SRPC_ARG_TYPE_DOUBLE:
    printf("d(");
    DumpDouble(&arg->u.dval);
    printf(")");
    break;
   case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
    count = arg->u.daval.count;
    printf("D(%"NACL_PRIu32"", count);
    for (i = 0; i < count; ++i) {
      printf(",");
      DumpDouble(&(arg->u.daval.darr[i]));
    }
    printf(")");
    break;
   case NACL_SRPC_ARG_TYPE_HANDLE:
    printf("h(%d)", AddDescToList(arg->u.hval, "imported"));
    break;
   case NACL_SRPC_ARG_TYPE_INT:
    printf("i(%"NACL_PRId32")", arg->u.ival);
    break;
   case NACL_SRPC_ARG_TYPE_INT_ARRAY:
    count = arg->u.iaval.count;
    printf("I(%"NACL_PRIu32"", count);
    for (i = 0; i < count; ++i)
      printf(",%"NACL_PRId32, arg->u.iaval.iarr[i]);
    printf(")");
    break;
   case NACL_SRPC_ARG_TYPE_LONG:
    printf("l(%"NACL_PRId64")", arg->u.lval);
    break;
   case NACL_SRPC_ARG_TYPE_LONG_ARRAY:
    count = arg->u.laval.count;
    printf("L(%"NACL_PRIu32"", count);
    for (i = 0; i < count; ++i)
      printf(",%"NACL_PRId64, arg->u.laval.larr[i]);
    printf(")");
    break;
   case NACL_SRPC_ARG_TYPE_STRING:
    printf("s(\"");
    for (p = arg->u.sval; '\0' != *p; ++p)
      PrintOneChar(*p);
    printf("\")");
    break;
    /*
     * The two cases below are added to avoid warnings, they are only used
     * in the plugin code
     */
   case NACL_SRPC_ARG_TYPE_OBJECT:
    /* this is a pointer that NaCl module can do nothing with */
    printf("o(%p)", arg->u.oval);
    break;
   case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
    count = arg->u.vaval.count;
    printf("A(%"NACL_PRIu32"", count);
    for (i = 0; i < count; ++i) {
      printf(",");
      DumpArg(&arg->u.vaval.varr[i]);
    }
    printf(")");
    break;
   default:
    break;
  }
}

static void DumpArgs(const NaClSrpcArg* arg, int n) {
  int i;
  for (i=0; i<n; ++i) {
    printf("  ");
    DumpArg(&arg[i]);
  }
  printf("\n");
}

void BuildArgVec(NaClSrpcArg* argv[], NaClSrpcArg arg[], int count) {
  int i;
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
     case NACL_SRPC_ARG_TYPE_LONG_ARRAY:
      free(arg[i].u.laval.larr);
      break;
     case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
      FreeArrayArgs(arg[i].u.vaval.varr, arg[i].u.vaval.count);
      break;
     case NACL_SRPC_ARG_TYPE_INVALID:
     case NACL_SRPC_ARG_TYPE_BOOL:
     case NACL_SRPC_ARG_TYPE_DOUBLE:
     case NACL_SRPC_ARG_TYPE_HANDLE:
     case NACL_SRPC_ARG_TYPE_INT:
     case NACL_SRPC_ARG_TYPE_LONG:
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
  printf("  sysv\n");
  printf("    create a descriptor for an SysV shared memory (Linux only)\n");
  printf("  rpc method_name <in_args> * <out_args>\n");
  printf("    Invoke method_name.\n");
  printf("    Each in_arg is of form 'type(value)', e.g. i(42), s(\"foo\").\n");
  printf("    Each out_arg is of form 'type', e.g. i, s.\n");
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

static void UpcallString(NaClSrpcRpc* rpc,
                         NaClSrpcArg** ins,
                         NaClSrpcArg** outs,
                         NaClSrpcClosure* done) {
  UNREFERENCED_PARAMETER(outs);
  printf("UpcallString: called with '%s'\n", ins[0]->u.sval);
  rpc->result = NACL_SRPC_RESULT_OK;
  done->Run(done);
}

static char* BuildSignature(const char* name,
                            NaClSrpcArg** inputs,
                            NaClSrpcArg** outputs) {
  char* buffer;
  size_t i;
  size_t current_offset;

  buffer = (char*) malloc(kMaxCommandLineLength +
                          2 * (NACL_SRPC_MAX_ARGS + 1) + 1);
  if (NULL == buffer) {
    return NULL;
  }
  strncpy(buffer, name, kMaxCommandLineLength);
  current_offset = strlen(name);
  buffer[current_offset] = ':';
  ++current_offset;
  for (i = 0; i < NACL_SRPC_MAX_ARGS; ++i) {
    if (NULL == inputs[i]) {
      break;
    }
    buffer[current_offset] = inputs[i]->tag;
    ++current_offset;
  }
  buffer[current_offset] = ':';
  ++current_offset;
  for (i = 0; i < NACL_SRPC_MAX_ARGS; ++i) {
    if (NULL == outputs[i]) {
      break;
    }
    buffer[current_offset] = outputs[i]->tag;
    ++current_offset;
  }
  buffer[current_offset] = 0;
  return buffer;
}

void NaClSrpcCommandLoop(NaClSrpcService* service,
                         NaClSrpcChannel* channel,
                         NaClSrpcInterpreter interpreter,
                         NaClSrpcImcDescType default_socket_address) {
  NaClSrpcError errcode;
  int           command_count = 0;

  /* Add the default descriptors to the table */
  BuildDefaultDescList();
  if (kNaClSrpcInvalidImcDesc != default_socket_address) {
    AddDescToList(default_socket_address, "module socket address");
  }
  /* Add a simple upcall service to the channel (if any) */
  if (NULL != channel) {
    static const NaClSrpcHandlerDesc upcall_handlers[] = {
      { "upcall_string:s:", UpcallString },
      { NULL, NULL }
    };
    NaClSrpcService* service = (NaClSrpcService*) malloc(sizeof(*service));
    if (NULL == service) {
      fprintf(stderr, "Couldn't allocate upcall service\n");
      return;
    }
    if (!NaClSrpcServiceHandlerCtor(service, upcall_handlers)) {
      fprintf(stderr, "Couldn't construct upcall service\n");
      return;
    }
    channel->server = service;
  }
  /* Read RPC requests from stdin and send them. */
  for (;;) {
    char        buffer[kMaxCommandLineLength];
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
      NaClSrpcServicePrint(service);
    } else if (0 == strcmp("descs", command)) {
      PrintDescList();
    } else if (0 == strcmp("sysv", command)) {

#ifndef __native_client__
/* TODO(robertm): for ARM both __native_client__ and NACL_LINUX are true
   this needs to be fixed */
#if NACL_LINUX
      AddDescToList(SysvShmDesc(), "SysV shared memory");
#endif  /* NACL_LINUX */
#endif /* __native_client__ */
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
      uint32_t     rpc_num;
      char*        signature;

      if (n < 2) {
        fprintf(stderr, "Insufficient arguments to 'rpc' command.\n");
        continue;
      }

      for (int_out_sep = 2; int_out_sep < n; ++int_out_sep) {
        if (0 == strcmp(tokens[int_out_sep].start, "*"))
          break;
      }

      if (int_out_sep == n) {
        fprintf(stderr,
                "No input/output argument separator for 'rpc' command.\n");
        continue;
      }

      /* Build the input parameter values. */
      n_in = int_out_sep - 2;
      dprintf((SIDE "SRPC: Parsing %d input args.\n", n_in));
      BuildArgVec(inv, in, n_in);

      if (ParseArgs(in, &tokens[2], n_in) < 0) {
        fprintf(stderr, "Bad input args for RPC.\n");
        continue;
      }

      /* Build the output (rpc return) values. */
      n_out =  n - int_out_sep - 1;
      dprintf((SIDE "SRPC: Parsing %d output args.\n", n_out));
      BuildArgVec(outv, out, n_out);

      if (ParseArgs(out, &tokens[int_out_sep + 1], n_out) < 0) {
        fprintf(stderr, "Bad output args for RPC.\n");
        continue;
      }

      signature = BuildSignature(tokens[1].start, inv, outv);
      if (NULL == signature) {
        fprintf(stderr, "Failed to build RPC method signature (OOM?).\n");
        continue;
      }
      rpc_num = NaClSrpcServiceMethodIndex(service, signature);
      free(signature);
      if (kNaClSrpcInvalidMethodIndex == rpc_num) {
        fprintf(stderr, "No RPC found of that name/signature.\n");
        continue;
      }

      fprintf(stderr, "Calling RPC %s (#%"NACL_PRIu32")...\n",
              tokens[1].start, rpc_num);
      errcode = (*interpreter)(service, channel, rpc_num, inv, outv);
      if (NACL_SRPC_RESULT_OK != errcode) {
        fprintf(stderr, "RPC call failed: %s.\n", NaClSrpcErrorString(errcode));
        continue;
      }

      /* dump result vector */
      printf("%s RESULTS: ", tokens[1].start);
      DumpArgs(outv[0], n_out);

      /* Free the storage allocated for array valued parameters and returns. */
      FreeArrayArgs(in, n_in);
      FreeArrayArgs(out, n_out);
    } else {
      fprintf(stderr, "Unknown command `%s'.\n", command);
      continue;
    }
  }
}

typedef struct {
  struct NaClSrpcClosure base;
  int run_was_invoked;
} RpcCheckingClosure;

static void RpcCheckingClosureRun(NaClSrpcClosure* self) {
  RpcCheckingClosure* vself = (RpcCheckingClosure*) self;
  vself->run_was_invoked = 1;
}

static void RpcCheckingClosureCtor(RpcCheckingClosure* self) {
  self->base.Run = RpcCheckingClosureRun;
  self->run_was_invoked = 0;
}

static NaClSrpcError Interpreter(NaClSrpcService *service,
                                 NaClSrpcChannel *channel,
                                 uint32_t rpc_number,
                                 NaClSrpcArg **ins,
                                 NaClSrpcArg **outs) {
  NaClSrpcRpc rpc;
  RpcCheckingClosure done;
  RpcCheckingClosureCtor(&done);
  UNREFERENCED_PARAMETER(channel);
  (*NaClSrpcServiceMethod(service, rpc_number))(&rpc,
                                                ins,
                                                outs,
                                                (NaClSrpcClosure*) &done);
  if (!done.run_was_invoked) {
    fprintf(stderr, "done->Run(done) was never called\n");
  }
  return rpc.result;
}

int NaClSrpcCommandLoopMain(const struct NaClSrpcHandlerDesc *methods) {
  /* Build the service */
  NaClSrpcService *service = (NaClSrpcService *) malloc(sizeof(*service));
  if (NULL == service) {
    return 0;
  }
  if (!NaClSrpcServiceHandlerCtor(service, methods)) {
    free(service);
    return 0;
  }
  /*
   * TODO(sehr): Add the descriptor for the default socket address
   * message processing loop.
   */
  NaClSrpcCommandLoop(service, NULL, Interpreter, kNaClSrpcInvalidImcDesc);
  return 1;
}
