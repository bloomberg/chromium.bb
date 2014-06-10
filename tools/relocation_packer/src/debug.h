// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Logging and checks.
//
// Log messages to stdout.  LOG() prints normal user messages, VLOG()
// is verbose, for tracing and debugging.  SetVerbose() enables/disables
// VLOG() output.
//
// LOG() and VLOG() are printf-like, and arguments are checked by gcc.
// LOG_IF() and VLOG_IF() call LOG/VLOG if their predicate is true.
// CHECK() aborts if its predicate is false.  NOTREACHED() always aborts.
// Logging is not thread-safe.
//

#ifndef TOOLS_RELOCATION_PACKER_SRC_DEBUG_H_
#define TOOLS_RELOCATION_PACKER_SRC_DEBUG_H_

#ifdef NDEBUG
#undef NDEBUG
#include <assert.h>
#define NDEBUG
#else
#include <assert.h>
#endif

#include <stdarg.h>
#include <string.h>

namespace relocation_packer {

// If gcc, define PRINTF_ATTRIBUTE so that gcc checks Log() as printf-like.
#if defined(__GNUC__) && (__GNUC__ >= 3)
#define PRINTF_ATTRIBUTE(string_index, first_to_check) \
    __attribute__((__format__(__printf__, string_index, first_to_check)))
#else
#define PRINTF_ATTRIBUTE(string_index, first_to_check)
#endif

// Logging and checking macros.
#define LOG(...) ::relocation_packer::Logger::Log(__VA_ARGS__)
#define VLOG(...) ::relocation_packer::Logger::VLog(__VA_ARGS__)
#define LOG_IF(cond, ...)  \
  do {                     \
    if ((cond))            \
      LOG(__VA_ARGS__);    \
  } while (0)
#define VLOG_IF(cond, ...) \
  do {                     \
    if ((cond))            \
      VLOG(__VA_ARGS__);   \
  } while (0)

#define CHECK(expr) assert((expr))
#define NOTREACHED(_) assert(false)

class Logger {
 public:
  // Log and verbose log to stdout.
  // |format| is a printf format string.
  static void Log(const char* format, ...) PRINTF_ATTRIBUTE(1, 2);
  static void VLog(const char* format, ...) PRINTF_ATTRIBUTE(1, 2);

  // Set verbose mode.
  // |flag| is true to enable verbose logging, false to disable it.
  static void SetVerbose(bool flag);

 private:
  Logger() : is_verbose_(false) { }
  ~Logger() {}

  // Implementation of log to stdout.
  // |format| is a printf format string.
  // |args| is a varargs list of printf arguments.
  void Log(const char* format, va_list args);

  // If set, VLOG is enabled, otherwise it is a no-op.
  bool is_verbose_;

  // Singleton support.  Not thread-safe.
  static Logger* GetInstance();
  static Logger* instance_;
};

}  // namespace relocation_packer

#endif  // TOOLS_RELOCATION_PACKER_SRC_DEBUG_H_
