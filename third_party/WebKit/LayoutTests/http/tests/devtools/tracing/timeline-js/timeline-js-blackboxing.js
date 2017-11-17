// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests JS blackboxing for timeline\n`);
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.showPanel('timeline');

  var sessionId = '6.23';
  var rawTraceEvents = [
    {
      'args': {'name': 'Renderer'},
      'cat': '__metadata',
      'name': 'process_name',
      'ph': 'M',
      'pid': 17851,
      'tid': 23,
      'ts': 0
    },
    {
      'args': {'name': 'CrRendererMain'},
      'cat': '__metadata',
      'name': 'thread_name',
      'ph': 'M',
      'pid': 17851,
      'tid': 23,
      'ts': 0
    },
    {
      'args': {'sessionId': sessionId},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'TracingStartedInPage',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 100000,
      'tts': 606543
    },
    {
      'args': {},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'Program',
      'ph': 'B',
      'pid': 17851,
      'tid': 23,
      'ts': 200000,
      'tts': 5612442
    },
    {
      'args': {'data': {'functionName': 'level1', 'url': 'user_script.js'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSFrame',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 208000,
      'dur': 10000,
      'tts': 1758056
    },
    {
      'args': {'data': {'functionName': 'level2', 'url': 'user_script.js'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSFrame',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 208000,
      'dur': 10000,
      'tts': 1758056
    },
    {
      'args': {'data': {'functionName': 'level3', 'url': 'user_script.js'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSFrame',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 209000,
      'dur': 1000,
      'tts': 1758056
    },
    {
      'args': {'data': {'functionName': 'level3blackboxed', 'url': 'lib_script.js'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSFrame',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 211000,
      'dur': 6000,
      'tts': 1758056
    },
    {
      'args': {'data': {'functionName': 'level4user', 'url': 'user_script.js'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSFrame',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 211000,
      'dur': 1000,
      'tts': 1758056
    },
    {
      'args': {'data': {'functionName': 'level4blackboxed', 'url': 'lib_script.js'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSFrame',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 213000,
      'dur': 3000,
      'tts': 1758056
    },
    {
      'args': {'data': {'functionName': 'level5blackboxed', 'url': 'lib_script.js'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSFrame',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 213000,
      'dur': 3000,
      'tts': 1758056
    },
    {
      'args': {'data': {'functionName': 'level6user', 'url': 'user_script.js'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSFrame',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 213000,
      'dur': 3000,
      'tts': 1758056
    },
    {
      'args': {'data': {'functionName': 'level7blackboxed', 'url': 'lib_script.js'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSFrame',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 213000,
      'dur': 3000,
      'tts': 1758056
    },
    {
      'args': {'data': {'functionName': 'level8user', 'url': 'user_script.js'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSFrame',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 213000,
      'dur': 3000,
      'tts': 1758056
    },
    {
      'args': {'data': {'functionName': 'level1blackboxed', 'url': 'lib_script.js'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSFrame',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 230000,
      'dur': 3000,
      'tts': 1758056
    },
    {
      'args': {'data': {'functionName': 'level1blackboxed', 'url': 'lib_script.js'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSFrame',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 235000,
      'dur': 3000,
      'tts': 1758056
    },
    {
      'args': {'data': {'functionName': 'level2blackboxed', 'url': 'lib_script.js'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSFrame',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 236000,
      'dur': 1000,
      'tts': 1758056
    },
    {
      'args': {},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'Program',
      'ph': 'E',
      'pid': 17851,
      'tid': 23,
      'ts': 500000,
      'tts': 5612506
    }
  ];

  function printTimelineData(dataProvider) {
    dataProvider.reset();
    var timelineData = dataProvider.timelineData();
    for (var i = 0; i < timelineData.entryStartTimes.length; ++i) {
      var name = dataProvider.entryTitle(i);
      var padding = '  '.repeat(timelineData.entryLevels[i] - 1);
      TestRunner.addResult(
          padding + name + ': ' + timelineData.entryTotalTimes[i] + ' @ ' + timelineData.entryStartTimes[i]);
    }
  }

  Runtime.experiments.enableForTest('blackboxJSFramesOnTimeline');
  var dataProvider = new Timeline.TimelineFlameChartDataProvider(UI.panels.timeline._filters);
  dataProvider.setModel(PerformanceTestRunner.createPerformanceModelWithEvents(rawTraceEvents));

  TestRunner.addResult('\nBlackboxed url: lib_script.js');
  Bindings.blackboxManager._blackboxURL('lib_script.js');
  printTimelineData(dataProvider);

  TestRunner.addResult('\nUnblackboxed url: lib_script.js');
  Bindings.blackboxManager._unblackboxURL('lib_script.js');
  printTimelineData(dataProvider);
  TestRunner.completeTest();
})();
