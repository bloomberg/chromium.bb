// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/system_event_logger.h"

#include <syslog.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace remoting {

namespace {

class SystemEventLoggerLinux : public SystemEventLogger {
 public:
  SystemEventLoggerLinux(const std::string& application_name)
      : application_name_(application_name) {
    openlog(application_name_.c_str(), 0, LOG_USER);
  }

  ~SystemEventLoggerLinux() {
    closelog();
  }

  virtual void Log(const std::string& message) OVERRIDE {
    syslog(LOG_USER | LOG_NOTICE, "%s", message.c_str());
  }

 private:
  // Store this string here, to avoid deleting memory passed to openlog().
  std::string application_name_;

  DISALLOW_COPY_AND_ASSIGN(SystemEventLoggerLinux);
};

}  // namespace

// static
scoped_ptr<SystemEventLogger> SystemEventLogger::Create(
    const std::string& application_name) {
  return scoped_ptr<SystemEventLogger>(
      new SystemEventLoggerLinux(application_name));
}

}  // namespace remoting
