(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that evaluating V8-embedder callbacks allows side-effect-free methods. Should not crash.`);

  await session.evaluate(`
    var global_performance = window.performance;
    var div = document.createElement('div');
    div.setAttribute('attr1', 'attr1-value');
    div.setAttribute('attr2', 'attr2-value');
    var textNode = document.createTextNode('footext');
    var divNoAttrs = document.createElement('div');
  `);

  // Element
  await checkHasNoSideEffect(`div.getAttributeNames()`);
  await checkHasNoSideEffect(`divNoAttrs.getAttributeNames()`);
  await checkHasNoSideEffect(`div.getAttribute()`);
  await checkHasNoSideEffect(`div.getAttribute('attr1')`);
  await checkHasNoSideEffect(`div.getAttribute({})`);
  await checkHasNoSideEffect(`divNoAttrs.getAttribute('attr1')`);
  await checkHasNoSideEffect(`div.hasAttribute()`);
  await checkHasNoSideEffect(`div.hasAttribute('attr1')`);
  await checkHasNoSideEffect(`div.hasAttribute({})`);
  await checkHasNoSideEffect(`divNoAttrs.hasAttribute('attr1')`);

  // Node
  await checkHasNoSideEffect(`div.contains(textNode)`);
  await checkHasNoSideEffect(`div.contains()`);
  await checkHasNoSideEffect(`div.contains({})`);
  await checkHasNoSideEffect(`textNode.contains(textNode)`);

  // Window
  await checkHasNoSideEffect(`global_performance.now()`);

  testRunner.completeTest();


  async function checkHasSideEffect(expression) {
    return checkExpression(expression, true);
  }

  async function checkHasNoSideEffect(expression) {
    return checkExpression(expression, false);
  }

  async function checkExpression(expression, expectSideEffect) {
    var response = await dp.Runtime.evaluate({expression, throwOnSideEffect: true});
    var hasSideEffect = false;
    var exceptionDetails = response.result.exceptionDetails;
    if (exceptionDetails &&
        exceptionDetails.exception.description.startsWith('EvalError: Possible side-effect in debug-evaluate'))
      hasSideEffect = true;
    if (hasSideEffect !== expectSideEffect) {
      testRunner.log(`FAIL: ${expression} hasSideEffect = ${hasSideEffect}, expectSideEffect = ${expectSideEffect}`);
      testRunner.completeTest();
      return;
    }
  }
})
