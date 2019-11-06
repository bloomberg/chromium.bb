// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_FAKE_TETHER_SESSION_COMPLETION_LOGGER_H_
#define CHROMEOS_COMPONENTS_TETHER_FAKE_TETHER_SESSION_COMPLETION_LOGGER_H_

#include <memory>

#include "base/macros.h"
#include "chromeos/components/tether/tether_session_completion_logger.h"

namespace chromeos {

namespace tether {

// Test double for TetherSessionCompletionLogger.
class FakeTetherSessionCompletionLogger : public TetherSessionCompletionLogger {
 public:
  FakeTetherSessionCompletionLogger();
  ~FakeTetherSessionCompletionLogger() override;

  TetherSessionCompletionLogger::SessionCompletionReason*
  last_session_completion_reason() {
    return last_session_completion_reason_.get();
  }

  // TetherSessionCompletionLogger:
  void RecordTetherSessionCompletion(
      const SessionCompletionReason& reason) override;

 private:
  std::unique_ptr<TetherSessionCompletionLogger::SessionCompletionReason>
      last_session_completion_reason_;

  DISALLOW_COPY_AND_ASSIGN(FakeTetherSessionCompletionLogger);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_FAKE_TETHER_SESSION_COMPLETION_LOGGER_H_
