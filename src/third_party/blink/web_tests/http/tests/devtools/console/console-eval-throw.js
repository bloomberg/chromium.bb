// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that evaluating 'throw undefined|1|string|object|Error' in the console won't crash the browser and correctly reported. Bug 59611.\n`);

  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');

  async function dumpMessages(next, message) {
    await ConsoleTestRunner.dumpConsoleMessagesIgnoreErrorStackFrames();
    SDK.consoleModel.addEventListener(SDK.ConsoleModel.Events.ConsoleCleared, afterCleared);
    Console.ConsoleView.clearConsole();

    function afterCleared() {
      SDK.consoleModel.removeEventListener(SDK.ConsoleModel.Events.ConsoleCleared, afterCleared);
      next();
    }
  }

  TestRunner.runTestSuite([
    function testThrowUndefined(next) {
      ConsoleTestRunner.evaluateInConsole('throw undefined', dumpMessages.bind(null, next));
    },
    function testThrowNumber(next) {
      ConsoleTestRunner.evaluateInConsole('throw 1', dumpMessages.bind(null, next));
    },
    function testThrowString(next) {
      ConsoleTestRunner.evaluateInConsole('throw \'asdf\'', dumpMessages.bind(null, next));
    },
    function testThrowObject(next) {
      ConsoleTestRunner.evaluateInConsole('throw {a:42}', dumpMessages.bind(null, next));
    },
    function testThrowError(next) {
      ConsoleTestRunner.evaluateInConsole('throw new Error(\'asdf\')', dumpMessages.bind(null, next));
    }
  ]);
})();
