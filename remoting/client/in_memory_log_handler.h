// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_IN_MEMORY_LOG_HANDLER_H_
#define REMOTING_CLIENT_IN_MEMORY_LOG_HANDLER_H_

#include <string>

#include "base/macros.h"

namespace remoting {

// Class for capturing logs in memory before printing out.
class InMemoryLogHandler {
 public:
  // Registers the log handler. This is not thread safe and should be called
  // exactly once in the main function.
  static void Register();

  // Returns most recently captured logs (#lines <= kMaxNumberOfLogs) since the
  // app is launched. This must be called after Register() is called.
  static std::string GetInMemoryLogs();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(InMemoryLogHandler);
};

}  // namespace remoting

#endif  //  REMOTING_CLIENT_IN_MEMORY_LOG_HANDLER_H_
