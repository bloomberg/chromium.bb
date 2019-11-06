// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that time calculator is updated for both visible and hidden requests.\n`);
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.showPanel('network');

  var target = UI.panels.network._networkLogView;
  target._resourceCategoryFilterUI._toggleTypeFilter(Common.resourceTypes.XHR.category().title, false);
  TestRunner.addResult('Clicked \'' + Common.resourceTypes.XHR.name() + '\' button.');
  target._reset();

  function appendRequest(id, type, startTime, endTime) {
    var request = new SDK.NetworkRequest('', '', '', '', '');
    request.setResourceType(type);
    request.setRequestIdForTest(id);
    request.setIssueTime(startTime);
    request.endTime = endTime;
    TestRunner.networkManager._dispatcher._startNetworkRequest(request);
    target._refresh();

    var isFilteredOut = !!target.nodeForRequest(request)[Network.NetworkLogView._isFilteredOutSymbol];
    TestRunner.addResult('');
    TestRunner.addResult(
        'Appended request [' + request.requestId() + '] of type \'' + request.resourceType().name() +
        '\' is hidden: ' + isFilteredOut + ' from [' + request.startTime + '] to [' + request.endTime + ']');
    TestRunner.addResult(
        'Timeline: from [' + target._calculator.minimumBoundary() + '] to [' + target._calculator.maximumBoundary() +
        ']');
  }

  appendRequest('a', Common.resourceTypes.Script, 1, 2);
  appendRequest('b', Common.resourceTypes.XHR, 3, 4);
  appendRequest('c', Common.resourceTypes.Script, 5, 6);

  TestRunner.completeTest();
})();
