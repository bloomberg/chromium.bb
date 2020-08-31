// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRASH_CORE_COMMON_BREAKPAD_RUNNING_IOS_H_
#define COMPONENTS_CRASH_CORE_COMMON_BREAKPAD_RUNNING_IOS_H_

namespace crash_reporter {

// Returns true if Breakpad is installed and running.
bool IsBreakpadRunning();

// Sets whether Breakpad is installed and running.
void SetBreakpadRunning(bool running);

}  // namespace crash_reporter

#endif  // COMPONENTS_CRASH_CORE_COMMON_BREAKPAD_RUNNING_IOS_H_
