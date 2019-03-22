// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_LOGGER_H_
#define COMPONENTS_INVALIDATION_IMPL_LOGGER_H_

#include <string>

#define TLOG(logger, level, str, ...) \
  logger->Log(Logger::level##_LEVEL, __FILE__, __LINE__, str, ##__VA_ARGS__);

namespace syncer {

// Forward logging messages to the approppriate channel.
class Logger {
 public:
  enum LogLevel { FINE_LEVEL, INFO_LEVEL, WARNING_LEVEL, SEVERE_LEVEL };

  Logger();

  ~Logger();

  // invalidation::Logger implementation.
  void Log(LogLevel level, const char* file, int line, const char* format, ...);
};

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_IMPL_LOGGER_H_
