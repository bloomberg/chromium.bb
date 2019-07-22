// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/api/logging.h"

#include <unistd.h>

#include <cstdlib>
#include <iostream>
#include <sstream>

#include "platform/api/trace_logging.h"

namespace openscreen {
namespace platform {
namespace {

LogLevel g_log_level = LogLevel::kWarning;

std::ostream& operator<<(std::ostream& os, const LogLevel& level) {
  os << LogLevelToString(level);

  return os;
}

}  // namespace

int g_log_fd;

void SetLogLevel(LogLevel level) {
  g_log_level = level;
}

void LogWithLevel(LogLevel level,
                  absl::string_view file,
                  int line,
                  absl::string_view msg) {
  if (level < g_log_level)
    return;

  std::stringstream ss;
  ss << "[" << level << ":" << file << "(" << line << "):T" << std::hex
     << TRACE_CURRENT_ID << "] " << msg << '\n';
  const auto ss_str = ss.str();
  const auto bytes_written = write(g_log_fd, ss_str.c_str(), ss_str.size());
  OSP_DCHECK(bytes_written);
}

void Break() {
#if defined(_DEBUG)
  __builtin_trap();
#else
  std::abort();
#endif
}

}  // namespace platform
}  // namespace openscreen
