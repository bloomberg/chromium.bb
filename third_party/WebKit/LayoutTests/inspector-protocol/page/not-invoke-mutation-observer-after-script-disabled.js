(async function(testRunner) {
  var {page, session, dp} =
      await testRunner.startHTML(`
    <body><script>
      count = 0;
      var observer = new MutationObserver(() => {count++;});
      observer.observe(document.body, {childList: true});
    </script></body>
`, 'Tests mutation observer invocation.');
  async function triggerMutation() {
    await session.evaluate(`document.body.appendChild(document.createTextNode('ha'))`);
    let result = await session.evaluate(`count`);
    testRunner.log(`Count: ${result}.`);
  }

  await triggerMutation();
  await dp.Emulation.setScriptExecutionDisabled({value: true});
  await triggerMutation();

  testRunner.completeTest();
})
