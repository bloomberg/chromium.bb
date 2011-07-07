// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/logger.h"

#include <stdarg.h>  // va_list

#include "base/stringprintf.h"

namespace remoting {

// Copied from base/logging.cc.
const char* const Logger::log_severity_names[logging::LOG_NUM_SEVERITIES] = {
  "INFO", "WARNING", "ERROR", "ERROR_REPORT", "FATAL" };

Logger::Logger() {
}

Logger::~Logger() {
}

void Logger::Log(logging::LogSeverity severity, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  va_Log(severity, format, ap);
  va_end(ap);
}

void Logger::VLog(int verboselevel, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  va_VLog(verboselevel, format, ap);
  va_end(ap);
}

void Logger::va_Log(logging::LogSeverity severity,
                          const char* format, va_list ap) {
  std::string message;
  base::StringAppendV(&message, format, ap);
  logging::LogMessage(__FILE__, __LINE__, severity).stream() << message;
}

void Logger::va_VLog(int verboselevel, const char* format, va_list ap) {
  std::string message;
  base::StringAppendV(&message, format, ap);
  VLOG(verboselevel) << message;
}

}  // namespace remoting
