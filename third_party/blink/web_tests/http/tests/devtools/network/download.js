// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that responseReceived is called on NetworkDispatcher for downloads.\n`);

  await TestRunner.loadModule('browser_sdk');
  await TestRunner.showPanel('network');

  await TestRunner.evaluateInPagePromise(`
    function loadIFrameWithDownload()
    {
        var iframe = document.createElement("iframe");
        iframe.setAttribute("src", "resources/download.zzz");
        document.body.appendChild(iframe);
    };
  `);

  TestRunner.addSniffer(SDK.NetworkDispatcher.prototype, 'responseReceived', responseReceived);
  TestRunner.addSniffer(SDK.NetworkDispatcher.prototype, 'loadingFailed', loadingFailed);
  TestRunner.addSniffer(SDK.NetworkDispatcher.prototype, 'loadingFinished', loadingFinished);
  TestRunner.addIframe('resources/download.zzz');

  function responseReceived(requestId, time, resourceType, response) {
    var request = SDK.networkLog.requestByManagerAndId(TestRunner.networkManager, requestId);

    if (/download\.zzz/.exec(request.url())) {
      TestRunner.addResult('Received response for download.zzz');
      TestRunner.addResult('SUCCESS');
      TestRunner.completeTest();
    }
  }

  function loadingFinished(requestId, finishTime) {
    var request = SDK.networkLog.requestByManagerAndId(TestRunner.networkManager, requestId);

    if (/download\.zzz/.exec(request.url())) TestRunner.completeTest();
  }

  function loadingFailed(requestId, time, localizedDescription, canceled) {
    var request = SDK.networkLog.requestByManagerAndId(TestRunner.networkManager, requestId);

    if (/download\.zzz/.exec(request.url())) TestRunner.completeTest();
  }
})();
