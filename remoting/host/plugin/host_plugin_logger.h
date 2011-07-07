// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_PLUGIN_HOST_PLUGIN_LOGGER_H_
#define REMOTING_HOST_PLUGIN_HOST_PLUGIN_LOGGER_H_

#include "remoting/base/logger.h"

#include "base/task.h"

class MessageLoop;

namespace remoting {

class HostNPScriptObject;

class HostPluginLogger : public Logger {
 public:
  explicit HostPluginLogger(HostNPScriptObject* scriptable);
  virtual ~HostPluginLogger();

  virtual void va_Log(logging::LogSeverity severity, const char* format,
                      va_list ap);
  virtual void va_VLog(int verboselevel, const char* format, va_list ap);

 private:
  void LogToHostUI(const std::string& message);

  HostNPScriptObject* scriptable_object_;
  MessageLoop* message_loop_;

  DISALLOW_COPY_AND_ASSIGN(HostPluginLogger);
};

}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::HostPluginLogger);

#endif  // REMOTING_HOST_PLUGIN_HOST_PLUGIN_LOGGER_H_
