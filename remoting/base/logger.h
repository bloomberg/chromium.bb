// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_LOGGER_H_
#define REMOTING_BASE_LOGGER_H_

#include "base/basictypes.h"
#include "base/task.h"

class MessageLoop;

namespace remoting {

class Logger {
 public:
  Logger();
  virtual ~Logger();

  void SetThread(MessageLoop* message_loop) {
    message_loop_ = message_loop;
  }

  void Log(logging::LogSeverity severity, const char* format, ...);
  void VLog(int verboselevel, const char* format, ...);

  void va_Log(logging::LogSeverity severity, const char* format, va_list ap);
  void va_VLog(int verboselevel, const char* format, va_list ap);

 protected:
  // Log to the UI after switching to the correct thread.
  void LogToUI_CorrectThread(const std::string& message);

  virtual void LogToUI(const std::string& message);

  static const char* const log_severity_names[];

  // We want all the log messages to be posted to the same thread so that:
  // (1) they are queued up in-order, and
  // (2) we don't try to update the log at the same time from multiple threads.
  MessageLoop* message_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Logger);
};

}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::Logger);

#endif  // REMOTING_BASE_LOGGER_H_
