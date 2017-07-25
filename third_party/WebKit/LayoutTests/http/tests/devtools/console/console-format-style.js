// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Tests that console logging dumps properly styled messages.\n');

  await TestRunner.loadModule('console_test_runner');
  await TestRunner.loadPanel('console');

  await TestRunner.evaluateInPagePromise(`
    (function onload()
    {
        console.log("%cBlue!.", "color: blue;");
        console.log("%cBlue! %cRed!", "color: blue;", "color: red;");
        console.log("%cwww.google.com", "color: blue");
    })();
  `);

  ConsoleTestRunner.expandConsoleMessages(onExpanded);

  function onExpanded() {
    ConsoleTestRunner.dumpConsoleMessagesWithStyles();
    TestRunner.completeTest();
  }
})();
