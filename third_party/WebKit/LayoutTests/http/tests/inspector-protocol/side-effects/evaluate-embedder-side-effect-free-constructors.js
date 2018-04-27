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

  // These constructor callbacks are blacklisted with `[Affects=Everything]`,
  // and should throwOnSideEffect.
  var EmbedderConstructorBlacklist = new Set([
    // The Worker constructor may run a worker in parallel, fetch URLs, and
    // modify the global worker set. See spec section 10.2.6.3, step 9.
    // https://html.spec.whatwg.org/#dedicated-workers-and-the-worker-interface
    'Worker',

    // The SharedWorker constructor may run a worker in parallel, fetch URLs,
    // and modify the global worker set. See spec section 10.2.6.4, step 11.
    // https://html.spec.whatwg.org/#shared-workers-and-the-sharedworker-interface
    'SharedWorker'
  ]);

  testRunner.log(`Checking functions on 'window'`);
  for (var i = 0; i < constructorNames.length; i++) {
    var name = constructorNames[i];
    if (TestFrameworkBuiltins.has(name) || V8BuiltinsWithoutSideEffect.has(name))
      continue;
    var response = await dp.Runtime.evaluate({expression: `new nativeConstructors[${i}]`, throwOnSideEffect: true});
    var exception = response.result.exceptionDetails;
    var failedSideEffectCheck = exception && exception.exception.description.startsWith('EvalError: Possible side-effect in debug-evaluate');
    var expectedToThrow = EmbedderConstructorBlacklist.has(name);
    if (failedSideEffectCheck && !EmbedderCallbacksWithoutSideEffect.has(name))
      testRunner.log(`${expectedToThrow ? '' : 'FAIL: '}Function "${name}" failed side-effect check`);
  }

  testRunner.completeTest();
})
