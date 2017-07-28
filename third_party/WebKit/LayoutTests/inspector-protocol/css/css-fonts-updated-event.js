(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`<link rel='stylesheet'>some text`, 'Verifies that CSS.fontsUpdated event is sent as the web font is loaded.');

  await dp.DOM.enable();
  await dp.CSS.enable();
  session.evaluate(fontURL => {
    var link = document.querySelector('link');
    link.href = fontURL;
  }, testRunner.url('./resources/ahem.css'));
  await dp.CSS.onceFontsUpdated();
  testRunner.log('SUCCESS: CSS.FontsUpdated event received.');
  testRunner.completeTest();
});
