// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that console artifacts can be expanded, collapsed via keyboard.\n`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');
  ConsoleTestRunner.fixConsoleViewportDimensions(600, 200);
  await ConsoleTestRunner.waitUntilConsoleEditorLoaded();

  const consoleView = Console.ConsoleView.instance();
  const viewport = consoleView._viewport;
  const prompt = consoleView._prompt;

  TestRunner.runTestSuite([
    async function testExpandingTraces(next) {
      await clearAndLog(`console.warn("warning")`);
      forceSelect(0);

      dumpFocus();
      press('ArrowRight');
      dumpFocus();
      press('ArrowLeft');
      dumpFocus();

      next();
    },

    async function testExpandingGroups(next) {
      await clearAndLog(`console.group("group"); console.log("log child");`, 2);
      forceSelect(0);

      dumpFocus();
      ConsoleTestRunner.dumpConsoleMessages();
      press('ArrowLeft');
      dumpFocus();
      ConsoleTestRunner.dumpConsoleMessages();
      press('ArrowRight');
      dumpFocus();
      ConsoleTestRunner.dumpConsoleMessages();

      next();
    },
  ]);


  // Utilities.
  async function clearAndLog(expression, expectedCount = 1) {
    consoleView._consoleCleared();
    TestRunner.addResult(`Evaluating: ${expression}`);
    await TestRunner.evaluateInPagePromise(expression);
    await ConsoleTestRunner.waitForConsoleMessagesPromise(expectedCount);
    await ConsoleTestRunner.waitForPendingViewportUpdates();
  }

  function forceSelect(index) {
    TestRunner.addResult(`\nForce selecting index ${index}`);
    viewport._virtualSelectedIndex = index;
    viewport._contentElement.focus();
    viewport._updateFocusedItem();
  }

  function press(key) {
    TestRunner.addResult(`\n${key}:`);
    eventSender.keyDown(key);
  }

  function dumpFocus() {
    const firstMessage = consoleView._visibleViewMessages[0];
    const hasTrace = !!firstMessage.element().querySelector('.console-message-stack-trace-toggle');
    const hasHiddenStackTrace = firstMessage.element().querySelector('.console-message-stack-trace-wrapper > div.hidden');
    const hasCollapsedObject = firstMessage.element().querySelector('.console-view-object-properties-section.hidden');
    const hasExpandedObject = firstMessage.element().querySelector('.console-view-object-properties-section:not(.hidden)');

    TestRunner.addResult(`Viewport virtual selection: ${viewport._virtualSelectedIndex}`);

    if (hasCollapsedObject) {
      TestRunner.addResult(`Has object: collapsed`);
    } else if (hasExpandedObject) {
      TestRunner.addResult(`Has object: expanded`);
    }

    if (hasTrace) {
      TestRunner.addResult(`Is trace expanded: ${!hasHiddenStackTrace ? 'YES' : 'NO'}`);
    }
    if (firstMessage instanceof Console.ConsoleGroupViewMessage) {
      const expanded = !firstMessage.collapsed();
      TestRunner.addResult(`Is group expanded: ${expanded ? 'YES' : 'NO'}`);
    }
  }
})();
