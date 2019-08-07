// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/test/integration/status_change_checker.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/timer/timer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr base::TimeDelta kTimeout = base::TimeDelta::FromSeconds(30);

}  // namespace

StatusChangeChecker::StatusChangeChecker()
    : run_loop_(base::RunLoop::Type::kNestableTasksAllowed),
      timed_out_(false) {}

StatusChangeChecker::~StatusChangeChecker() {}

bool StatusChangeChecker::Wait() {
  if (IsExitConditionSatisfied()) {
    DVLOG(1) << "Already satisfied: " << GetDebugMessage();
  } else {
    DVLOG(1) << "Blocking: " << GetDebugMessage();
    StartBlockingWait();
  }
  return !TimedOut();
}

bool StatusChangeChecker::TimedOut() const {
  return timed_out_;
}

void StatusChangeChecker::StopWaiting() {
  if (run_loop_.running())
    run_loop_.Quit();
}

void StatusChangeChecker::CheckExitCondition() {
  DVLOG(1) << "Await -> Checking Condition: " << GetDebugMessage();
  if (IsExitConditionSatisfied()) {
    DVLOG(1) << "Await -> Condition met: " << GetDebugMessage();
    StopWaiting();
  }
}

void StatusChangeChecker::StartBlockingWait() {
  DCHECK(!run_loop_.running());

  base::OneShotTimer timer;
  timer.Start(FROM_HERE, kTimeout,
              base::BindRepeating(&StatusChangeChecker::OnTimeout,
                                  base::Unretained(this)));

  run_loop_.Run();
}

void StatusChangeChecker::OnTimeout() {
  ADD_FAILURE() << "Await -> Timed out: " << GetDebugMessage();
  timed_out_ = true;
  StopWaiting();
}
