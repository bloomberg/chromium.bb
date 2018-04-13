(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that invoking embedder constructors is side-effect-free. Should not crash.`);

  var constructorNames = await session.evaluate(`
    var windowProps = Object.keys(Object.getOwnPropertyDescriptors(window));
    var constructorNames = windowProps.filter(prop => {
      var value = window[prop];
      return typeof value === 'function' &&
          value.toString().endsWith('{ [native code] }');
    });
    var nativeConstructors = constructorNames.map(name => window[name]);
    constructorNames;
  `);

  // These functions are for testing-only, should not be tested.
  var TestFrameworkBuiltins = new Set(['gc']);

  // These functions are V8 builtins that have not yet been whitelisted.
  // Do not check for test.
  var V8BuiltinsWithoutSideEffect = new Set(['Function', 'Promise', 'RegExp', 'Proxy']);

  // These functions are actually side-effect-free, but fail the V8
  // side-effect check. Methods and attributes marked with [Affects=Nothing]
  // that invoke these as constructors MUST properly handle exceptions.
  var EmbedderCallbacksWithoutSideEffect = new Set(['ReadableStream', 'WritableStream', 'TransformStream']);

  testRunner.log(`Checking functions on 'window'`);
  for (var i = 0; i < constructorNames.length; i++) {
    var name = constructorNames[i];
    if (TestFrameworkBuiltins.has(name) || V8BuiltinsWithoutSideEffect.has(name))
      continue;
    var response = await dp.Runtime.evaluate({expression: `new nativeConstructors[${i}]`, throwOnSideEffect: true});
    var exception = response.result.exceptionDetails;
    var failedSideEffectCheck = exception && exception.exception.description.startsWith('EvalError: Possible side-effect in debug-evaluate');
    if (failedSideEffectCheck && !EmbedderCallbacksWithoutSideEffect.has(name))
      testRunner.log(`Function "${name}" failed side-effect check`);
  }

  testRunner.completeTest();
})
