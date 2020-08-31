// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that nested import scripts in worker show correct stack on syntax error.\n`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
      function startWorker()
      {
          var worker = new Worker("resources/importScripts-1.js");
      }
  `);

  ConsoleTestRunner.waitForConsoleMessages(1, step1);
  TestRunner.evaluateInPage('startWorker();');

  async function step1() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
