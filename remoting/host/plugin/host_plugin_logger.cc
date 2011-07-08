// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/plugin/host_plugin_logger.h"

#include <stdarg.h>  // va_list

#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "remoting/host/plugin/host_script_object.h"

namespace remoting {

HostPluginLogger::HostPluginLogger(HostNPScriptObject* scriptable)
    : scriptable_object_(scriptable),
      message_loop_(MessageLoop::current()) {
}

HostPluginLogger::~HostPluginLogger() {
}

void HostPluginLogger::va_Log(logging::LogSeverity severity,
                              const char* format, va_list ap) {
  DCHECK(severity >= 0 && severity <= logging::LOG_NUM_SEVERITIES);

  // Based in LOG_IS_ON macro in base/logging.h.
  if (severity >= ::logging::GetMinLogLevel()) {
    std::string message;
    base::StringAppendV(&message, format, ap);

    // Standard logging.
    logging::LogMessage(__FILE__, __LINE__, severity).stream() << message;

    // Send log message to the host UI.
    LogToHostUI(StringPrintf("LOG(%s) %s",
                             log_severity_names[severity], message.c_str()));
  }

  va_end(ap);
}

void HostPluginLogger::va_VLog(int verboselevel,
                               const char* format,
                               va_list ap) {
  if (VLOG_IS_ON(verboselevel)) {
    std::string message;
    base::StringAppendV(&message, format, ap);

    // Standard verbose logging.
    VLOG(verboselevel) << message;

    // Send log message to the host UI.
    LogToHostUI(StringPrintf("VLOG(%d) %s", verboselevel, message.c_str()));
  }
}

void HostPluginLogger::LogToHostUI(const std::string& message) {
  // TODO(garykac): Fix crbug.com/88792
  return;
  if (message_loop_ != MessageLoop::current()) {
    message_loop_->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &HostPluginLogger::LogToHostUI, message));
    return;
  }

  scriptable_object_->LogDebugInfo(message);
}

}  // namespace remoting
