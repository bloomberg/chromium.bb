(async function(testRunner) {
  var {page, session, dp} = await testRunner.startURL('resources/dom-getFlattenedDocument.html', '');

  await session.evaluate(() => {
    var host = document.querySelector('#shadow-host').createShadowRoot();
    var template = document.querySelector('#shadow-template');
    host.appendChild(template.content);
    template.remove();
  });
  dp.DOM.enable();
  var response = await dp.DOM.getFlattenedDocument({depth: -1, pierce: true});
  testRunner.logMessage(response.result);
  testRunner.completeTest();
})

