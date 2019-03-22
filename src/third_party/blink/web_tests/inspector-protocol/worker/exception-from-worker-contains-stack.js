(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests that console message from worker contains stack trace.');

  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false});

  session.evaluate(`
    window.worker1 = new Worker('${testRunner.url('../resources/worker-with-throw.js')}');
    window.worker1.onerror = function(e) {
      e.preventDefault();
      worker1.terminate();
    }
  `);
  let event = await dp.Target.onceAttachedToTarget();
  const worker1 = new WorkerProtocol(dp, event.params.sessionId);
  testRunner.log('Worker created');
  await worker1.dp.Runtime.enable({});
  session.evaluate('worker1.postMessage(239);');
  await dp.Target.onceDetachedFromTarget();
  testRunner.log('Worker destroyed');

  session.evaluate(`
    window.worker2 = new Worker('${testRunner.url('../resources/worker-with-throw.js')}');
  `);
  event = await dp.Target.onceAttachedToTarget();
  const worker2 = new WorkerProtocol(dp, event.params.sessionId);
  testRunner.log('\nWorker created');
  await worker2.dp.Runtime.enable({});

  session.evaluate('worker2.postMessage(42);');
  event = await worker2.dp.Runtime.onceExceptionThrown();
  const callFrames = event.exceptionDetails.stackTrace ? event.exceptionDetails.stackTrace.callFrames : [];
  testRunner.log(callFrames.length > 0 ? 'Message with stack trace received.' : '[FAIL] Message contains empty stack trace');

  testRunner.completeTest();
})
