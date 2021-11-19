// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  'use strict';
  TestRunner.addResult(`Tests the gutter decorations in target source code after ScriptFormatterEditorAction\n`);
  await TestRunner.loadLegacyModule('panels/coverage'); await TestRunner.loadTestModule('coverage_test_runner');
  await TestRunner.loadHTML(`
      <p id="id">PASS</p>
    `);
  await TestRunner.addScriptTag('resources/coverage.js');

  await CoverageTestRunner.startCoverage(true);
  await TestRunner.evaluateInPagePromise('performActions()');
  await CoverageTestRunner.stopCoverage();
  await UI.inspectorView.showPanel('sources');
  await CoverageTestRunner.sourceDecorated('coverage.js');

  var decoratorPromise = TestRunner.addSnifferPromise(Coverage.CoverageView.LineDecorator.prototype, 'innerDecorate');
  Sources.ScriptFormatterEditorAction.instance().toggleFormatScriptSource();
  await decoratorPromise;
  CoverageTestRunner.dumpDecorationsInSourceFrame(UI.panels.sources.visibleView);
  TestRunner.completeTest();
})();
