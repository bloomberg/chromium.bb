// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests saving nodes to temporary variables.\n`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`<div id="node"></div>`);

  const node = await ElementsTestRunner.nodeWithIdPromise('node');
  ElementsTestRunner.firstElementsTreeOutline()._saveNodeToTempVariable(node);
  const promise = TestRunner.addSnifferPromise(Console.ConsoleViewMessage.prototype, '_formattedParameterAsNodeForTest');
  await ConsoleTestRunner.waitForConsoleMessagesPromise(2);
  const secondMessage = Console.ConsoleView.instance()._visibleViewMessages[1];
  await promise;
  await ConsoleTestRunner.dumpConsoleMessages();

  TestRunner.completeTest();
})();
