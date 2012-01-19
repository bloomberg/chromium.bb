// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SYSTEM_EVENT_LOGGER_H_
#define REMOTING_HOST_SYSTEM_EVENT_LOGGER_H_

#include <string>

#include "base/memory/scoped_ptr.h"

namespace remoting {

class SystemEventLogger {
 public:
  virtual ~SystemEventLogger() {}

  virtual void Log(const std::string& message) = 0;

  // Create an event-logger that tags log messages with |application_name|.
  static scoped_ptr<SystemEventLogger> Create(
      const std::string& application_name);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SYSTEM_EVENT_LOGGER_H_
