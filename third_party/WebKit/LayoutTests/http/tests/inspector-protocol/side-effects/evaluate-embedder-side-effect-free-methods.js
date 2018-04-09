(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that evaluating V8-embedder methods allows side-effect-free methods. Should not crash.`);

  await session.evaluate(`
    var global_performance = window.performance;
  `);

  await checkHasNoSideEffect('global_performance.now()');
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
