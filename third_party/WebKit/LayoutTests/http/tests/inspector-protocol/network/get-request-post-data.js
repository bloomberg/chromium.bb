(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests fetching POST request body.`);

  function SendRequest(method, body) {
    return session.evaluateAsync(`
        new Promise(resolve => {
          const req = new XMLHttpRequest();
          req.open('${method}', '/', true);
          req.setRequestHeader('content-type', 'application/octet-stream');
          req.onreadystatechange = () => resolve();
          req.send(${body});
        });`);
  }

  async function SendAndReportRequest(method, body = '') {
    var requestWillBeSentPromise = dp.Network.onceRequestWillBeSent();
    await SendRequest(method, body);
    var notification = (await requestWillBeSentPromise).params;
    var request = notification.request;
    testRunner.log(`Data included: ${request.postData !== undefined}, has post data: ${request.hasPostData}`);
    var { result, error } = await dp.Network.getRequestPostData({ requestId: notification.requestId });
    if (error) {
      testRunner.log(`Did not fetch data: ${error.message}`);
    }
    else
      testRunner.log(`Data length: ${result.postData.length}`);
  }

  await dp.Network.enable({ maxPostDataSize: 512 });
  await SendAndReportRequest('POST', 'new Uint8Array(1024)');
  await SendAndReportRequest('POST', 'new Uint8Array(128)');
  await SendAndReportRequest('POST', '');
  await SendAndReportRequest('GET', 'new Uint8Array(1024)');
  var result = await dp.Network.getRequestPostData({ requestId: 'fake-id' });
  testRunner.log(`Error is: ${result.error.message}`);
  testRunner.completeTest();
})
