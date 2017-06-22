(async function(testRunner) {
  let {page, session, dp} = await testRunner.startHTML(`
    <div id='node' style='background-color: red; height: 100px'></div>
  `, '');

  dp.Animation.enable();
  session.evaluate(`node.animate([{ width: '100px' }, { width: '200px' }], 2000);`);
  await dp.Animation.onceAnimationCreated();
  testRunner.log('Animation created');
  await dp.Animation.onceAnimationStarted();
  testRunner.log('Animation started');
  testRunner.completeTest();
})
