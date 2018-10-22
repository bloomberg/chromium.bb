// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/logger.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"

namespace syncer {

Logger::Logger() {}
Logger::~Logger() {}

void Logger::Log(LogLevel level,
                 const char* file,
                 int line,
                 const char* format,
                 ...) {
  logging::LogSeverity log_severity = -2;  // VLOG(2)
  bool emit_log = false;
  switch (level) {
    case FINE_LEVEL:
      log_severity = -2;  // VLOG(2)
      emit_log = VLOG_IS_ON(2);
      break;
    case INFO_LEVEL:
      log_severity = -1;  // VLOG(1)
      emit_log = VLOG_IS_ON(1);
      break;
    case WARNING_LEVEL:
      log_severity = logging::LOG_WARNING;
      emit_log = LOG_IS_ON(WARNING);
      break;
    case SEVERE_LEVEL:
      log_severity = logging::LOG_ERROR;
      emit_log = LOG_IS_ON(ERROR);
      break;
  }
  if (emit_log) {
    va_list ap;
    va_start(ap, format);
    std::string result;
    base::StringAppendV(&result, format, ap);
    logging::LogMessage(file, line, log_severity).stream() << result;
    va_end(ap);
  }
}

}  // namespace syncer
