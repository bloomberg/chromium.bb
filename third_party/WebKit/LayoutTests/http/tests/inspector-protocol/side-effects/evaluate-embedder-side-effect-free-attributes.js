(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that evaluating V8-embedder callbacks allows side-effect-free attribute getters. Should not crash.`);

  await session.evaluate(`
    var div = document.createElement('div');
  `);

  await checkHasNoSideEffect('div.isConnected');
  testRunner.completeTest();


  async function checkHasSideEffect(expression) {
    return checkExpression(expression, true);
  }

  async function checkHasNoSideEffect(expression) {
    return checkExpression(expression, false);
  }

  async function checkExpression(expression, expectSideEffect) {
    testRunner.log(`Checking: \`${expression}\` for ${expectSideEffect ? '' : 'no'} side effect`);
    var response = await dp.Runtime.evaluate({expression, throwOnSideEffect: true});
    var hasSideEffect = false;
    var exceptionDetails = response.result.exceptionDetails;
    if (exceptionDetails &&
        exceptionDetails.exception.description.startsWith('EvalError: Possible side-effect in debug-evaluate'))
      hasSideEffect = true;
    if (hasSideEffect !== expectSideEffect) {
      testRunner.log(`FAIL: hasSideEffect = ${hasSideEffect}, expectSideEffect = ${expectSideEffect}`);
      testRunner.completeTest();
      return;
    }
  }
})
