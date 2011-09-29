// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/plugin/host_log_handler.h"

#include "remoting/base/util.h"
#include "remoting/host/plugin/host_script_object.h"

namespace remoting {

// Records whether or not we have a scriptable object registered for logging.
// This is set inside the lock, but is read (in LogToUI) outside of a lock so
// that we don't needlessly slow down the system when we log.
static bool g_has_logging_scriptable_object = false;

// The lock that protects the logging globals.
static base::Lock g_logging_lock;

// The scriptable object that will display the log information to the user.
static HostNPScriptObject* g_logging_scriptable_object = NULL;

// The previously registered LogMessageHandler. If not NULL, we call this after
// we're doing processing the log message.
static logging::LogMessageHandlerFunction g_logging_old_handler = NULL;

// Set to true when we register our global log handler so that we don't try
// to register it twice.
static bool g_has_registered_log_handler = false;

// static
void HostLogHandler::RegisterLogMessageHandler() {
  base::AutoLock lock(g_logging_lock);

  if (!g_has_registered_log_handler)
    return;

  LOG(INFO) << "Registering global log handler";

  // Record previous handler so we can call it in a chain.
  g_logging_old_handler = logging::GetLogMessageHandler();

  // Set up log message handler.
  // This is not thread-safe so we need it within our lock.
  // Note that this will not log anything until a scriptable object instance
  // has been created to handle the log message display.
  logging::SetLogMessageHandler(&LogToUI);
}

// static
void HostLogHandler::RegisterLoggingScriptObject(
    HostNPScriptObject* script_object) {
  base::AutoLock lock(g_logging_lock);

  VLOG(1) << "Registering log handler scriptable object";

  // Register this script object as the one that will handle all logging calls
  // and display them to the user.
  // If multiple plugins are run, then the last one registered will handle all
  // logging for all instances.
  g_logging_scriptable_object = script_object;
  g_has_logging_scriptable_object = true;
}

// static
void HostLogHandler::UnregisterLoggingScriptObject(
    HostNPScriptObject* script_object) {
  base::AutoLock lock(g_logging_lock);

  // Ignore unless we're the currently registered script object.
  if (script_object != g_logging_scriptable_object)
    return;

  // Unregister this script object for logging.
  g_has_logging_scriptable_object = false;
  g_logging_scriptable_object = NULL;

  VLOG(1) << "Unregistering log handler scriptable object";
}

// static
bool HostLogHandler::LogToUI(int severity, const char* file, int line,
                             size_t message_start,
                             const std::string& str) {
  // Note: We're reading |g_has_logging_scriptable_object| outside of a lock.
  // This lockless read is done so that we don't needlessly slow down global
  // logging with a lock for each log message.
  //
  // This lockless read is safe because:
  //
  // Misreading a false value (when it should be true) means that we'll simply
  // skip processing a few log messages.
  //
  // Misreading a true value (when it should be false) means that we'll take
  // the lock and check |g_logging_scriptable_object| unnecessarily. This is not
  // problematic because we always set |g_logging_scriptable_object| inside a
  // lock.
  //
  // Misreading an old cached value is also not problematic for the same
  // reasons: a mis-read either skips a log message or causes us to take a lock
  // unnecessarily.
  if (g_has_logging_scriptable_object) {
    base::AutoLock lock(g_logging_lock);

    if (g_logging_scriptable_object) {
      std::string message = remoting::GetTimestampString();
      message += (str.c_str() + message_start);
      g_logging_scriptable_object->PostLogDebugInfo(message);
    }
  }

  // Call the next log handler in the chain.
  if (g_logging_old_handler)
    return (g_logging_old_handler)(severity, file, line, message_start, str);

  return false;
}

}  // namespace remoting
