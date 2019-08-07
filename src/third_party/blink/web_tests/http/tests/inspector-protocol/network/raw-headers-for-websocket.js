(async function(testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      'resources/cookie.pl',
      `Tests that cross-domain WebSocket cookies are not reported.`);

  await dp.Network.enable();

  await testURL('ws://127.0.0.1:8000/inspector-protocol/network/resources/cookie.pl');
  await testURL('ws://devtools.oopif.test:8000/inspector-protocol/network/resources/cookie.pl');
  testRunner.completeTest();

  async function testURL(url) {
    session.evaluate(`
      // The WebSocket handshake will fail but it doesn't matter.
      window.ws = new WebSocket('${url}');`);
    const response = await dp.Network.onceWebSocketHandshakeResponseReceived();
    dump(url, response);
  }

  function dump(url, response) {
    response = response.params.response;
    testRunner.log(`\nnew WebSocket('${url}')`);
    testRunner.log(`Set-Cookie: ${response.headers['Set-Cookie']}`);
  }
})
