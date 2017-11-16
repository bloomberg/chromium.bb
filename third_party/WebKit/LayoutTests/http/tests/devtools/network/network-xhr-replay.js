// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests XHR replaying. Bug 95187\n`);
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.showPanel('network');
  await TestRunner.loadHTML(`
      Tests XHR replaying.
      <a href="https://bugs.webkit.org/show_bug.cgi?id=95187">Bug 95187</a>
    `);

  function lastRequest() {
    return NetworkTestRunner.networkRequests().pop();
  }

  function dumpRequest(request) {
    TestRunner.addResult('Dumping request: ');
    TestRunner.addResult('    url: ' + request.url());
    if (request.requestFormData)
      TestRunner.addResult('    requestFormData: ' + request.requestFormData);
    TestRunner.addResult('    requestMethod: ' + request.requestMethod);
    TestRunner.addResult('    test request header value: ' + request.requestHeaderValue('headerName'));
  }

  function assertRequestEqual(request1, request2) {
    TestRunner.assertEquals(request1.url(), request2.url(), 'Requests have different url');
    TestRunner.assertEquals(
        request1.requestFormData, request2.requestFormData, 'Requests have different requestFormData');
    TestRunner.assertEquals(request1.requestMethod, request2.requestMethod, 'Requests have different requestMethod');
    TestRunner.assertEquals(
        request1.requestHeadersText, request2.requestHeadersText, 'Requests have different requestHeadersText');
  }

  async function testXHRReplay(method, url, async, user, password, headers, withCredentials, payload, type, callback) {
    NetworkTestRunner.makeXHR(method, url, async, user, password, headers, withCredentials, payload, type);

    var originalRequest =
        await TestRunner.waitForEvent(NetworkLog.NetworkLog.Events.RequestAdded, NetworkLog.networkLog);
    dumpRequest(originalRequest);
    TestRunner.NetworkAgent.replayXHR(originalRequest.requestId());
    var replayedRequest =
        await TestRunner.waitForEvent(NetworkLog.NetworkLog.Events.RequestAdded, NetworkLog.networkLog);

    assertRequestEqual(originalRequest, replayedRequest);
    callback();
  }


  TestRunner.runTestSuite([
    function testGetStaticAsync(next) {
      testXHRReplay(
          'GET', 'resources/empty.html', true, 'user', 'password', [['headerName', 'headerValueA']], false, undefined,
          undefined, next);
    },

    function testGetStaticSync(next) {
      testXHRReplay(
          'GET', 'resources/empty.html', false, 'user', 'password', [['headerName', 'headerValueB']], false, undefined,
          undefined, next);
    },

    function testGetCachedAsync(next) {
      testXHRReplay(
          'GET', 'resources/random-cached.php', true, 'user', 'password', [['headerName', 'headerValueC']], false,
          undefined, undefined, next);
    },

    function testGetCachedSync(next) {
      testXHRReplay(
          'GET', 'resources/random-cached.php', false, 'user', 'password', [['headerName', 'headerValueD']], false,
          undefined, undefined, next);
    },

    function testGetRandomAsync(next) {
      testXHRReplay(
          'GET', 'resources/random.php', true, 'user', 'password', [['headerName', 'headerValueE']], false, undefined,
          undefined, next);
    },

    function testGetRandomSync(next) {
      testXHRReplay(
          'GET', 'resources/random.php', false, 'user', 'password', [['headerName', 'headerValueF']], false, undefined,
          undefined, next);
    },

    function testPostAsync(next) {
      testXHRReplay(
          'POST', 'resources/random.php', true, 'user', 'password', [['headerName', 'headerValueG']], false, 'payload',
          undefined, next);
    },

    function testPostSync(next) {
      testXHRReplay(
          'POST', 'resources/random.php', false, 'user', 'password', [['headerName', 'headerValueH']], false, 'payload',
          undefined, next);
    }
  ]);
})();
