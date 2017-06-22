(async function(testRunner) {
  let {page, session, dp} = await testRunner.startBlank(
      'Test that profiler doesn\'t crash when we call stop without preceeding start.');
  var messageObject = await dp.Profiler.stop();
  testRunner.expectedError('ProfileAgent.stop', messageObject);
  testRunner.completeTest();
})
