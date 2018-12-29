(async function(testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      'http://localhost:8000/',
      'Verifies that replayed CORS XHRs still have post data');
  await dp.Network.enable();

  const requestsById = {};
  let sentReplayXhr = false;

  function replayOptionsXhr() {
    sentReplayXhr = true;
    const optionsRequestId =
        Object.keys(requestsById)
            .find(id => requestsById[id].method === 'OPTIONS');
    dp.Network.replayXHR({requestId: optionsRequestId});
  }

  function printResultsAndFinish() {
    const requests = Object.values(requestsById)
                         .sort((one, two) => one.wallTime - two.wallTime)
                         .map(request => {
                           delete request.wallTime;
                           return request;
                         });
    for (let i = 0; i < requests.length; i++) {
      testRunner.log(`request ${i}: ${JSON.stringify(requests[i], null, 2)}`);
    }
    testRunner.completeTest();
  }

  dp.Network.onRequestWillBeSent(event => {
    requestsById[event.params.requestId] = {
      method: event.params.request.method,
      url: event.params.request.url,
      postData: event.params.request.postData,
      wallTime: event.params.wallTime
    };
  });

  dp.Network.onLoadingFinished(async event => {
    const requestId = event.params.requestId;
    const responseData =
        await dp.Network.getResponseBody({'requestId': requestId});
    requestsById[requestId].responseData = responseData.result.body;

    if (Object.values(requestsById).every(request => request.responseData)) {
      if (sentReplayXhr)
        printResultsAndFinish();
      else
        replayOptionsXhr();
    }
  });

  await session.evaluate(`
      const xhr = new XMLHttpRequest();
      xhr.open('POST', 'http://127.0.0.1:8000/inspector-protocol/network/resources/cors-return-post.php');
      xhr.setRequestHeader('content-type', 'application/json');
      xhr.send(JSON.stringify({data: 'test post data'}));
  `);
})
