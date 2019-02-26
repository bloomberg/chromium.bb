(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests that worker can be paused.');

  await session.evaluate(`
    window.worker = new Worker('${testRunner.url('resources/dedicated-worker.js')}');
    window.worker.onmessage = function(event) { };
    window.worker.postMessage(1);
  `);
  testRunner.log('Started worker');

  dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false});

  let event = await dp.Target.onceAttachedToTarget();
  const worker = new WorkerProtocol(dp, event.params.sessionId);
  testRunner.log('Worker created');
  testRunner.log('didConnectToWorker');

  await worker.dp.Debugger.enable({});
  worker.dp.Debugger.pause({});
  await worker.dp.Debugger.oncePaused();
  testRunner.log('SUCCESS: Worker paused');
  await worker.dp.Debugger.disable({});
  testRunner.completeTest();
})
