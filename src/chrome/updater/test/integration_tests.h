// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_TEST_INTEGRATION_TESTS_H_
#define CHROME_UPDATER_TEST_INTEGRATION_TESTS_H_

namespace updater {

namespace test {

// Removes traces of the updater from the system. It is best to run this at the
// start of each test in case a previous crash or timeout on the machine running
// the test left the updater in an installed or partially installed state.
void Clean();

// Expect that the system is in a clean state, i.e. no updater is installed and
// no traces of an updater exist. Should be run at the start and end of each
// test.
void ExpectClean();

// Expect that the updater is installed on the system.
void ExpectInstalled();

// Install the updater.
void Install();

// Expect that the updater is installed on the system and the launchd tasks are
// updated correctly.
void ExpectSwapped();

// Finish up setup.
void Swap();

// Uninstall the updater. If the updater was installed during the test, it
// should be uninstalled before the end of the test to avoid having an actual
// live updater on the machine that ran the test.
void Uninstall();

}  // namespace test

}  // namespace updater

#endif  // CHROME_UPDATER_TEST_INTEGRATION_TESTS_H_
