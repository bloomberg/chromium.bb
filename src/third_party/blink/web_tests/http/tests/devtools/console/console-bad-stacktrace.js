// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Tests that console messages with invalid stacktraces will still be rendered, crbug.com/826210\n');

  await TestRunner.loadModule('console'); await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.showPanel('console');

  var consoleView = Console.ConsoleView.instance();
  consoleView._setImmediatelyFilterMessagesForTest();

  // Add invalid message.
  var badStackTrace = {
    callFrames: [
      {
        'functionName': '',
        'scriptId': 'invalid-ScriptId',
        'url': '',
        'lineNumber': 0,
        'columnNumber': 0,
      }
    ]
  };
  var badStackTraceMessage = new SDK.ConsoleMessage(
      TestRunner.runtimeModel,
      SDK.ConsoleMessage.FrontendMessageSource.ConsoleAPI,
      Protocol.Log.LogEntryLevel.Error, 'This should be visible',
      Protocol.Runtime.ConsoleAPICalledEventType.Error, null, undefined,
      undefined, undefined, badStackTrace);
  SDK.consoleModel.addMessage(badStackTraceMessage);

  await ConsoleTestRunner.dumpConsoleMessages();
  TestRunner.completeTest();
})();
