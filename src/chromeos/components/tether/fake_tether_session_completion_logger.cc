// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/tether/fake_tether_session_completion_logger.h"

namespace chromeos {

namespace tether {

FakeTetherSessionCompletionLogger::FakeTetherSessionCompletionLogger() =
    default;

FakeTetherSessionCompletionLogger::~FakeTetherSessionCompletionLogger() =
    default;

void FakeTetherSessionCompletionLogger::RecordTetherSessionCompletion(
    const SessionCompletionReason& reason) {
  last_session_completion_reason_ =
      std::make_unique<SessionCompletionReason>(reason);
}

}  // namespace tether

}  // namespace chromeos
