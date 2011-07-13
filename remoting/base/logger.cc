// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/logger.h"

#include <stdarg.h>  // va_list

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"

namespace remoting {

// Copied from base/logging.cc.
const char* const Logger::log_severity_names[logging::LOG_NUM_SEVERITIES] = {
  "INFO", "WARNING", "ERROR", "ERROR_REPORT", "FATAL"
};

Logger::Logger()
    : message_loop_(NULL) {
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
  DCHECK(severity >= 0 && severity <= logging::LOG_NUM_SEVERITIES);

  // Based in LOG_IS_ON macro in base/logging.h.
  if (severity >= ::logging::GetMinLogLevel()) {
    std::string message;
    base::StringAppendV(&message, format, ap);

    // Standard logging.
    logging::LogMessage(__FILE__, __LINE__, severity).stream() << message;

    // Send log message to the host UI.
    LogToUI_CorrectThread(StringPrintf("LOG(%s) %s",
                                       log_severity_names[severity],
                                       message.c_str()));
  }

  va_end(ap);
}

void Logger::va_VLog(int verboselevel,
                     const char* format,
                     va_list ap) {
  if (VLOG_IS_ON(verboselevel)) {
    std::string message;
    base::StringAppendV(&message, format, ap);

    // Standard verbose logging.
    VLOG(verboselevel) << message;

    // Send log message to the host UI.
    LogToUI_CorrectThread(StringPrintf("VLOG(%d) %s",
                                       verboselevel, message.c_str()));
  }
}

void Logger::LogToUI_CorrectThread(const std::string& message) {
  if (!message_loop_)
    return;

  if (message_loop_ != MessageLoop::current()) {
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &Logger::LogToUI_CorrectThread, message));
    return;
  }

  LogToUI(message);
}

void Logger::LogToUI(const std::string& message) {
}

}  // namespace remoting
