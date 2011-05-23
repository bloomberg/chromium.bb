// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "client_logger.h"

#include <stdarg.h>  // va_list

#include "base/stringprintf.h"

namespace remoting {

ClientLogger::ClientLogger() {
}

ClientLogger::~ClientLogger() {
}

void ClientLogger::Log(logging::LogSeverity severity, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  va_Log(severity, format, ap);
  va_end(ap);
}

void ClientLogger::VLog(int verboselevel, const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  va_VLog(verboselevel, format, ap);
  va_end(ap);
}

void ClientLogger::va_Log(logging::LogSeverity severity,
                          const char* format, va_list ap) {
  std::string message;
  base::StringAppendV(&message, format, ap);
  logging::LogMessage(__FILE__, __LINE__, severity).stream() << message;
}

void ClientLogger::va_VLog(int verboselevel, const char* format, va_list ap) {
  std::string message;
  base::StringAppendV(&message, format, ap);
  VLOG(verboselevel) << message;
}

}  // namespace remoting
