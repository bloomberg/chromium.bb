// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function () {
  await TestRunner.loadModule('axe_core_test_runner');
  await TestRunner.showPanel('sources');

  await UI.viewManager.showView('sources.eventListenerBreakpoints');
  const eventListenerWidget = self.runtime.sharedInstance(
      BrowserDebugger.EventListenerBreakpointsSidebarPane);
  TestRunner.addResult('Setting event listener breakpoints.');
  const {checkbox, element} = eventListenerWidget._categories.get('Animation');
  element.revealAndSelect();
  checkbox.click();

  TestRunner.addResult('Dumping Animation category.');
  TestRunner.addResult(element.listItemElement.deepTextContent());
  TestRunner.addResult('Running the axe-core linter on the Animation category of the event listener breakpoints pane.');
  await AxeCoreTestRunner.runValidation(element.listItemElement);

  TestRunner.completeTest();
})();
