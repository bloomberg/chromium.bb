(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank('Tests loading and navigation events when submitting a form in an onload handler.');

  dp.Page.onFrameStartedLoading(() => testRunner.log('FrameStartedLoading'));
  dp.Page.onFrameStoppedLoading(() => { testRunner.log('FrameStoppedLoading'); });
  dp.Page.onFrameScheduledNavigation(() => testRunner.log('FrameScheduledNavigation'));
  dp.Page.onFrameClearedScheduledNavigation(() => testRunner.log('FrameClearedScheduledNavigation'));
  await dp.Page.enable();

  dp.Page.navigate({url: testRunner.url('resources/post-in-onload.html')});
  await dp.Page.onceFrameStartedLoading();
  await dp.Page.onceFrameStoppedLoading();
  await dp.Page.onceFrameStartedLoading();
  await dp.Page.onceFrameStoppedLoading();
  testRunner.completeTest();
})
