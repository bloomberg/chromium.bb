(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      'Tests that tasks order is not changed when worker is resumed.');
  dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true});

  await session.evaluate(`
      const workerScript = \`
        self.count = 0;
        self.onmessage = msg => {
          if (++self.count === 1) {
            debugger;
            console.log("Should be one:", self.count);
            debugger;
          } else {
            (function FAIL_Should_Not_Pause_Here() { debugger; })();
          }
        };
        //# sourceURL=worker.js\`;
      const blob = new Blob([workerScript], { type: "text/javascript" });
      worker = new Worker(URL.createObjectURL(blob));
      worker.postMessage(1);
    `);

  const event = await dp.Target.onceAttachedToTarget();
  const worker = new WorkerProtocol(dp, event.params.sessionId);
  testRunner.log('Worker created');

  await worker.dp.Debugger.enable();

  worker.dp.Runtime.runIfWaitingForDebugger();
  await worker.dp.Debugger.oncePaused();

  await session.evaluate(`worker.postMessage(2)`);
  worker.dp.Debugger.resume();
  await worker.dp.Debugger.oncePaused();
  const result = await worker.dp.Runtime.evaluate({expression: 'self.count'});
  testRunner.log(`count must be 1: ${result.result.value}`);

  await worker.dp.Debugger.disable();
  testRunner.completeTest();
})
