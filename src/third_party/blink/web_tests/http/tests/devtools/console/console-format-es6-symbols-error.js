// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Tests that console properly displays information about ES6 Symbols.\n');

  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');

  await TestRunner.evaluateInPagePromise(`
    function logSymbolToConsoleWithError()
    {
        Symbol.prototype.toString = function (arg) { throw new Error(); };
        var smb = Symbol();
        console.log(smb);
    }
  `);

  TestRunner.evaluateInPage('logSymbolToConsoleWithError()', complete);

  async function complete() {
    await ConsoleTestRunner.dumpConsoleMessages();
    TestRunner.completeTest();
  }
})();
