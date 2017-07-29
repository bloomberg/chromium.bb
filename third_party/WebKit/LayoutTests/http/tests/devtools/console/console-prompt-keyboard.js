// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Tests that console prompt keyboard events work.\n');

  await TestRunner.loadModule('console_test_runner');
  await TestRunner.loadPanel('console');
  await ConsoleTestRunner.waitUntilConsoleEditorLoaded();

  var firstCommand = 'First\nmultiline command';
  var secondCommand = 'Second\nmultiline command';
  TestRunner.addResult('Adding first message: ' + firstCommand);
  await ConsoleTestRunner.evaluateInConsolePromise(firstCommand);

  TestRunner.addResult('Setting prompt text: ' + secondCommand);
  var prompt = Console.ConsoleView.instance()._prompt;
  prompt.setText(secondCommand);

  TestRunner.addResult('\nTest that arrow Up stays in the same command');
  prompt._editor.setSelection(TextUtils.TextRange.createFromLocation(1, 0));
  dumpSelection();
  sendKeyUpToPrompt();
  TestRunner.addResult('Prompt text:' + prompt.text());

  TestRunner.addResult('\nTest that ArrowUp+shift stays in the same command');
  prompt._editor.setSelection(new TextUtils.TextRange(0, 0, 0, 1));
  dumpSelection();
  sendKeyUpToPrompt(true);
  TestRunner.addResult('Prompt text:' + prompt.text());

  TestRunner.addResult('\nTest that arrow Up from the first line loads previous command');
  prompt._editor.setSelection(TextUtils.TextRange.createFromLocation(0, 0));
  dumpSelection();
  sendKeyUpToPrompt();
  TestRunner.addResult('Prompt text:' + prompt.text());


  TestRunner.addResult('\nTest that arrow Down stays in the same command');
  prompt._editor.setSelection(TextUtils.TextRange.createFromLocation(0, 0));
  dumpSelection();
  sendKeyDownToPrompt();
  TestRunner.addResult('Prompt text:' + prompt.text());

  TestRunner.addResult('\nTest that ArrowDown+shift stays in the same command');
  prompt._editor.setSelection(new TextUtils.TextRange(1, 0, 1, 1));
  dumpSelection();
  sendKeyDownToPrompt(true);
  TestRunner.addResult('Prompt text:' + prompt.text());

  TestRunner.addResult('\nTest that arrow Down from the first line loads next command');
  prompt._editor.setSelection(TextUtils.TextRange.createFromLocation(1, 0));
  dumpSelection();
  sendKeyDownToPrompt();
  TestRunner.addResult('Prompt text:' + prompt.text());

  TestRunner.completeTest();

  /**
   * @param {boolean} shiftKey
   */
  function sendKeyUpToPrompt(shiftKey) {
    prompt._editor.element.focus();
    if (shiftKey)
      eventSender.keyDown('ArrowUp', ['shiftKey']);
    else
      eventSender.keyDown('ArrowUp');
  }

  /**
   * @param {boolean} shiftKey
   */
  function sendKeyDownToPrompt(shiftKey) {
    prompt._editor.element.focus();
    if (shiftKey)
      eventSender.keyDown('ArrowDown', ['shiftKey']);
    else
      eventSender.keyDown('ArrowDown');
  }

  function dumpSelection() {
    TestRunner.addResult(JSON.stringify(prompt._editor.selection()));
  }
})();
