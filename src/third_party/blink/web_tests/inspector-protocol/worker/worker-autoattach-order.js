(async function(testRunner) {
  var {page, session, dp} =
      await testRunner.startBlank('Target.setAutoAttach should report all workers before returning.');

  await session.evaluate(`
    const w1 = new Worker('${testRunner.url('../resources/worker-console-worker.js')}');
    const promise1 = new Promise(x => w1.onmessage = x);
    const w2 = new Worker('${testRunner.url('../resources/worker-console-worker.js')}');
    const promise2 = new Promise(x => w2.onmessage = x);
    Promise.all([promise1, promise2]);
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
