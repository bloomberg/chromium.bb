// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verifies viewport stick-to-bottom behavior when Console is opened.\n`);

  // Log a ton of messages before opening console.
  await TestRunner.evaluateInPagePromise(`
      for (var i = 0; i < 150; ++i)
        console.log("Message #" + i);

      //# sourceURL=console-viewport-stick-to-bottom-onload.js
    `);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');
  await ConsoleTestRunner.waitUntilConsoleEditorLoaded();

  var viewport = Console.ConsoleView.instance()._viewport;
  ConsoleTestRunner.waitForConsoleMessagesPromise(150);
  await ConsoleTestRunner.waitForPendingViewportUpdates();

  TestRunner.addResult(
      'Is at bottom: ' + viewport.element.isScrolledToBottom() + ', should stick: ' + viewport.stickToBottom());
  TestRunner.completeTest();
})();
