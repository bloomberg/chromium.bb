// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that interacting with the console gives appropriate focus.\n`);

  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');

  var consoleView = Console.ConsoleView.instance();
  var viewport = consoleView._viewport;
  var prompt = consoleView._prompt;
  var consoleEditor;
  ConsoleTestRunner.waitUntilConsoleEditorLoaded().then(e => consoleEditor = e).then(logMessages);

  // Ensure that the body is focusable.
  document.body.tabIndex = -1;
  function resetAndDumpFocusAndScrollTop() {
    document.body.focus();
    viewport.element.scrollTop = 0;
    dumpFocusAndScrollInfo();
  }

  function logMessages() {
    ConsoleTestRunner.waitForConsoleMessages(2, () => TestRunner.runTestSuite(testSuite));
    ConsoleTestRunner.evaluateInConsole(
        '\'foo ' +
        '\n'.repeat(50) + 'bar\'');
  }

  var testSuite = [
    function testClickingWithSelectedTextShouldNotFocusPrompt(next) {
      resetAndDumpFocusAndScrollTop();

      // Make a selection.
      var messageElement = consoleView.itemElement(0).element();
      var firstTextNode = messageElement.traverseNextTextNode();
      window.getSelection().setBaseAndExtent(firstTextNode, 0, firstTextNode, 1);

      clickObjectInMessage(0);
      dumpFocusAndScrollInfo();
      window.getSelection().removeAllRanges();
      next();
    },

    function testClickOnMessageShouldFocusPromptWithoutScrolling(next) {
      resetAndDumpFocusAndScrollTop();

      clickObjectInMessage(0);

      dumpFocusAndScrollInfo();
      next();
    },

    function testClickOutsideMessageListShouldFocusPromptAndMoveCaretToEnd(next) {
      prompt.setText('foobar');
      consoleEditor.setSelection(TextUtils.TextRange.createFromLocation(0, 1));
      resetAndDumpFocusAndScrollTop();
      TestRunner.addResult('Selection before: ' + consoleEditor.selection().toString());

      TestRunner.addResult('Clicking on container');
      consoleView._messagesElement.click();

      dumpFocusAndScrollInfo();
      TestRunner.addResult('Selection after: ' + consoleEditor.selection().toString());
      next();
    }
  ];

  function clickObjectInMessage(index) {
    var previewElement = consoleView._visibleViewMessages[index].element().querySelector('.source-code');
    var previewRect = previewElement.getBoundingClientRect();
    var clientX = previewRect.left + previewRect.width / 2;
    var clientY = previewRect.top + previewRect.height / 2;

    TestRunner.addResult('Clicking message ' + index);
    previewElement.dispatchEvent(new MouseEvent('click', {clientX: clientX, clientY: clientY, bubbles: true}));
  }

  function dumpFocusAndScrollInfo() {
    var focusedElement = document.deepActiveElement();
    if (focusedElement)
      TestRunner.addResult('Focused element: ' + focusedElement.tagName);
    else
      TestRunner.addResult('No focus');
    TestRunner.addResult('Viewport scrolled to top: ' + String(viewport.element.scrollTop === 0));
  }
})();
