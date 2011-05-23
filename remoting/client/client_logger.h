// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CLIENT_LOGGER_H_
#define REMOTING_CLIENT_CLIENT_LOGGER_H_

#include "base/basictypes.h"
#include "base/logging.h"

namespace remoting {

class ClientLogger {
 public:
  ClientLogger();
  virtual ~ClientLogger();

  void Log(logging::LogSeverity severity, const char* format, ...);
  void VLog(int verboselevel, const char* format, ...);

  virtual void va_Log(logging::LogSeverity severity,
                      const char* format, va_list ap);
  virtual void va_VLog(int verboselevel, const char* format, va_list ap);

 private:
  DISALLOW_COPY_AND_ASSIGN(ClientLogger);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CLIENT_LOGGER_H_
