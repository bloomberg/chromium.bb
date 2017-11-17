// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the Timeline window filter.`);
  TestRunner.addResult(`It applies different ranges to the OverviewGrid and expects that current view reflects the change.\n`);
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.addScriptTag('../resources/timeline-data.js');

  var timeline = UI.panels.timeline;
  var overviewPane = timeline._overviewPane;

  PerformanceTestRunner.loadTimeline(PerformanceTestRunner.timelineData()).then(onTimelineLoaded);

  function onTimelineLoaded() {
    overviewPane._update();
    TestRunner.addResult('OverviewPane:');
    overviewPane._overviewCalculator.setDisplayWidth(450);
    dumpDividers(overviewPane._overviewCalculator);
    TestRunner.addResult('');

    dumpFlameChartRecordsCountForRange(0, 1);
    dumpFlameChartRecordsCountForRange(0.25, 0.75);
    dumpFlameChartRecordsCountForRange(0.33, 0.66);

    overviewPane._overviewGrid.setWindow(0.1, 0.9);

    TestRunner.addResult('--------------------------------------------------------');
    var selectionData = timeline._currentModelSelectionData();
    TestRunner.addResult('time range = ' + selectionData.windowStartTime + ' - ' + selectionData.windowEndTime);
    TestRunner.completeTest();
  }

  function dumpFlameChartRecordsCountForRange(windowLeft, windowRight) {
    var mainView = timeline._currentView._mainFlameChart;
    mainView._muteAnimation = true;
    overviewPane._overviewGrid.setWindow(windowLeft, windowRight);
    mainView.update();
    TestRunner.addResult('range = ' + windowLeft + ' - ' + windowRight);
    var selectionData = timeline._currentModelSelectionData();
    TestRunner.addResult('time range = ' + selectionData.windowStartTime + ' - ' + selectionData.windowEndTime);
    TestRunner.addResult('');
  }

  function dumpDividers(calculator) {
    var times = PerfUI.TimelineGrid.calculateDividerOffsets(calculator)
                    .offsets.map(offset => offset.time - calculator.zeroTime());
    TestRunner.addResult('divider offsets: [' + times.join(', ') + ']. We are expecting round numbers.');
  }
})();
