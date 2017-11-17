// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the Timeline API instrumentation of a network resource load\n`);
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.evaluateInPagePromise(`
      var scriptUrl = "../resources/timeline-network-resource.js";

      function performActions()
      {
          var promise = new Promise((fulfill) => window.timelineNetworkResourceEvaluated = fulfill);
          var script = document.createElement("script");
          script.src = scriptUrl;
          document.body.appendChild(script);
          return promise;
      }
  `);

  await TestRunner.NetworkAgent.setCacheDisabled(true);
  var requestId;
  var scriptUrl = 'timeline-network-resource.js';

  PerformanceTestRunner.invokeAsyncWithTimeline('performActions', finish);

  function finish() {
    var model = PerformanceTestRunner.timelineModel();
    model.mainThreadEvents().forEach(event => {
      if (event.name === TimelineModel.TimelineModel.RecordType.ResourceSendRequest)
        printSend(event);
      else if (event.name === TimelineModel.TimelineModel.RecordType.ResourceReceiveResponse)
        printReceive(event);
      else if (event.name === TimelineModel.TimelineModel.RecordType.ResourceFinish)
        printFinish(event);
    });
    TestRunner.completeTest();
  }

  function printEvent(event) {
    TestRunner.addResult('');
    PerformanceTestRunner.printTraceEventProperties(event);
    TestRunner.addResult(
        `Text details for ${event.name}: ` + Timeline.TimelineUIUtils.buildDetailsTextForTraceEvent(event));
  }

  function printSend(event) {
    printEvent(event);
    var data = event.args['data'];
    requestId = data.requestId;
    if (data.url === undefined)
      TestRunner.addResult('* No \'url\' property in record');
    else if (data.url.indexOf(scriptUrl) === -1)
      TestRunner.addResult('* Didn\'t find URL: ' + scriptUrl);
  }

  function printReceive(event) {
    printEvent(event);
    var data = event.args['data'];
    if (requestId !== data.requestId)
      TestRunner.addResult('Didn\'t find matching requestId: ' + requestId);
    if (data.statusCode !== 0)
      TestRunner.addResult('Response received status: ' + data.statusCode);
  }

  function printFinish(event) {
    printEvent(event);
    var data = event.args['data'];
    if (requestId !== data.requestId)
      TestRunner.addResult('Didn\'t find matching requestId: ' + requestId);
    if (data.didFail)
      TestRunner.addResult('Request failed.');
  }
})();
