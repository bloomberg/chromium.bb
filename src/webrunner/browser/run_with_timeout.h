// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBRUNNER_BROWSER_RUN_WITH_TIMEOUT_H_
#define WEBRUNNER_BROWSER_RUN_WITH_TIMEOUT_H_

#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"

namespace webrunner {

// Runs |run_loop| and logs a test failure if it fails to finish before
// |timeout|.
void CheckRunWithTimeout(
    base::RunLoop* run_loop,
    const base::TimeDelta& timeout = TestTimeouts::action_timeout());

}  // namespace webrunner

#endif  // WEBRUNNER_BROWSER_RUN_WITH_TIMEOUT_H_
