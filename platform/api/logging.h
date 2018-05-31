// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_API_LOGGING_H_
#define PLATFORM_API_LOGGING_H_

#include <sstream>

namespace openscreen {
namespace platform {

enum class LogLevel {
  kVerbose = 0,
  kInfo,
  kWarning,
  kError,
  kFatal,
};

std::string LogLevelToString(LogLevel level);

//
// PLATFORM IMPLEMENTATION
// The follow functions must be implemented by the platform.
void SetLogLevel(LogLevel level, int verbose_level = 0);
void LogWithLevel(LogLevel level,
                  int verbose_level,
                  const char* file,
                  int line,
                  const char* msg);
void Break();
//
// END PLATFORM IMPLEMENTATION
//

// The stream-based logging macros below are adapted from Chromium's
// base/logging.h.
class LogMessage {
 public:
  LogMessage(LogLevel level, int verbose_level, const char* file, int line);
  ~LogMessage();

  std::ostream& stream() { return stream_; }

 private:
  const LogLevel level_;
  const int verbose_level_;
  const char* const file_;
  const int line_;
  std::ostringstream stream_;
};

#define VLOG(l)                                                          \
  ::openscreen::platform::LogMessage(                                    \
      ::openscreen::platform::LogLevel::kVerbose, l, __FILE__, __LINE__) \
      .stream()
#define LOG_INFO                                                              \
  ::openscreen::platform::LogMessage(::openscreen::platform::LogLevel::kInfo, \
                                     0, __FILE__, __LINE__)                   \
      .stream()
#define LOG_WARN                                                         \
  ::openscreen::platform::LogMessage(                                    \
      ::openscreen::platform::LogLevel::kWarning, 0, __FILE__, __LINE__) \
      .stream()
#define LOG_ERROR                                                              \
  ::openscreen::platform::LogMessage(::openscreen::platform::LogLevel::kError, \
                                     0, __FILE__, __LINE__)                    \
      .stream()
#define LOG_FATAL                                                              \
  ::openscreen::platform::LogMessage(::openscreen::platform::LogLevel::kFatal, \
                                     0, __FILE__, __LINE__)                    \
      .stream()

namespace detail {

class Voidify {
 public:
  Voidify() = default;
  void operator&(std::ostream&) {}
};

}  // namespace detail

#define LAZY_STREAM(stream, condition) \
  !(condition) ? (void)0 : ::openscreen::platform::detail::Voidify() & (stream)
#define EAT_STREAM LAZY_STREAM(LOG_INFO, false)
#define LOG_IF(level, condition) LAZY_STREAM(LOG_##level, (condition))
#define VLOG_IF(level, condition) LAZY_STREAM(VLOG(level), (condition))

#define CHECK(condition) \
  LAZY_STREAM(LOG_FATAL, !(condition)) << "CHECK(" << #condition << ") failed: "

#if defined(_DEBUG) || defined(DCHECK_ALWAYS_ON)
#define DCHECK_IS_ON() 1
#define DCHECK(condition) CHECK(condition)
#else
#define DCHECK_IS_ON() 0
#define DCHECK(condition) EAT_STREAM
#endif

}  // namespace platform
}  // namespace openscreen

#endif
