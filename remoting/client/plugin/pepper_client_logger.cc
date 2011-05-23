// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_client_logger.h"

#include <stdarg.h>  // va_list

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "remoting/client/plugin/chromoting_instance.h"

namespace remoting {

PepperClientLogger::PepperClientLogger(ChromotingInstance* instance)
    : instance_(instance),
      message_loop_(MessageLoop::current()) {
}

PepperClientLogger::~PepperClientLogger() {
}

// Copied from base/logging.cc.
const char* const log_severity_names[logging::LOG_NUM_SEVERITIES] = {
  "INFO", "WARNING", "ERROR", "ERROR_REPORT", "FATAL" };

void PepperClientLogger::va_Log(logging::LogSeverity severity,
                                const char* format, va_list ap) {
  DCHECK(severity >= 0 && severity <= logging::LOG_NUM_SEVERITIES);

  // Based in LOG_IS_ON macro in base/logging.h.
  if (severity >= ::logging::GetMinLogLevel()) {
    std::string message;
    base::StringAppendV(&message, format, ap);

    // Standard logging.
    logging::LogMessage(__FILE__, __LINE__, severity).stream() << message;

    // Send log message to the Chromoting instance so that it can be sent to the
    // client UI.
    LogToClientUI(StringPrintf("LOG(%s) %s",
                               log_severity_names[severity], message.c_str()));
  }

  va_end(ap);
}

void PepperClientLogger::va_VLog(int verboselevel,
                                 const char* format,
                                 va_list ap) {
  if (VLOG_IS_ON(verboselevel)) {
    std::string message;
    base::StringAppendV(&message, format, ap);

    // Standard verbose logging.
    VLOG(verboselevel) << message;

    // Send log message to the Chromoting instance so that it can be sent to the
    // client UI.
    LogToClientUI(StringPrintf("VLOG(%d) %s", verboselevel, message.c_str()));
  }
}

void PepperClientLogger::LogToClientUI(const std::string& message) {
  if (message_loop_ != MessageLoop::current()) {
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &PepperClientLogger::LogToClientUI, message));
    return;
  }

  instance_->GetScriptableObject()->LogDebugInfo(message);
}

}  // namespace remoting
