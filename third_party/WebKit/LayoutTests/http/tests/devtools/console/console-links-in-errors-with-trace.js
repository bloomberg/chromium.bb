// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Test that relative links in traces open in the sources panel.\n');

  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('console');
  await TestRunner.addScriptTag('../resources/source3.js');

  TestRunner.evaluateInPage('foo()', step1);
  UI.inspectorView._tabbedPane.addEventListener(UI.TabbedPane.Events.TabSelected, panelChanged);

  function panelChanged() {
    TestRunner.addResult('Panel ' + UI.inspectorView._tabbedPane._currentTab.id + ' was opened.');
    TestRunner.completeTest();
  }

  var clickTarget;

  function step1() {
    var firstMessageEl = Console.ConsoleView.instance()._visibleViewMessages[0].element();
    clickTarget = firstMessageEl.querySelectorAll('.console-message-text .devtools-link')[1];
    UI.inspectorView.showPanel('console').then(testClickTarget);
  }

  function testClickTarget() {
    TestRunner.addResult('Clicking link ' + clickTarget.textContent);
    clickTarget.click();
  }

  InspectorFrontendHost.openInNewTab = function() {
    TestRunner.addResult('Failure: Open link in new tab!!');
    TestRunner.completeTest();
  };
})();
