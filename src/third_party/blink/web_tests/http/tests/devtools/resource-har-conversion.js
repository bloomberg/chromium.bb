// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests conversion of Inspector's resource representation into HAR format.\n`);
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.loadModule('application_test_runner');

  await TestRunner.NetworkAgent.setCacheDisabled(true);
  await TestRunner.reloadPagePromise();
  await TestRunner.evaluateInPagePromise(`
      var xhr = new XMLHttpRequest();
      xhr.open("POST", "${TestRunner.url('resources/post-target.cgi')}", false);
      xhr.setRequestHeader("Content-Type", "text/xml");
      xhr.send("<xml></xml>");
  `);

  function findRequestByURL(url) {
    var requests = NetworkTestRunner.networkRequests();
    for (var i = 0; i < requests.length; ++i) {
      if (url.test(requests[i].url()))
        return requests[i];
    }
  }

  function addCookieHeadersToRequest(request) {
    request.setRequestHeaders([{name: 'Cookie', value: 'a=b; $Path=/path; $Domain=example.com; a1=b1\nc1=d1'}]);

    request.responseHeaders = [{
      name: 'Set-Cookie',
      value:
          'x=y; Path=/path; Domain=example.com; Discard; httpOnly; Secure; Version=1\nx1=y1; SameSite=Strict\nz2=y2; SameSite=Lax'
    }];
  }

  addCookieHeadersToRequest(findRequestByURL(/inspected-page\.html$/));
  var log = await SDK.HARLog.build(NetworkTestRunner.networkRequests());
  // Filter out favicon.ico requests that only appear on certain platforms.
  log.entries = log.entries.filter(function(entry) {
    return !/favicon\.ico$/.test(entry.request.url);
  });
  log.entries.sort(ApplicationTestRunner.requestURLComparer);
  TestRunner.addObject(log, NetworkTestRunner.HARPropertyFormattersWithSize);
  var pageTimings = log.pages[0].pageTimings;
  TestRunner.completeTest();
})();
