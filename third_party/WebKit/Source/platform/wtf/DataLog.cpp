/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/wtf/DataLog.h"

#include "build/build_config.h"

#if defined(OS_POSIX)
#include <pthread.h>
#include <unistd.h>
#endif

#define DATA_LOG_TO_FILE 0

// Uncomment to force logging to the given file regardless of what the
// environment variable says. Note that we will append ".<pid>.txt" where <pid>
// is the PID.

// This path won't work on Windows, make sure to change to something like
// C:\\Users\\<more path>\\log.txt.
#define DATA_LOG_FILENAME "/tmp/WTFLog"

namespace WTF {

#if defined(OS_POSIX)
static pthread_once_t g_initialize_log_file_once_key = PTHREAD_ONCE_INIT;
#endif

static FilePrintStream* g_file;

static void InitializeLogFileOnce() {
#if DATA_LOG_TO_FILE
#ifdef DATA_LOG_FILENAME
  const char* filename = DATA_LOG_FILENAME;
#else
  const char* filename = getenv("WTF_DATA_LOG_FILENAME");
#endif
  char actualFilename[1024];

  snprintf(actualFilename, sizeof(actualFilename), "%s.%d.txt", filename,
           getpid());

  if (filename) {
    file = FilePrintStream::open(actualFilename, "w").release();
    if (!file)
      fprintf(stderr, "Warning: Could not open log file %s for writing.\n",
              actualFilename);
  }
#endif  // DATA_LOG_TO_FILE
  if (!g_file)
    g_file = new FilePrintStream(stderr, FilePrintStream::kBorrow);

  // Prefer unbuffered output, so that we get a full log upon crash or
  // deadlock.
  setvbuf(g_file->File(), nullptr, _IONBF, 0);
}

static void InitializeLogFile() {
#if defined(OS_POSIX)
  pthread_once(&g_initialize_log_file_once_key, InitializeLogFileOnce);
#else
  if (!g_file)
    InitializeLogFileOnce();
#endif
}

FilePrintStream& DataFile() {
  InitializeLogFile();
  return *g_file;
}

void DataLogFV(const char* format, va_list arg_list) {
  DataFile().Vprintf(format, arg_list);
}

void DataLogF(const char* format, ...) {
  va_list arg_list;
  va_start(arg_list, format);
  DataLogFV(format, arg_list);
  va_end(arg_list);
}

}  // namespace WTF
