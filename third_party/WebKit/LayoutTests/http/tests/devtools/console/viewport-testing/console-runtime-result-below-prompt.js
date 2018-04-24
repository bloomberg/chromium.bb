// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that console fills the empty element below the prompt editor.\n`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');
  await ConsoleTestRunner.waitUntilConsoleEditorLoaded();
  await ConsoleTestRunner.waitForPendingViewportUpdates();

  const consoleView = Console.ConsoleView.instance();
  const prompt = consoleView._prompt;

  TestRunner.runTestSuite([
    async function testUnsafeExpressions(next) {
      await checkExpression(`var should_not_be_defined`);
      await checkExpression(`window.prop_should_not_be_set = 1`);

      await evaluateAndDumpResult(`should_not_be_defined`);
      await evaluateAndDumpResult(`window.prop_should_not_be_set`);

      next();
    },

    async function testSafeExpressions(next) {
      await checkExpression(`1 + 2`);
      await checkExpression(`123`);

      next();
    },

    async function testNoOpForLongText(next) {
      TestRunner.addResult('Setting max length for evaluation to 0');
      const originalMaxLength = Console.ConsolePrompt._MaxLengthForEvaluation;
      Console.ConsolePrompt._MaxLengthForEvaluation = 0;
      await checkExpression(`1 + 2`);
      Console.ConsolePrompt._MaxLengthForEvaluation = originalMaxLength;

      next();
    },
  ]);

  async function checkExpression(expression) {
    prompt.setText(expression);
    await prompt._previewRequestForTest;
    const previewText = prompt._innerPreviewElement.deepTextContent();
    TestRunner.addResult(`Expression: "${expression}" yielded preview: "${previewText}"`);
  }

  async function evaluateAndDumpResult(expression) {
    const customFormatters = {};
    for (let name of ['objectId', 'scriptId', 'exceptionId'])
      customFormatters[name] = 'formatAsTypeNameOrNull';

    TestRunner.addResult(`Evaluating "${expression}"`);
    await TestRunner.evaluateInPage(expression, (value, exceptionDetails) => {
      TestRunner.dump(value, customFormatters, '', 'value: ');
      TestRunner.dump(exceptionDetails, customFormatters, '', 'exceptionDetails: ');
    });
  }
})();
