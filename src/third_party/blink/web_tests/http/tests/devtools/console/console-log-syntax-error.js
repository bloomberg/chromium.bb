// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that syntax errors are logged into console and doesn't cause browser crash.\n`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.addScriptTag('resources/syntax-error.js');

  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();
})();
