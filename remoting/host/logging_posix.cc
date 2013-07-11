// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/logging.h"

#include "base/logging.h"

namespace remoting {

void InitHostLogging() {
  // Write logs to the system debug log.
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  // TODO(alexeypa): remove this line once logging is updated to not create
  // the lock if the log file is not used.
  settings.lock_log = logging::DONT_LOCK_LOG_FILE;
  logging::InitLogging(settings);
}

}  // namespace remoting
