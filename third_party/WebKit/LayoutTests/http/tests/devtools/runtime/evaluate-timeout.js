// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
    await TestRunner.loadModule('sources_test_runner');
    TestRunner.addResult("Test frontend's timeout support.\n");

    const executionContext = UI.context.flavor(SDK.ExecutionContext);
    const regularExpression = '1 + 1';
    const infiniteExpression = 'while (1){}';

    await runtimeTestCase(regularExpression);
    await runtimeTestCase(infiniteExpression);

    let supports = executionContext.runtimeModel.hasSideEffectSupport();
    TestRunner.addResult(`\nDoes the runtime support side effect checks? ${supports}`);
    TestRunner.completeTest();

    async function runtimeTestCase(expression) {
      TestRunner.addResult(`\nTesting expression ${expression} with timeout: 0`);
      const result = await executionContext.evaluate({expression, timeout: 0});
      printDetails(result);
    }

    function printDetails(result) {
      if (result.error) {
        TestRunner.addResult(`Error occurred.`);
      } else if (result.exceptionDetails) {
        let exceptionDescription = result.exceptionDetails.exception.description;
        TestRunner.addResult(`Exception: ${exceptionDescription.split("\n")[0]}`);
      } else if (result.object) {
        let objectDescription = result.object.description;
        TestRunner.addResult(`Result: ${objectDescription}`);
      }
    }
  })();
