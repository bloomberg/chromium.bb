/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/untrusted/crash_dump/untrusted_crash_dump.h"

#include <assert.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#ifdef __GLIBC__
#include <elf.h>
#include <link.h>
#endif  /* __GLIBC__ */

#include "native_client/src/include/portability.h"
#include "native_client/src/include/nacl/nacl_exception.h"


#define CRASH_PAGE_CHUNK (64 * 1024)
#define CRASH_STACK_SIZE (CRASH_PAGE_CHUNK * 4)
#define CRASH_STACK_GUARD_SIZE CRASH_PAGE_CHUNK
#define CRASH_STACK_COMPLETE_SIZE (CRASH_STACK_GUARD_SIZE + CRASH_STACK_SIZE)


static pthread_key_t g_CrashStackKey;
static int g_ExceptionHandlingEnabled = 0;


#ifdef __GLIBC__

struct ProgramTableData {
  FILE *core;
  int first;
};

static void WriteJsonString(const char *str, FILE *file) {
  char ch;

  fputc('"', file);
  for (;;) {
    ch = *str++;
    if (ch == '\0') {
      break;
    } else if (ch == '"') {
      fprintf(file, "\\\"");
    } else if (ch == '\\') {
      fprintf(file, "\\\\");
    } else if (ch < 32 || ch > 126) {
      fprintf(file, "\\x%02x", (uint8_t)ch);
    } else {
      fputc(ch, file);
    }
  }
  fputc('"', file);
}

static int PrintSegmentsOne(
    struct dl_phdr_info *info, size_t size, void *data) {
  int i;
  int print_comma = 0;
  struct ProgramTableData *ptd = (struct ProgramTableData*) data;

  if (ptd->first) {
    ptd->first = 0;
  } else {
    fprintf(ptd->core, ",\n");
  }
  fprintf(ptd->core, "{\n");
  fprintf(ptd->core, "\"dlpi_name\": ");
  WriteJsonString(info->dlpi_name, ptd->core);
  fprintf(ptd->core, ",\n");
  fprintf(ptd->core, "\"dlpi_addr\": %"NACL_PRIuPTR",\n",
          (uintptr_t)  info->dlpi_addr);
  fprintf(ptd->core, "\"dlpi_phdr\": [\n");
  for (i = 0; i < info->dlpi_phnum; i++) {
    /* Skip non-LOAD type segments. */
    if (info->dlpi_phdr[i].p_type != PT_LOAD) {
      continue;
    }
    if (print_comma) {
      fprintf(ptd->core, ",\n");
    } else {
      print_comma = 1;
    }
    fprintf(ptd->core, "{\n");
    fprintf(ptd->core, "\"p_vaddr\": %"NACL_PRIuPTR",\n",
            (uintptr_t) info->dlpi_phdr[i].p_vaddr);
    fprintf(ptd->core, "\"p_memsz\": %"NACL_PRIuPTR"\n",
            (uintptr_t) info->dlpi_phdr[i].p_memsz);
    fprintf(ptd->core, "}\n");
  }
  fprintf(ptd->core, "]\n");
  fprintf(ptd->core, "}\n");
  return 0;
}

static void PrintSegments(FILE *core) {
  struct ProgramTableData data;
  data.core = core;
  data.first = 1;
  dl_iterate_phdr(PrintSegmentsOne, &data);
}

#else  /* __GLIBC__ */

static void PrintSegments(FILE *core) {
}

#endif  /* __GLIBC__ */

static uintptr_t SafeRead(uintptr_t a) {
  /* TODO(bradnelson): use exception handling to recover from reads. */
  return *(uintptr_t*)a;
}

static uintptr_t FrameLocPC(uintptr_t fp) {
#if defined(__arm__)
  return fp;
#elif defined(__x86_64__)
  return fp + 8;
#else
  return fp + 4;
#endif
}

static uintptr_t FrameLocNext(uintptr_t fp) {
#if defined(__arm__)
  return fp - 4;
#else
  return fp;
#endif
}

static uintptr_t FrameLocArgs(uintptr_t fp) {
#if defined(__arm__)
  return fp + 4;
#elif defined(__x86_64__)
  return fp + 8;
#else
  return fp + 16;
#endif
}

static void StackWalk(FILE *core,
                      struct NaClExceptionPortableContext *pcontext) {
  uintptr_t next;
  uintptr_t i;
  int first = 1;
  uintptr_t prog_ctr = pcontext->prog_ctr;
  uintptr_t frame_ptr = pcontext->frame_ptr;
  uintptr_t args_start;

  fprintf(core, "\"frames\": [\n");
  for (;;) {
    next = SafeRead(FrameLocNext(frame_ptr));
    if (next <= frame_ptr || next == 0) {
      break;
    }
    if (first) {
      first = 0;
    } else {
      fprintf(core, ",");
    }
    fprintf(core, "{\n");
    fprintf(core, "\"frame_ptr\": %"NACL_PRIuPTR",\n", frame_ptr);
    fprintf(core, "\"prog_ctr\": %"NACL_PRIuPTR",\n", prog_ctr);
    fprintf(core, "\"data\": [\n");
    args_start = FrameLocArgs(frame_ptr);
    for (i = args_start; i < next; i += 4) {
      if (i != args_start) {
        fprintf(core, ",");
      }
      fprintf(core, "%"NACL_PRIuPTR"\n", SafeRead(i));
    }
    fprintf(core, "]\n");
    fprintf(core, "}\n");

    prog_ctr = SafeRead(FrameLocPC(frame_ptr));
    frame_ptr = next;
  }

  fprintf(core, "]\n");
}

void CrashHandler(struct NaClExceptionContext *context) {
  FILE *core;
  const char *core_filename;

  /* Pick core file name. */
  core_filename = getenv("NACLCOREFILE");
  if (core_filename == NULL) {
    core_filename = "naclcore.json";
  }

  /* Attempt to open core file, otherwise use stdout. */
  core = fopen(core_filename, "w");
  if (core == NULL) {
    core = stdout;
  }

  fprintf(core, "{\n");

  fprintf(core, "\"segments\": [");
  PrintSegments(core);
  fprintf(core, "],\n");

  struct NaClExceptionPortableContext *pcontext =
      nacl_exception_context_get_portable(context);
  fprintf(core, "\"handler\": {\n");
  fprintf(core, "\"prog_ctr\": %"NACL_PRIuPTR",\n", pcontext->prog_ctr);
  fprintf(core, "\"stack_ptr\": %"NACL_PRIuPTR",\n", pcontext->stack_ptr);
  fprintf(core, "\"frame_ptr\": %"NACL_PRIuPTR"\n", pcontext->frame_ptr);
  fprintf(core, "},\n");

  StackWalk(core, pcontext);

  fprintf(core, "}\n");

  if (core != stdout) {
    fclose(core);
  }

  exit(166);
}

void NaClCrashDumpThreadDestructor(void *arg) {
  munmap(arg, CRASH_STACK_COMPLETE_SIZE);
}

int NaClCrashDumpInit(void) {
  int result;

  assert(g_ExceptionHandlingEnabled == 0);
  result = pthread_key_create(&g_CrashStackKey, NaClCrashDumpThreadDestructor);
  assert(result == 0);
  if (nacl_exception_set_handler(CrashHandler) != 0) {
    return 0;
  }
  g_ExceptionHandlingEnabled = 1;
  if (!NaClCrashDumpInitThread()) {
    g_ExceptionHandlingEnabled = 0;
    return 0;
  }
  return 1;
}

int NaClCrashDumpInitThread(void) {
  void *stack;
  void *guard;
  int result;

  if (!g_ExceptionHandlingEnabled) {
    return 0;
  }
  /*
   * NOTE: Setting up a per thread stack is only particularly interesting
   *       for stack overflow.
   */
  stack = mmap(NULL, CRASH_STACK_COMPLETE_SIZE,
               PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  assert(stack != MAP_FAILED);
  guard = mmap(stack, CRASH_STACK_GUARD_SIZE,
               PROT_NONE, MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  assert(guard == stack);
  pthread_setspecific(g_CrashStackKey, stack);
  result = nacl_exception_set_stack(stack, CRASH_STACK_COMPLETE_SIZE);
  return result == 0;
}
