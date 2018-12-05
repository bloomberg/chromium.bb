(async function(testRunner) {
  var {page, session, dp} =
      await testRunner.startBlank('Target.setAutoAttach should report all workers before returning.');

  await session.evaluate(`
    window.w1 = new Worker('${testRunner.url('../resources/worker-with-throw.js')}');
    window.w2 = new Worker('${testRunner.url('../resources/worker-with-throw.js')}');
  `);

  let resolved = false;
  const autoAttach = dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false}).then(() => {
    resolved = true;
  });

  testRunner.log((await dp.Target.onceAttachedToTarget()).params.targetInfo.type);
  testRunner.log((await dp.Target.onceAttachedToTarget()).params.targetInfo.type);

  testRunner.log('Before await. Resolved: ' + resolved);
  await autoAttach;
  testRunner.log('After await. Resolved: ' + resolved);
  testRunner.completeTest();
})
