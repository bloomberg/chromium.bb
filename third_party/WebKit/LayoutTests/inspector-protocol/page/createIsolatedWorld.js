(async function(testRunner) {
  let {page, session, dp} = await testRunner.startBlank('Tests Page.createIsolatedWorld method.');

  var reportedExecutionContextId;
  dp.Runtime.onExecutionContextCreated(message => {
    if (message.params.context.auxData.frameId !== mainFrameId)
      return;
    if (message.params.context.auxData.isDefault === false &&
        message.params.context.name === 'Test world') {
      reportedExecutionContextId = message.params.context.id;
    } else {
      testRunner.log('fail - main world created.');
      testRunner.log(JSON.stringify(message.params));
      testRunner.completeTest();
    }
  });

  await dp.Runtime.enable();
  testRunner.log('Runtime enabled');

  await dp.Page.enable();
  testRunner.log('Page enabled');

  var response = await dp.Page.getResourceTree();
  var mainFrameId = response.result.frameTree.frame.id;
  testRunner.log('Main Frame obtained');

  response = await dp.Page.createIsolatedWorld({frameId: mainFrameId, worldName: 'Test world'});
  if (reportedExecutionContextId === response.result.executionContextId)
    testRunner.log('PASS - execution context id match.');
  else
    testRunner.log('fail - execution context id differ.');
  testRunner.completeTest();
})
