// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function() {
  TestRunner.addResult('Tests the transfer size of signed exchange is set correctly.\n');
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('network');
  SDK.networkLog.reset();
  await TestRunner.addIframe('/loading/sxg/resources/sxg-larger-than-10k.sxg');
  await ConsoleTestRunner.dumpConsoleMessages();
  NetworkTestRunner.dumpNetworkRequestsWithSignedExchangeInfo();
  var requests = NetworkTestRunner.findRequestsByURLPattern(/sxg-larger-than-10k.sxg/);
  TestRunner.assertTrue(requests.length === 1);
  TestRunner.assertTrue(requests[0].transferSize > 10000);
  TestRunner.completeTest();
})();
