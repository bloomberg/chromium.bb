// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TABLET_MODE_SCOPED_SKIP_USER_SESSION_BLOCKED_CHECK_H_
#define ASH_WM_TABLET_MODE_SCOPED_SKIP_USER_SESSION_BLOCKED_CHECK_H_

#include "base/macros.h"

namespace ash {

// ScopedSkipUserSessionBlockedCheck allows for skipping checks for if the user
// session is blocked in the event client for a short region of code within a
// scope.
class ScopedSkipUserSessionBlockedCheck {
 public:
  ScopedSkipUserSessionBlockedCheck();
  ~ScopedSkipUserSessionBlockedCheck();

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedSkipUserSessionBlockedCheck);
};

}  // namespace ash

#endif  // ASH_WM_TABLET_MODE_SCOPED_SKIP_USER_SESSION_BLOCKED_CHECK_H_
