// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that a URL logged to the console is trimmed down to 150 characters.\n`);

  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.evaluateInPagePromise(`
    //        0---------1---------2---------3---------4---------5---------6---------7---------8---------9---------0---------1---------2---------3---------4---------5---------6---------7---------8---------9---------0
    var url = "http://example.com/2---------3---------4---------5---------6---------7---------8---------9---------0---------1---------2---------3---------4---------5---------6---------7---------8---------9---------0";

    console.log("The URL is: %s", url);
    console.log(url);
  `);

  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();
})();
