// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that console revokes lazily handled promise rejections.\n`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
      var worker;

      function createPromise()
      {
          worker = new Worker("resources/worker-with-defer-handled-promise.js");
      }

      function handlePromiseRejection()
      {
          worker.postMessage("");
      }
  `);

  ConsoleModel.consoleModel.addEventListener(
      ConsoleModel.ConsoleModel.Events.MessageAdded, ConsoleTestRunner.wrapListener(messageAdded));
  ConsoleModel.consoleModel.addEventListener(
      ConsoleModel.ConsoleModel.Events.MessageUpdated, ConsoleTestRunner.wrapListener(messageUpdated));

  TestRunner.addResult('Creating worker with promise');
  TestRunner.evaluateInPageWithTimeout('createPromise()');

  async function messageAdded(event) {
    TestRunner.addResult('');
    TestRunner.addResult('Message added: ' + event.data.level + ' ' + event.data.type);

    if (event.data.level === ConsoleModel.ConsoleMessage.MessageLevel.Error) {
      await ConsoleTestRunner.dumpConsoleCounters();
      TestRunner.addResult('');
      TestRunner.addResult('Handling promise');
      TestRunner.evaluateInPageWithTimeout('handlePromiseRejection()');
    }
  }

  async function messageUpdated() {
    await ConsoleTestRunner.dumpConsoleCounters();
    TestRunner.completeTest();
  }
})();
