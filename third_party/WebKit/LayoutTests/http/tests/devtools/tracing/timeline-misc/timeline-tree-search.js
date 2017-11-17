// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test search in Timeline Tree View panel.\n`);
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.showPanel('timeline');

  var sessionId = '4.20';
  var mainThread = 1;
  var pid = 100;

  var testData = [
    {
      'args': {'sessionId': sessionId},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'TracingStartedInPage',
      'ph': 'I',
      'pid': pid,
      'tid': mainThread,
      'ts': 100,
    },
    {
      'name': 'top level event name',
      'ts': 1000000,
      'ph': 'B',
      'tid': mainThread,
      'pid': pid,
      'cat': 'toplevel',
      'args': {}
    },
    {
      'name': 'TimeStamp',
      'ts': 1010000,
      'ph': 'B',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'bar01'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 1020000,
      'ph': 'I',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'bar02'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 1120000,
      'ph': 'I',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'bar03'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 1180000,
      'ph': 'E',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {}
    },
    {
      'name': 'TimeStamp',
      'ts': 1210000,
      'ph': 'B',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'bar04'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 1220000,
      'ph': 'I',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'foo05'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 1320000,
      'ph': 'I',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'bar06'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 1380000,
      'ph': 'E',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {}
    },
    {
      'name': 'TimeStamp',
      'ts': 1410000,
      'ph': 'B',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'bar07'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 1420000,
      'ph': 'I',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'foo08'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 1520000,
      'ph': 'I',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'foo09'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 1580000,
      'ph': 'E',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {}
    },
    {
      'name': 'top level event name',
      'ts': 1990000,
      'ph': 'E',
      'tid': mainThread,
      'pid': pid,
      'cat': 'toplevel',
      'args': {}
    },
    {
      'name': 'Program',
      'ts': 2000000,
      'ph': 'B',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {}
    },
    {
      'name': 'TimeStamp',
      'ts': 2010000,
      'ph': 'B',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'foo10'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 2020000,
      'ph': 'I',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'bar11'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 2120000,
      'ph': 'I',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'bar12'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 2180000,
      'ph': 'E',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {}
    },
    {
      'name': 'TimeStamp',
      'ts': 2210000,
      'ph': 'B',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'foo13'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 2220000,
      'ph': 'I',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'foo14'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 2320000,
      'ph': 'I',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'bar15'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 2380000,
      'ph': 'E',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {}
    },
    {
      'name': 'TimeStamp',
      'ts': 2410000,
      'ph': 'B',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'foo16'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 2420000,
      'ph': 'I',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'foo17'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 2520000,
      'ph': 'I',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'foo18'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 2580000,
      'ph': 'E',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {}
    },
    {
      'name': 'Program',
      'ts': 2590000,
      'ph': 'E',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {}
    }
  ];

  Runtime.experiments.enableForTest('timelineMultipleMainViews');
  var model = PerformanceTestRunner.createPerformanceModelWithEvents(testData);
  var panel = new Timeline.TimelinePanel();
  panel._setModel(model);
  for (var mode of ['BottomUp', 'CallTree', 'EventLog']) {
    TestRunner.addResult('');
    TestRunner.addResult(`Testing mode: ${mode}`);
    panel._tabbedPane.selectTab(mode);
    var view = panel._searchableView;
    view.showSearchField();
    var valueChangedPromise = TestRunner.addSnifferPromise(view, '_onValueChanged');
    view._searchInputElement.value = 'foo';
    view._searchInputElement.dispatchEvent(new Event('input', {'bubbles': true, 'cancelable': true}));
    await valueChangedPromise;
    var count = view._searchProvider.currentSearchMatches;
    TestRunner.addResult(count);
    for (var i = 0; i < count + 2; ++i) {
      view._onNextButtonSearch();
      var node = panel._currentView._treeView.lastSelectedNode();
      TestRunner.addResult(Timeline.TimelineUIUtils.eventTitle(node.event));
    }
  }
  TestRunner.completeTest();
})();
