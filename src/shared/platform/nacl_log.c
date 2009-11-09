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
 * NaCl Server Runtime logging code.
 */
#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/portability_process.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define NON_THREAD_SAFE_DETAIL_CHECK  1
/*
 * If set, check detail_level without grabbing a mutex.  This makes
 * logging much cheaper, but implies that the verbosity level should
 * only be changed prior to going multithreaded.
 */

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_log_intern.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_threads.h"
#include "native_client/src/shared/platform/nacl_timestamp.h"

#include "native_client/src/trusted/gio/gio.h"
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

void NaClLogSetFile(char const *log_file) {
  int log_desc = -1;
  FILE            *log_stream;
  struct GioFile  *log_gio;

  log_desc = open(log_file, O_WRONLY | O_APPEND | O_CREAT, 0777);
  if (-1 == log_desc) {
    perror("NaClLogSetFile");
    fprintf(stderr, "Could not create log file\n");
    abort();
  }

  log_stream = FDOPEN(log_desc, "a");
  if (NULL == log_stream) {
    perror("NaClLogSetFile");
    fprintf(stderr, "Could not fdopen log stream\n");
    abort();
  }
  log_gio = malloc(sizeof *log_gio);
  if (NULL == log_gio) {
    perror("NaClLogSetFile");
    fprintf(stderr, "No memory for log buffers\n");
    abort();
  }
  GioFileRefCtor(log_gio, log_stream);
  NaClLogSetGio((struct Gio *) log_gio);
}

void NaClLogModuleInitExtended(enum NaClLogOptions opt) {
  char *log_file;
  char *env_verbosity;

  NaClMutexCtor(&log_mu);
  switch (opt) {
    case NACL_LOG_OPTIONS_DEFAULT_FROM_ENVIRONMENT:
      log_file = getenv("NACLLOG");

      if (NULL != (env_verbosity = getenv("NACLVERBOSITY"))) {
        int v = strtol(env_verbosity, (char **) 0, 0);

        if (v >= 0) {
          NaClLogSetVerbosity(v);
        }
      }
      if (NULL != log_file) {
        NaClLogSetFile(log_file);
      }
      break;
    case NACL_LOG_OPTIONS_NONE:
      break;
  }
}

void NaClLogModuleInit(void) {
  NaClLogModuleInitExtended(NACL_LOG_OPTIONS_DEFAULT_FROM_ENVIRONMENT);
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

void  NaClLogSetVerbosity(int verb) {
  NaClLogLock();
  verbosity = verb;
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

void  NaClLogSetGio(struct Gio *stream) {
  NaClLogLock();
  if (NULL != log_stream) {
    (void) (*log_stream->vtbl->Flush)(log_stream);
  }
  log_stream = stream;
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
    char *env_verbosity = getenv("NACLVERBOSITY");
    if (NULL != env_verbosity) {
      int v = strtol(env_verbosity, (char **) NULL, 0);
      if (v < 0) {
        v = 0;
      }
      verbosity = v;
    } else {
      verbosity = 0;
    }
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
