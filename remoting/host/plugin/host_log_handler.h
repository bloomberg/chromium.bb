// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_PLUGIN_HOST_LOG_HANDLER_H_
#define REMOTING_HOST_PLUGIN_HOST_LOG_HANDLER_H_

#include <string>

namespace remoting {

class HostNPScriptObject;

class HostLogHandler {
 public:
  // Register the log handler.
  // These should be called from the plugin init/destroy methods so that they
  // are only called once per plugin process (not once per plugin instance).
  static void RegisterLogMessageHandler();

  // We don't have the corresponding UnregisterLogMessageHandler because it
  // is not safe to call it when there are multiple threads that might be
  // logging.

  static void RegisterLoggingScriptObject(HostNPScriptObject* script_object);
  static void UnregisterLoggingScriptObject(HostNPScriptObject* script_object);

 private:
  // A Log Message Handler that is called after each LOG message has been
  // processed. This must be of type LogMessageHandlerFunction defined in
  // base/logging.h.
  static bool LogToUI(int severity, const char* file, int line,
                      size_t message_start, const std::string& str);
};

}  // namespace remoting

#endif  // REMOTING_HOST_PLUGIN_HOST_LOG_HANDLER_H_
