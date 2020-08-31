// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
    `Tests that message from script inside svg has correct source location.\n`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.navigatePromise('resources/svg.html');
  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();
})();
