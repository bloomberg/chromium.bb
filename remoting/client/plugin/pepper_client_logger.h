// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_CLIENT_LOGGER_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_CLIENT_LOGGER_H_

#include "remoting/client/client_logger.h"

#include "base/task.h"

class MessageLoop;

namespace remoting {

class ChromotingInstance;

class PepperClientLogger : public ClientLogger {
 public:
  PepperClientLogger(ChromotingInstance* instance);
  virtual ~PepperClientLogger();

  virtual void va_Log(logging::LogSeverity severity, const char* format,
                      va_list ap);
  virtual void va_VLog(int verboselevel, const char* format, va_list ap);

 private:
  void LogToClientUI(const std::string& message);

  ChromotingInstance* instance_;
  MessageLoop* message_loop_;

  DISALLOW_COPY_AND_ASSIGN(PepperClientLogger);
};

}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::PepperClientLogger);

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_CLIENT_LOGGER_H_
