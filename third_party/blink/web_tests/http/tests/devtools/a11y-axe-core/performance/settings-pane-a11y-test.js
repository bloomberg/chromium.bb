// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Testing a11y in performance panel - settings pane.');

  await TestRunner.loadModule('axe_core_test_runner');
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.showPanel('timeline');

  await PerformanceTestRunner.runPerfTraceWithReload();
  const widget = await PerformanceTestRunner.getTimelineWidget();
  const settingsButton = widget._showSettingsPaneButton.element;
  settingsButton.click();
  await AxeCoreTestRunner.runValidation(widget._settingsPane.contentElement);

  TestRunner.completeTest();
})();
