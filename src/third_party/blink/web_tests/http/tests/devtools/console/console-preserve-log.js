// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that the console can preserve log messages across navigations. Bug 53359\n`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');

  SDK.consoleModel.addMessage(new SDK.ConsoleMessage(
      TestRunner.runtimeModel, SDK.ConsoleMessage.MessageSource.Other,
      SDK.ConsoleMessage.MessageLevel.Info, 'PASS'));
  Common.settingForTest('preserveConsoleLog').set(true);
  TestRunner.reloadPage(async function() {
    await ConsoleTestRunner.dumpConsoleMessages();
    Common.settingForTest('preserveConsoleLog').set(false);
    TestRunner.completeTest();
  });
})();
