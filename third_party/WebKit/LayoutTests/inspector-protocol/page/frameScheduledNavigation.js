(async function(testRunner) {
  let {page, session, dp} = await testRunner.startBlank('');

  dp.Page.enable();
  session.evaluate(`
    var frame = document.createElement('iframe');
    document.body.appendChild(frame);
    frame.src = '${testRunner.url('../resources/blank.html')}';
  `);

  var msg = await dp.Page.onceFrameScheduledNavigation();
  testRunner.log('Scheduled navigation with delay ' + msg.params.delay);

  await dp.Page.onceFrameStartedLoading();
  // This event should be received before the scheduled navigation is cleared.
  testRunner.log('Started loading');

  await dp.Page.onceFrameClearedScheduledNavigation();
  testRunner.log('Cleared scheduled navigation');
  testRunner.completeTest();
})
