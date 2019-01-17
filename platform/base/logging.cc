// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>

#include <cstdlib>
#include <iostream>
#include <sstream>

#include "platform/api/logging.h"

namespace openscreen {
namespace platform {
namespace {

struct CombinedLogLevel {
  LogLevel level;
  int verbose_level;
};

bool operator<(const CombinedLogLevel& l1, const CombinedLogLevel& l2) {
  if (l1.level < l2.level) {
    return true;
  } else if (l2.level < l1.level) {
    return false;
  } else if (l1.level == LogLevel::kVerbose) {
    return l1.verbose_level > l2.verbose_level;
  }
  return false;
}

CombinedLogLevel g_log_level{LogLevel::kInfo, 0};

std::ostream& operator<<(std::ostream& os, const CombinedLogLevel& level) {
  os << LogLevelToString(level.level);
  if (level.level == LogLevel::kVerbose)
    os << "(" << level.verbose_level << ")";

  return os;
}

}  // namespace

int g_log_fd;

void SetLogLevel(LogLevel level, int verbose_level) {
  g_log_level = CombinedLogLevel{level, verbose_level};
}

void LogWithLevel(LogLevel level,
                  int verbose_level,
                  absl::string_view file,
                  int line,
                  absl::string_view msg) {
  if (CombinedLogLevel{level, verbose_level} < g_log_level)
    return;

  std::stringstream ss;
  ss << "[" << CombinedLogLevel{level, verbose_level} << ":" << file << ":"
     << line << "] " << msg << std::endl;
  write(g_log_fd, ss.str().c_str(), ss.str().size());
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
