// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shutdown_reason.h"

#include "base/logging.h"

namespace ash {

const char* ShutdownReasonToString(ShutdownReason reason) {
  switch (reason) {
    case ShutdownReason::POWER_BUTTON:
      return "power button";
    case ShutdownReason::LOGIN_SHUT_DOWN_BUTTON:
      return "login shut down button";
    case ShutdownReason::TRAY_SHUT_DOWN_BUTTON:
      return "tray shut down button";
  }
  NOTREACHED() << "Invalid reason " << static_cast<int>(reason);
  return "invalid";
}

}  // namespace ash
