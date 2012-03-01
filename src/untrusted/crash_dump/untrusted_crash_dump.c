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
#include <sys/nacl_syscalls.h>

#ifdef __GLIBC__
#include <elf.h>
#include <link.h>
#endif  /* __GLIBC__ */

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/service_runtime/include/sys/nacl_exception.h"
#include "native_client/src/untrusted/nacl/syscall_bindings_trampoline.h"


#define CRASH_PAGE_CHUNK (64 * 1024)
#define CRASH_STACK_SIZE (CRASH_PAGE_CHUNK * 4)
#define CRASH_STACK_GUARD_SIZE CRASH_PAGE_CHUNK
#define CRASH_STACK_COMPLETE_SIZE (CRASH_STACK_GUARD_SIZE + CRASH_STACK_SIZE)


static pthread_key_t g_CrashStackKey;


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
    if (i != 0) {
      fprintf(ptd->core, ",\n");
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

uintptr_t SafeRead(uintptr_t a) {
  /* TODO(bradnelson): use exception handling to recover from reads. */
  return *(uintptr_t*)a;
}

static void StackWalk(FILE *core, struct NaClExceptionContext *context) {
  uintptr_t next;
  uintptr_t i;
  int first = 1;
  uintptr_t prog_ctr = context->prog_ctr;
  uintptr_t frame_ptr = context->frame_ptr;
  uintptr_t args_start;

  fprintf(core, "\"frames\": [\n");
  for (;;) {
    next = SafeRead(frame_ptr);
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
#if defined(__x86_64__)
    args_start = frame_ptr + 8;
#else
    args_start = frame_ptr + 16;
#endif
    for (i = args_start; i < next; i += 4) {
      if (i != args_start) {
        fprintf(core, ",");
      }
      fprintf(core, "%"NACL_PRIuPTR"\n", SafeRead(i));
    }
    fprintf(core, "]\n");
    fprintf(core, "}\n");

#if defined(__x86_64__)
    prog_ctr = SafeRead(frame_ptr + 8);
#else
    prog_ctr = SafeRead(frame_ptr + 4);
#endif
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

  fprintf(core, "\"handler\": {\n");
  fprintf(core, "\"prog_ctr\": %"NACL_PRIuPTR",\n", context->prog_ctr);
  fprintf(core, "\"stack_ptr\": %"NACL_PRIuPTR",\n", context->stack_ptr);
  fprintf(core, "\"frame_ptr\": %"NACL_PRIuPTR"\n", context->frame_ptr);
  fprintf(core, "},\n");

  StackWalk(core, context);

  fprintf(core, "}\n");

  if (core != stdout) {
    fclose(core);
  }

  exit(166);
}

void NaClCrashDumpThreadDestructor(void *arg) {
  munmap(arg, CRASH_STACK_COMPLETE_SIZE);
}

void NaClCrashDumpInit(void) {
  int result;
  result = pthread_key_create(&g_CrashStackKey, NaClCrashDumpThreadDestructor);
  assert(result == 0);
  result = NACL_SYSCALL(exception_handler)(CrashHandler, NULL);
  assert(result == 0);
  NaClCrashDumpInitThread();
}

void NaClCrashDumpInitThread(void) {
  void *stack;
  void *guard;
  int result;
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
  result = NACL_SYSCALL(exception_stack)(stack, CRASH_STACK_COMPLETE_SIZE);
  assert(result == 0);
}
