// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/idle/idle.h"

#include "base/logging.h"

namespace ui {

int CalculateIdleTime() {
  // TODO(crbug.com/878979): implementation pending.
  NOTIMPLEMENTED();
  return 0;
}

bool CheckIdleStateIsLocked() {
  // TODO(crbug.com/878979): implementation pending.
  NOTIMPLEMENTED();
  return false;
}

IdleState CalculateIdleState(int idle_threshold) {
  // TODO(crbug.com/878979): implementation pending.
  NOTIMPLEMENTED();
  return IdleState::IDLE_STATE_UNKNOWN;
}

}  // namespace ui
