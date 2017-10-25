// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that inspector doesn't force sync layout on operations with CSSOM.Bug 315885.\n`);
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      .test-0 { font-family: 'Arial'; }
      .test-1 { font-family: 'Arial'; }
      .test-2 { font-family: 'Arial'; }
      .test-3 { font-family: 'Arial'; }
      .test-4 { font-family: 'Arial'; }
      .test-5 { font-family: 'Arial'; }
      .test-6 { font-family: 'Arial'; }
      .test-7 { font-family: 'Arial'; }
      .test-8 { font-family: 'Arial'; }
      .test-9 { font-family: 'Arial'; }
      </style>
      <p>
      Tests that inspector doesn't force sync layout on operations with CSSOM.<a href="https://code.google.com/p/chromium/issues/detail?id=315885">Bug 315885</a>.
      </p>
    `);
  await TestRunner.evaluateInPagePromise(`
      function performActions()
      {
          var styleElement = document.querySelector("#testSheet");
          for (var i = 0; i < 10; ++i)
              styleElement.sheet.deleteRule(0);
      }
  `);

  UI.context.setFlavor(Timeline.TimelinePanel, UI.panels.timeline);
  PerformanceTestRunner.evaluateWithTimeline('performActions()', callback);

  function callback() {
    PerformanceTestRunner.timelineModel().mainThreadEvents().forEach(event => {
      if (event.name === TimelineModel.TimelineModel.RecordType.UpdateLayoutTree)
        TestRunner.addResult(event.name);
    });
    TestRunner.completeTest();
  }
})();
