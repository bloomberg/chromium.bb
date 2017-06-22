(async function(testRunner) {
  let {page, session, dp} = await testRunner.startHTML(`
    <input type='text'></input>
  `, '');

  var dumpAccessibilityNodesBySelectorAndCompleteTest =
      (await testRunner.loadScript('../resources/accessibility-dumpAccessibilityNodes.js'))(testRunner, session);

  var msg = await dp.DOM.getDocument();
  dumpAccessibilityNodesBySelectorAndCompleteTest('head', false, msg);
})
