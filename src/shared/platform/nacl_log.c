/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * NaCl Server Runtime logging code.
 */
#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/portability_process.h"
#include "native_client/src/include/portability_string.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define NON_THREAD_SAFE_DETAIL_CHECK  1
/*
 * If set, check detail_level without grabbing a mutex.  This makes
 * logging much cheaper, but implies that the verbosity level should
 * only be changed prior to going multithreaded.
 */

#include "native_client/src/shared/gio/gio.h"
#include "native_client/src/shared/platform/nacl_exit.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_log_intern.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/shared/platform/nacl_timestamp.h"

/*
 * All logging is protected by this mutex.
 */
static struct NaClMutex log_mu;
static int              tag_output = 0;
static int              abort_on_unlock = 0;
static char const       *nacl_log_module_name = NULL;

#define NACL_VERBOSITY_UNSET INT_MAX

static int              verbosity = NACL_VERBOSITY_UNSET;
static struct Gio       *log_stream = NULL;
static struct GioFile   log_file_stream;
static int              timestamp_enabled = 1;

/* global, but explicitly not exposed in non-test header file */
void (*gNaClLogAbortBehavior)(void) = NaClAbort;

/*
 * For now, we use a simple linked list.  New entries are pushed to
 * the front; search starts at front.  So last entry for a particular
 * module wins, and we don't bother to eliminate duplicates.  The
 * expected number of modules is small, so we don't do anything
 * fancier.  TODO(bsy): measure performance loss and consider
 * alternatives.
 */

struct NaClLogModuleVerbosity {
  struct NaClLogModuleVerbosity *next;
  char const                    *module_name;  /* strdup'd */
  int                           verbosity;
};

static struct NaClLogModuleVerbosity *gNaClLogModuleVerbosity = NULL;

static FILE *NaClLogFileIoBufferFromFile(char const *log_file) {
  int   log_desc;
  FILE  *log_iob;

  log_desc = open(log_file, O_WRONLY | O_APPEND | O_CREAT, 0777);
  if (-1 == log_desc) {
    perror("NaClLogSetFile");
    fprintf(stderr, "Could not create log file\n");
    NaClAbort();
  }

  log_iob = FDOPEN(log_desc, "a");
  if (NULL == log_iob) {
    perror("NaClLogSetFile");
    fprintf(stderr, "Could not fdopen log stream\n");
    NaClAbort();
  }
  return log_iob;
}

/*
 * Setting the log stream buffering to fully buffered, so that the
 * write of the tag string will be less likely to be separated
 * from the write of the actual log message.
 */
static FILE *NaClLogDupFileIo(FILE *orig) {
#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_arm
  /*
   * TODO(cbiffle,bsy): memory allocation early in the startup can
   * result in the system returning memory from the first gigabyte,
   * where the nexe should run.  We need to allocate/squat on this
   * first gig early in the startup code, prior to *any* allocation.
   * Ideally, this is done by linker directives to hold it as BSS,
   * since static Ctors etc might still bite us if we tried to do it
   * as explicit code in main, which runs after the static Ctors run.
   * Similarly, if we had our own static Ctor, we're still at the
   * mercy of static Ctor initialization order, which isn't
   * guaranteed.  Unofficially, static ctors are run in linkage order,
   * so if kernels don't grok elf binaries with a bss in the first
   * gig, we could do it that way.
   *
   * We cannot setvbuf, since that would trigger the allocation.
   */
  return orig;
#else
  int  d;
  FILE *copy;

  /*
   * On windows (at least on a win7 machine which i tested on),
   * fileno(stderr) is -2.  I/O to the stderr stream appears to
   * succeed -- though who knows, maybe fclose(stderr) would actually
   * report an error? -- but DUP of -2 fails.  We don't try to detect
   * -2 (or other windows magic values) as a special case here, since
   * in the future other FILE* might be used here.  Instead, we just
   * check for DUP failure and trundle on as best as we could.
   */
  if (-1 == (d = DUP(fileno(orig)))) {
    copy = orig;
    /* this means that setvbuf later will affect the shared stream */
  } else if (NULL == (copy = FDOPEN(d, "a"))) {
    copy = orig;
    /* ditto */
  }
  (void) setvbuf(copy, (char *) NULL, _IOFBF, 1024);
  return copy;
#endif
}

static struct Gio *NaClLogGioFromFileIoBuffer(FILE *log_iob) {
  struct GioFile *log_gio;

  log_gio = malloc(sizeof *log_gio);
  if (NULL == log_gio) {
    perror("NaClLogSetFile");
    fprintf(stderr, "No memory for log buffers\n");
    NaClAbort();
  }
  if (!GioFileRefCtor(log_gio, log_iob)) {
    fprintf(stderr, "NaClLog module internal error: GioFileRefCtor failed\n");
    NaClAbort();
  }
  return (struct Gio *) log_gio;
}

void NaClLogSetFile(char const *log_file) {
  NaClLogSetGio(NaClLogGioFromFileIoBuffer(
      NaClLogFileIoBufferFromFile(log_file)));
}

int NaClLogDefaultLogVerbosity() {
  char *env_verbosity;

  if (NULL != (env_verbosity = getenv("NACLVERBOSITY"))) {
    int v = strtol(env_verbosity, (char **) 0, 0);

    if (v >= 0) {
      return v;
    }
  }
  return 0;
}

struct Gio *NaClLogDefaultLogGio() {
  char            *log_file;
  FILE            *log_iob;

  log_file = getenv("NACLLOG");

  if (NULL == log_file) {
    log_iob = NaClLogDupFileIo(stderr);
  } else {
    log_iob = NaClLogFileIoBufferFromFile(log_file);
  }
  return NaClLogGioFromFileIoBuffer(log_iob);
}

void NaClLogParseAndSetModuleVerbosityMap(char const *module_verbosity_map) {
  char        entry_buf[256];
  size_t      entry_len;
  char const  *sep;
  char const  *next;
  char        *assign;
  int         seen_global = 0;
  char        *module_name;
  int         module_verbosity;

  if (NULL == module_verbosity_map) {
    return;
  }

  while (*module_verbosity_map != '\0') {
    sep = strpbrk(module_verbosity_map, ",:");
    if (NULL == sep) {
      sep = module_verbosity_map + strlen(module_verbosity_map);
      next = sep;
    } else {
      next = sep + 1;
    }
    /* post: sep points to comma or termination NUL */
    entry_len = sep - module_verbosity_map;
    if (entry_len > sizeof entry_buf - 1) {
      NaClLog(LOG_ERROR,
              "NaClLog: entry too long in module verbosity map \"%.*s\".\n",
              (int) entry_len,
              module_verbosity_map);
      entry_len = sizeof entry_buf - 1;
    }
    strncpy(entry_buf, module_verbosity_map, entry_len);
    entry_buf[entry_len] = '\0';
    assign = strchr(entry_buf, '=');
    if (NULL == assign && !seen_global) {
      verbosity = strtol(entry_buf, (char **) 0, 0);
      seen_global = 1;
    } else {
      *assign = '\0';

      module_verbosity = strtol(assign+1, (char **) 0, 0);

      while (entry_buf < assign && (' ' == assign[-1] || '\t' == assign[-1])) {
        *--assign = '\0';
      }
      if (entry_buf == assign) {
        NaClLog(LOG_FATAL,
                "NaClLog: Bad module name in \"%s\".\n",
                module_verbosity_map);
      }

      for (module_name = entry_buf;
           ' ' == *module_name || '\t' == *module_name;
           ++module_name) {
        ;
      }
      NaClLogSetModuleVerbosity(module_name, module_verbosity);
    }
    module_verbosity_map = next;
  }
}

void NaClLogModuleInitExtended2(int         default_verbosity,
                                char const  *module_verbosity_map,
                                struct Gio  *log_gio) {

  NaClXMutexCtor(&log_mu);
  NaClLogSetVerbosity(default_verbosity);
  NaClLogParseAndSetModuleVerbosityMap(module_verbosity_map);
  NaClLogSetGio(log_gio);
}

void NaClLogModuleInitExtended(int        initial_verbosity,
                               struct Gio *log_gio) {

  NaClLogModuleInitExtended2(initial_verbosity,
                             getenv("NACLVERBOSITY"),
                             log_gio);
}

void NaClLogModuleInit(void) {
  NaClLogModuleInitExtended(NaClLogDefaultLogVerbosity(),
                            NaClLogDefaultLogGio());
}

void NaClLogModuleFini(void) {
  struct NaClLogModuleVerbosity *entry;
  struct NaClLogModuleVerbosity *next;

  entry = gNaClLogModuleVerbosity;
  while (entry != NULL) {
    next = entry->next;
    free(entry);
    entry = next;
  }
  gNaClLogModuleVerbosity = NULL;
  NaClMutexDtor(&log_mu);
}

void NaClLogTagNext_mu(void) {
  tag_output = 1;
}

void NaClLogLock(void) {
  NaClXMutexLock(&log_mu);
  NaClLogTagNext_mu();
}

void NaClLogUnlock(void) {
  if (abort_on_unlock) {
    /*
     * include an easy-to-recognize output for the fuzzer to recognize
     */
    NaClLog_mu(LOG_ERROR, "LOG_FATAL abort exit\n");
#ifdef __COVERITY__
    NaClAbort();  /* help coverity figure out that this is the default */
#else
    (*gNaClLogAbortBehavior)();
#endif
  }
  NaClXMutexUnlock(&log_mu);
}

static INLINE struct Gio *NaClLogGetGio_mu() {
  if (NULL == log_stream) {
    (void) GioFileRefCtor(&log_file_stream, NaClLogDupFileIo(stderr));
    log_stream = (struct Gio *) &log_file_stream;
  }
  return log_stream;
}

static void NaClLogSetVerbosity_mu(int verb) {
  verbosity = verb;
}

void NaClLogPreInitSetVerbosity(int verb) {
  /*
   * The lock used by NaClLogLock has not been initialized and cannot
   * be used; however, prior to initialization we are not going to be
   * invoked from multiple threads, since the caller is responsible
   * for not invoking NaClLog module functions (except for the PreInit
   * ones, obviously) at all, let alone from multiple threads.  Ergo,
   * it is safe to manipulate the module globals without locking.
   */
  NaClLogSetVerbosity_mu(verb);
}

void  NaClLogSetVerbosity(int verb) {
  NaClLogLock();
  NaClLogSetVerbosity_mu(verb);
  NaClLogUnlock();
}

void  NaClLogIncrVerbosity(void) {
  NaClLogLock();
  if (NACL_VERBOSITY_UNSET == verbosity) {
    verbosity = 0;
  }
  ++verbosity;
  NaClLogUnlock();
}

int NaClLogGetVerbosity(void) {
  int v;

  NaClLogLock();
  if (NACL_VERBOSITY_UNSET == verbosity) {
    verbosity = 0;
  }
  v = verbosity;
  NaClLogUnlock();

  return v;
}

static void NaClLogSetGio_mu(struct Gio *stream) {
  if (NULL != log_stream) {
    (void) (*log_stream->vtbl->Flush)(log_stream);
  }
  log_stream = stream;
}

void NaClLogPreInitSetGio(struct Gio *out_stream) {
  /*
   * See thread safety comment in NaClLogPreInitSetVerbosity.
   */
  NaClLogSetGio_mu(out_stream);
}

void NaClLogSetGio(struct Gio *stream) {
  NaClLogLock();
  NaClLogSetGio_mu(stream);
  NaClLogUnlock();
}

struct Gio  *NaClLogGetGio(void) {
  struct Gio  *s;

  NaClLogLock();
  s = NaClLogGetGio_mu();
  NaClLogUnlock();

  return s;
}

void NaClLogEnableTimestamp(void) {
  timestamp_enabled = 1;
}

void NaClLogDisableTimestamp(void) {
  timestamp_enabled = 0;
}

static void NaClLogOutputTag_mu(struct Gio *s) {
  char timestamp[128];
  int  pid;

  if (timestamp_enabled && tag_output) {
    pid = GETPID();
    gprintf(s, "[%d,%u:%s] ",
            pid,
            NaClThreadId(),
            NaClTimeStampString(timestamp, sizeof timestamp));
    tag_output = 0;
  }
}

/*
 * Output a printf-style formatted message if the log verbosity level
 * is set higher than the log output's detail level.  Note that since
 * this is not a macro, log message arguments that have side effects
 * will have their side effects regardless of whether the
 * corresponding log message is printed or not.  This is good from a
 * consistency point of view, but it means that should a logging
 * argument be expensive to compute, the log statement needs to be
 * surrounded by something like
 *
 *  if (detail_level <= NaClLogGetVerbosity()) {
 *    NaClLog(detail_level, "format string", expensive_arg(), ...);
 *  }
 *
 * The log message, if written, is prepended by a microsecond
 * resolution timestamp on linux and a millisecond resolution
 * timestamp on windows.  This means that if the NaCl app can read its
 * own logs, it can distinguish which host OS it is running on.
 */
void NaClLogDoLogV_mu(int         detail_level,
                      char const  *fmt,
                      va_list     ap) {
  struct Gio  *s;

  s = NaClLogGetGio_mu();

  NaClLogOutputTag_mu(s);
  (void) gvprintf(s, fmt, ap);
  (void) (*s->vtbl->Flush)(s);

  if (LOG_FATAL == detail_level) {
    abort_on_unlock = 1;
  }
}

void NaClLogV_mu(int        detail_level,
                 char const *fmt,
                 va_list    ap) {
  if (NACL_VERBOSITY_UNSET == verbosity) {
    verbosity = NaClLogDefaultLogVerbosity();
  }

  if (detail_level <= verbosity) {
    NaClLogDoLogV_mu(detail_level, fmt, ap);
  }
}

void NaClLogV(int         detail_level,
              char const  *fmt,
              va_list     ap) {
#if NON_THREAD_SAFE_DETAIL_CHECK
  if (detail_level > verbosity) {
    return;
  }
#endif
  NaClLogLock();
  NaClLogV_mu(detail_level, fmt, ap);
  NaClLogUnlock();
}

void NaClLogSetModuleVerbosity_mu(char const  *module_name,
                                  int         verbosity) {
  struct NaClLogModuleVerbosity *entry;

  entry = (struct NaClLogModuleVerbosity *) malloc(sizeof *entry);
  if (NULL == entry) {
    NaClLog_mu(LOG_FATAL,
               ("NaClLogSetModuleVerbosity_mu: Out of memory while setting"
                " module record for module: %s, verbosity: %d\n"),
               module_name, verbosity);
  }
  entry->module_name = STRDUP(module_name);
  if (NULL == entry->module_name) {
    NaClLog_mu(LOG_FATAL,
               ("NaClLogSetModuleVerbosity_mu: Out of memory while duplicating"
                " module name: %s, verbosity: %d\n"),
               module_name, verbosity);
  }
  entry->verbosity = verbosity;
  entry->next = gNaClLogModuleVerbosity;
  gNaClLogModuleVerbosity = entry;
}

void NaClLogSetModuleVerbosity(char const *module_name,
                               int        verbosity) {
  NaClLogLock();
  NaClLogSetModuleVerbosity_mu(module_name, verbosity);
  NaClLogUnlock();
}

int NaClLogGetModuleVerbosity_mu(char const *module_name) {
  struct NaClLogModuleVerbosity *p;

  if (NULL != module_name) {
    for (p = gNaClLogModuleVerbosity; NULL != p; p = p->next) {
      if (!strcmp(p->module_name, module_name)) {
        return p->verbosity;
      }
    }
  }
  return verbosity;
}

int NaClLogGetModuleVerbosity(char const *module_name) {
  int rv;
  NaClLogLock();
  rv = NaClLogGetModuleVerbosity_mu(module_name);
  NaClLogUnlock();
  return rv;
}

int NaClLogLockAndSetModule(char const *module_name) {
  NaClLogLock();
  nacl_log_module_name = module_name;
  return 0;
}

void NaClLogDoLogAndUnlockV(int         detail_level,
                            char const  *fmt,
                            va_list     ap) {
  int module_verbosity;

  module_verbosity = NaClLogGetModuleVerbosity_mu(nacl_log_module_name);
  if (detail_level <= module_verbosity) {
    NaClLogDoLogV_mu(detail_level, fmt, ap);
  }
  nacl_log_module_name = NULL;
  NaClLogUnlock();
}

void NaClLogDoLogAndUnlock(int        detail_level,
                           char const *fmt,
                           ...) {
  va_list ap;

  va_start(ap, fmt);
  NaClLogDoLogAndUnlockV(detail_level, fmt, ap);
  va_end(ap);
}

void NaClLog(int         detail_level,
             char const  *fmt,
             ...) {
  va_list ap;

#if NON_THREAD_SAFE_DETAIL_CHECK
  if (detail_level > verbosity) {
    return;
  }
#endif

  NaClLogLock();
  va_start(ap, fmt);
  NaClLogV_mu(detail_level, fmt, ap);
  va_end(ap);
  NaClLogUnlock();
}

void NaClLog_mu(int         detail_level,
                char const  *fmt,
                ...) {
  va_list ap;

#if NON_THREAD_SAFE_DETAIL_CHECK
  if (detail_level > verbosity) {
    return;
  }
#endif

  va_start(ap, fmt);
  NaClLogV_mu(detail_level, fmt, ap);
  va_end(ap);
}

void NaClLog2(char const *module_name,
              int        detail_level,
              char const *fmt,
              ...) {
  va_list ap;

  (void) NaClLogLockAndSetModule(module_name);
  va_start(ap, fmt);
  NaClLogDoLogAndUnlockV(detail_level, fmt, ap);
  va_end(ap);
}
