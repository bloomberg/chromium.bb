// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/openscreen/src/platform/api/logging.h"

#include "base/logging.h"

namespace openscreen {
namespace platform {
namespace {

::logging::LogSeverity MapLogLevel(LogLevel level) {
  switch (level) {
    case LogLevel::kVerbose:
      return ::logging::LOG_VERBOSE;
    case LogLevel::kInfo:
      return ::logging::LOG_INFO;
    case LogLevel::kWarning:
      return ::logging::LOG_WARNING;
    case LogLevel::kError:
      return ::logging::LOG_ERROR;
    case LogLevel::kFatal:
      return ::logging::LOG_FATAL;
  }
}

}  // namespace

// We don't initialize Chromium's logging.
void LogInit(const char* filename) {}

// We don't set Chromium's logging level.
void SetLogLevel(LogLevel level) {}

void LogWithLevel(LogLevel level,
                  absl::string_view file,
                  int line,
                  absl::string_view msg) {
  ::logging::LogMessage(file.data(), line, MapLogLevel(level)).stream() << msg;
}

void Break() {
#if defined(OFFICIAL_BUILD) && defined(NDEBUG)
  IMMEDIATE_CRASH();
#else
  ::base::debug::BreakDebugger();
#endif
}

}  // namespace platform
}  // namespace openscreen
