(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that waitForDebuggerOnStart works with out-of-process iframes.`);

  await dp.Target.setAutoAttach({autoAttach: true, waitForDebuggerOnStart: true});
  await dp.Target.setAttachToFrames({value: true});

  await dp.Page.enable();
  session.evaluate(`
    var iframe = document.createElement('iframe');
    iframe.src = 'http://devtools.oopif.test:8000/inspector-protocol/target/resources/test-page.html';
    document.body.appendChild(iframe);
  `);

  var sessionId = (await dp.Target.onceAttachedToTarget()).params.sessionId;
  await dp.Target.sendMessageToTarget({
    sessionId: sessionId,
    message: JSON.stringify({id: 1, method: 'Network.enable'})
  });
  await dp.Target.sendMessageToTarget({
    sessionId: sessionId,
    message: JSON.stringify({id: 2, method: 'Network.setUserAgentOverride', params: {userAgent: 'test'}})
  });
  dp.Target.sendMessageToTarget({
    sessionId: sessionId,
    message: JSON.stringify({id: 3, method: 'Runtime.runIfWaitingForDebugger'})
  });
  dp.Target.onReceivedMessageFromTarget(event => {
    var message = JSON.parse(event.params.message);
    if (message.method === 'Network.requestWillBeSent') {
      testRunner.log('sessionId matches: ' + (sessionId === event.params.sessionId));
      testRunner.log(`User-Agent = ${message.params.request.headers['User-Agent']}`);
      testRunner.completeTest();
    }
  });
})
