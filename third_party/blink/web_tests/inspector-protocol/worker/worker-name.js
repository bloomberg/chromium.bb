(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`Tests that the worker's name is exposed on its Execution Context.\n`);

  await session.evaluate(`
    worker = new Worker('${testRunner.url('../resources/worker-console-worker.js')}', {
      name: 'the name'
    });
  `);
  let workerCallback;
  const workerPromise = new Promise(x => workerCallback = x);
  dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: false});
  let {params: {sessionId, targetInfo}} = await dp.Target.onceAttachedToTarget();
  testRunner.log(`target title: "${targetInfo.title}"`);
  let wc = new WorkerProtocol(dp, sessionId);
  wc.dp.Runtime.enable({});
  const {context} =  await wc.dp.Runtime.onceExecutionContextCreated();
  testRunner.log(`execution context name: "${context.name}"`);
  testRunner.completeTest();
})
