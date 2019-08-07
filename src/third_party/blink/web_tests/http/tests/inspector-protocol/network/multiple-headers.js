 (async function(testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      '../resources/test-page.html',
      `Tests that multiple HTTP headers with same name are correctly folded into one LF-separated line.`);

  const url = 'http://127.0.0.1:8000/inspector-protocol/network/resources/multiple-headers.php';
  await dp.Network.enable();
  await dp.Page.enable();

  session.evaluate(`fetch("${url}?fetch=1").then(r => r.text())`);
  const fetchResponse = (await dp.Network.onceResponseReceived()).params.response;
  testRunner.log(`Pragma header of fetch of ${fetchResponse.url}: ${fetchResponse.headers['Access-Control-Pragma']}`);
  await dp.Network.onceLoadingFinished();

  session.evaluate(`
    var f = document.createElement('frame');
    f.src = "${url}";
    document.body.appendChild(f);
  `);
  const navigationResponse = (await dp.Network.onceResponseReceived()).params.response;
  testRunner.log(`Pragma header of navigation to ${navigationResponse.url}: ${navigationResponse.headers['Access-Control-Pragma']}`);
  testRunner.completeTest();
})
