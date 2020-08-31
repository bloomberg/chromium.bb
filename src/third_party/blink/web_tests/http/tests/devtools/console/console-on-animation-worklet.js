// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests console output from AnimationWorklet.\n`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
      function importWorklet()
      {
          CSS.animationWorklet.import('resources/console-worklet-script.js');
      }
  `);

  ConsoleTestRunner.waitForConsoleMessages(4, finish);
  TestRunner.evaluateInPage('importWorklet();');

  async function finish() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
