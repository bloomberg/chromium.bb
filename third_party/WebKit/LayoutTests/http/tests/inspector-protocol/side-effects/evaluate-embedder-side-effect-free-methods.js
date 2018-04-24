(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that evaluating V8-embedder callbacks allows side-effect-free methods. Should not crash.`);

  await session.evaluate(`
    var global_performance = window.performance;

    document.documentElement.setAttribute('xmlns', 'http://www.w3.org/1999/xhtml');

    var div = document.createElement('div');
    div.setAttribute('attr1', 'attr1-value');
    div.setAttribute('attr2', 'attr2-value');
    document.body.appendChild(div);

    var divNamed = document.createElement('div');
    divNamed.setAttribute('name', 'div-name');
    document.body.appendChild(divNamed);

    var spanWithClass = document.createElement('span');
    spanWithClass.className = 'foo bar';
    div.appendChild(spanWithClass);

    var textNode = document.createTextNode('footext');
    var divNoAttrs = document.createElement('div');
  `);

  // Sanity check: test that setters are not allowed on whitelisted methods.
  await checkHasSideEffect(`document.querySelector('div').x = "foo"`);

  // Command Line API
  await checkHasNoSideEffect(`$('div')`);
  await checkHasNoSideEffect(`$$('div')`);
  await checkHasNoSideEffect(`$x('//div')`);
  await checkHasNoSideEffect(`getEventListeners(document)`);
  await checkHasNoSideEffect(`$.toString()`);
  await checkHasNoSideEffect(`$$.toString()`);
  await checkHasNoSideEffect(`$x.toString()`);
  await checkHasNoSideEffect(`getEventListeners.toString()`);

  // Unsafe Command Line API
  await checkHasSideEffect(`monitorEvents()`);
  await checkHasSideEffect(`unmonitorEvents()`);
  await checkHasNoSideEffect(`monitorEvents.toString()`);
  await checkHasNoSideEffect(`unmonitorEvents.toString()`);

  // Document
  await checkHasNoSideEffect(`document.getElementsByTagName('div')`);
  await checkHasNoSideEffect(`document.getElementsByTagNameNS('http://www.w3.org/1999/xhtml', 'div')`);
  await checkHasNoSideEffect(`document.getElementsByClassName('foo')`);
  await checkHasNoSideEffect(`document.getElementsByName('div-name')`);

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
  var testNodes = ['div', 'document', 'textNode'];
  for (var node of testNodes) {
    await checkHasNoSideEffect(`${node}.contains(textNode)`);
    await checkHasNoSideEffect(`${node}.contains()`);
    await checkHasNoSideEffect(`${node}.contains({})`);
    await checkHasNoSideEffect(`${node}.querySelector('div')`);
    await checkHasNoSideEffect(`${node}.querySelectorAll('div')`);
  }

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
    var response = await dp.Runtime.evaluate({expression, throwOnSideEffect: true, includeCommandLineAPI: true});
    var hasSideEffect = false;
    var exceptionDetails = response.result.exceptionDetails;
    if (exceptionDetails &&
        exceptionDetails.exception.description.startsWith('EvalError: Possible side-effect in debug-evaluate'))
      hasSideEffect = true;
    const failed = (hasSideEffect !== expectSideEffect);
    testRunner.log(`${failed ? 'FAIL: ' : ''}Expression \`${expression}\`\nhas side effect: ${hasSideEffect}, expected: ${expectSideEffect}`);
  }
})
