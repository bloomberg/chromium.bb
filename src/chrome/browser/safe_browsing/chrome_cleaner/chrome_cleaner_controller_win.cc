// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/chrome_cleaner/chrome_cleaner_controller_win.h"

namespace safe_browsing {

using UserResponse = ChromeCleanerController::UserResponse;

std::ostream& operator<<(std::ostream& out,
                         ChromeCleanerController::State state) {
  switch (state) {
    case ChromeCleanerController::State::kIdle:
      return out << "Idle";
    case ChromeCleanerController::State::kReporterRunning:
      return out << "ReporterRunning";
    case ChromeCleanerController::State::kScanning:
      return out << "Scanning";
    case ChromeCleanerController::State::kInfected:
      return out << "Infected";
    case ChromeCleanerController::State::kCleaning:
      return out << "Cleaning";
    case ChromeCleanerController::State::kRebootRequired:
      return out << "RebootRequired";
    default:
      NOTREACHED();
      return out << "UnknownUserResponse" << state;
  }
}

std::ostream& operator<<(std::ostream& out, UserResponse response) {
  switch (response) {
    case UserResponse::kAcceptedWithLogs:
      return out << "UserAcceptedWithLogs";
    case UserResponse::kAcceptedWithoutLogs:
      return out << "UserAcceptedWithoutLogs";
    case UserResponse::kDenied:
      return out << "UserDenied";
    case UserResponse::kDismissed:
      return out << "UserDismissed";
    default:
      NOTREACHED();
      return out << "UnknownUserResponse";
  }
}

}  // namespace safe_browsing
