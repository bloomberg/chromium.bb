(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that evaluating V8-embedder callbacks allows side-effect-free attribute getters. Should not crash.`);

  await session.evaluate(`
    var div = document.createElement('div');
    div.id = 'foo';
    div.className = 'bar baz';
    var textNode = document.createTextNode('footext');
    div.appendChild(textNode);
  `);

  // Sanity check: test that setters are not allowed on whitelisted accessors.
  await checkHasSideEffect(`document.title = "foo"`);

  // Document
  await checkHasNoSideEffect(`document.domain`);
  await checkHasNoSideEffect(`document.referrer`);
  await checkHasNoSideEffect(`document.cookie`);
  await checkHasNoSideEffect(`document.title`);

  // Element
  await checkHasNoSideEffect(`div.tagName`);
  await checkHasNoSideEffect(`div.id`);
  await checkHasNoSideEffect(`div.className`);

  // Node
  var testNodes = ['div', 'document', 'textNode'];
  for (var node of testNodes) {
    await checkHasNoSideEffect(`${node}.nodeType`);
    await checkHasNoSideEffect(`${node}.nodeName`);
    await checkHasNoSideEffect(`${node}.nodeValue`);
    await checkHasNoSideEffect(`${node}.textContent`);
    await checkHasNoSideEffect(`${node}.isConnected`);
  }

  // ParentNode
  await checkHasNoSideEffect(`document.childElementCount`);
  await checkHasNoSideEffect(`div.childElementCount`);

  // Window
  await checkHasNoSideEffect(`devicePixelRatio`);
  await checkHasNoSideEffect(`screenX`);
  await checkHasNoSideEffect(`screenY`);

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
      testRunner.log(`FAIL: "${expression}" hasSideEffect = ${hasSideEffect}, expectSideEffect = ${expectSideEffect}`);
      testRunner.completeTest();
      return;
    }
  }
})
