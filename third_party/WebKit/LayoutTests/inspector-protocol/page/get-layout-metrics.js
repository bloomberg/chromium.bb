(async function(testRunner) {
  let {page, session, dp} = await testRunner.startHTML(`
    <style>
    body {
        min-height: 2000px;
    }
    </style>
  `, 'Tests that Page.getLayoutMetrics returns reasonable values.');

  session.evaluate('window.scrollTo(0, 200);');
  var message = await dp.Page.getLayoutMetrics();
  if (message.error) {
    testRunner.log(message.error.message);
    testRunner.completeTest();
    return;
  }

  testRunner.logObject(message.result.layoutViewport, 'LayoutViewport: ');
  testRunner.logObject(message.result.visualViewport, 'VisualViewport: ');
  testRunner.completeTest();
})
