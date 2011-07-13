// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_PEPPER_CLIENT_LOGGER_H_
#define REMOTING_CLIENT_PLUGIN_PEPPER_CLIENT_LOGGER_H_

#include "remoting/base/logger.h"

#include "base/task.h"

class MessageLoop;

namespace remoting {

class ChromotingInstance;

class PepperClientLogger : public Logger {
 public:
  explicit PepperClientLogger(ChromotingInstance* instance);
  virtual ~PepperClientLogger();

  virtual void LogToUI(const std::string& message);

 private:
  ChromotingInstance* instance_;

  DISALLOW_COPY_AND_ASSIGN(PepperClientLogger);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_PEPPER_CLIENT_LOGGER_H_
