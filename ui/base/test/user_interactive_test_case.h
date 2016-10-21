// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_USER_INTERACTIVE_TEST_CASE_H_
#define UI_BASE_TEST_USER_INTERACTIVE_TEST_CASE_H_

namespace test {

// Foregrounds the test process and spins a run loop to run a test case as
// interactive UI. Call this at the end of test fixtures that are used to
// invoke UI (designed for manual testing of hard-to-summon dialogs). This
// function does not return.
void RunTestInteractively();

}  // namespace test

#endif  // UI_BASE_TEST_USER_INTERACTIVE_TEST_CASE_H_
