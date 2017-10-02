// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that the console can preserve log messages across navigations. Bug 53359\n`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.loadHTML(`
      <p>
      Tests that the console can preserve log messages across navigations.
      <a href="https://bugs.webkit.org/show_bug.cgi?id=53359">Bug 53359</a>
      </p>
  `);

  ConsoleModel.consoleModel.addMessage(new ConsoleModel.ConsoleMessage(
      TestRunner.runtimeModel, ConsoleModel.ConsoleMessage.MessageSource.Other,
      ConsoleModel.ConsoleMessage.MessageLevel.Info, 'PASS'));
  Common.settingForTest('preserveConsoleLog').set(true);
  TestRunner.reloadPage(function() {
    ConsoleTestRunner.dumpConsoleMessages();
    Common.settingForTest('preserveConsoleLog').set(false);
    TestRunner.completeTest();
  });
})();
