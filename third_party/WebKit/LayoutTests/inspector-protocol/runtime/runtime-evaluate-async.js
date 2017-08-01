(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`Tests that Runtime.evaluate works with awaitPromise flag.`);

  function dumpResult(result) {
    if (result.exceptionDetails) {
      result.exceptionDetails.scriptId = '';
      result.exceptionDetails.exceptionId = 0;
      result.exceptionDetails.exception.objectId = 0;
    }
    testRunner.logObject(result);
  }

  await testRunner.runTestSuite([
    async function testResolvedPromise() {
      var result = await dp.Runtime.evaluate({ expression: 'Promise.resolve(239)', awaitPromise: true, generatePreview: true });
      dumpResult(result.result);
    },

    async function testRejectedPromise() {
      var result = await dp.Runtime.evaluate({ expression: 'Promise.reject(239)', awaitPromise: true });
      dumpResult(result.result);
    },

    async function testPrimitiveValueInsteadOfPromise() {
      var result = await dp.Runtime.evaluate({ expression: 'true', awaitPromise: true });
      testRunner.logObject(result.error);
    },

    async function testObjectInsteadOfPromise() {
      var result = await dp.Runtime.evaluate({ expression: '({})', awaitPromise: true });
      testRunner.logObject(result.error);
    },

    async function testPendingPromise() {
      var result = await dp.Runtime.evaluate({ expression: `
        new Promise(resolve => setTimeout(() => resolve({ a : 239 }), 0))
      `, awaitPromise: true, returnByValue: true });
      dumpResult(result.result);
    },

    async function testExceptionInEvaluate() {
      var result = await dp.Runtime.evaluate({ expression: 'throw 239', awaitPromise: true });
      dumpResult(result.result);
    }
  ]);
})
