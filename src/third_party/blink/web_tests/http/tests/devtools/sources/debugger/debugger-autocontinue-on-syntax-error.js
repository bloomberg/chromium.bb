// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that debugger won't stop on syntax errors even if "pause on uncaught exceptions" is on.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('sources');

  SourcesTestRunner.startDebuggerTest(step1);

  async function step1() {
    TestRunner.DebuggerAgent.setPauseOnExceptions(SDK.DebuggerModel.PauseOnExceptionsState.PauseOnUncaughtExceptions);
    await TestRunner.addIframe('resources/syntax-error.html');
    await ConsoleTestRunner.dumpConsoleMessages();
    SourcesTestRunner.completeDebuggerTest();
  }
})();
