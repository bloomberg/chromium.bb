// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_client_logger.h"

//#include <stdarg.h>  // va_list

//#include "base/logging.h"
#include "base/message_loop.h"
//#include "base/stringprintf.h"
#include "remoting/client/plugin/chromoting_instance.h"

namespace remoting {

PepperClientLogger::PepperClientLogger(ChromotingInstance* instance)
    : instance_(instance) {
  SetThread(MessageLoop::current());
}

PepperClientLogger::~PepperClientLogger() {
}

void PepperClientLogger::LogToUI(const std::string& message) {
  instance_->GetScriptableObject()->LogDebugInfo(message);
}

}  // namespace remoting
