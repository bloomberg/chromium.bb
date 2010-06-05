/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl Server Runtime logging code.
 */
#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/portability_process.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <sys/stat.h>
#include <fcntl.h>

#define NON_THREAD_SAFE_DETAIL_CHECK  1
/*
 * If set, check detail_level without grabbing a mutex.  This makes
 * logging much cheaper, but implies that the verbosity level should
 * only be changed prior to going multithreaded.
 */

#include "native_client/src/shared/gio/gio.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_log_intern.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/shared/platform/nacl_timestamp.h"

/*
 * All logging is protected by this mutex.
 */
static struct NaClMutex log_mu;
static int              tag_output = 0;
static int              abort_on_unlock = 0;

#define NACL_VERBOSITY_UNSET INT_MAX

static int              verbosity = NACL_VERBOSITY_UNSET;
static struct Gio       *log_stream = NULL;
static struct GioFile   log_file_stream;
static int              timestamp_enabled = 1;

/* global, but explicitly not exposed in non-test header file */
void (*gNaClLogAbortBehavior)(void) = abort;

static FILE *NaClLogFileIoBufferFromFile(char const *log_file) {
  int   log_desc;
  FILE  *log_iob;

  log_desc = open(log_file, O_WRONLY | O_APPEND | O_CREAT, 0777);
  if (-1 == log_desc) {
    perror("NaClLogSetFile");
    fprintf(stderr, "Could not create log file\n");
    abort();
  }

  log_iob = FDOPEN(log_desc, "a");
  if (NULL == log_iob) {
    perror("NaClLogSetFile");
    fprintf(stderr, "Could not fdopen log stream\n");
    abort();
  }
  return log_iob;
}

static struct Gio *NaClLogGioFromFileIoBuffer(FILE *log_iob) {
  struct GioFile *log_gio;

  log_gio = malloc(sizeof *log_gio);
  if (NULL == log_gio) {
    perror("NaClLogSetFile");
    fprintf(stderr, "No memory for log buffers\n");
    abort();
  }
  if (!GioFileRefCtor(log_gio, log_iob)) {
    fprintf(stderr, "NaClLog module internal error: GioFileRefCtor failed\n");
    abort();
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
    log_iob = stderr;
  } else {
    log_iob = NaClLogFileIoBufferFromFile(log_file);
  }
  return NaClLogGioFromFileIoBuffer(log_iob);
}

void NaClLogModuleInitExtended(int        initial_verbosity,
                               struct Gio *log_gio) {

  NaClMutexCtor(&log_mu);
  NaClLogSetVerbosity(initial_verbosity);
  NaClLogSetGio(log_gio);
}

void NaClLogModuleInit(void) {
  NaClLogModuleInitExtended(NaClLogDefaultLogVerbosity(),
                            NaClLogDefaultLogGio());
}

void NaClLogModuleFini(void) {
  NaClMutexDtor(&log_mu);
}

void NaClLogTagNext_mu(void) {
  tag_output = 1;
}

void NaClLogLock(void) {
  NaClMutexLock(&log_mu);
  NaClLogTagNext_mu();
}

void NaClLogUnlock(void) {
  if (abort_on_unlock) {
#ifdef __COVERITY__
    abort();  /* help coverity figure out that this is the default behavior */
#else
    (*gNaClLogAbortBehavior)();
#endif
  }
  NaClMutexUnlock(&log_mu);
}

static INLINE struct Gio *NaClLogGetGio_mu() {
  if (NULL == log_stream) {
    (void) GioFileRefCtor(&log_file_stream, stderr);
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

void  NaClLogSetGio(struct Gio *stream) {
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
void  NaClLogV_mu(int         detail_level,
                  char const  *fmt,
                  va_list     ap) {
  struct Gio  *s;

  s = NaClLogGetGio_mu();

  if (NACL_VERBOSITY_UNSET == verbosity) {
    verbosity = NaClLogDefaultLogVerbosity();
  }

  if (detail_level <= verbosity) {
    NaClLogOutputTag_mu(s);
    (void) gvprintf(s, fmt, ap);
    (void) (*s->vtbl->Flush)(s);
  }

  if (LOG_FATAL == detail_level) {
    abort_on_unlock = 1;
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

void  NaClLog(int         detail_level,
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

void  NaClLog_mu(int         detail_level,
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
