(async function(testRunner) {
  var {page, session, dp} = await testRunner.startHTML(`
    <div id='depth-1'>
        <div id='depth-2'>
            <div id='depth-3'>
                <iframe src='../dom/resources/shadow-dom-iframe.html'></iframe>
            </div>
        </div>
        <div id='targetDiv'></div>
    </div>
  `, '');
  var response = await dp.DOM.getDocument({depth: -1});
  var iframeOwner = response.result.root.children[0].children[1].children[0].children[0].children[0].children[0];
  if (iframeOwner.contentDocument.children) {
    testRunner.die('Error IFrame node should not include children: ' + JSON.stringify(iframeOwner, null, '    '));
    return;
  }

  var response = await dp.DOM.getDocument({depth: -1, pierce: true});
  testRunner.logMessage(response);
  testRunner.completeTest();
})

