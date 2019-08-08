// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASH_TEST_UTIL_H_
#define CHROME_BROWSER_UI_ASH_ASH_TEST_UTIL_H_

#include "ash/public/interfaces/shell_test_api.test-mojom-test-utils.h"
#include "ash/public/interfaces/shell_test_api.test-mojom.h"

namespace test {

// Binds to ash service and returns a ShellTestApiPtr.
ash::mojom::ShellTestApiPtr GetShellTestApi();

// Waits until WindowTreeHost no longer holding pointer events.
// If |wait_for_all_changes| is true, this also runs
// aura::test::WaitForAllChangesToComplete().
void WaitForNoPointerHoldLock(bool wait_for_changes = true);

// Waits until the overview animation finishes and its state becomes |state|.
void WaitForOverviewAnimationState(ash::mojom::OverviewAnimationState state);

}  // namespace test

#endif  // CHROME_BROWSER_UI_ASH_ASH_TEST_UTIL_H_
