// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Verify that navigator is rendered properly when frame with dynamic script and style is added and removed.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadModule('bindings_test_runner');

  var sourcesNavigator = new Sources.NetworkNavigatorView();
  sourcesNavigator.show(UI.inspectorView.element);

  TestRunner.markStep('dumpInitialNavigator');
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('attachFrame');
  await BindingsTestRunner.attachFrame('frame', './resources/dynamic-frame.html');
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('detachFrame');
  await BindingsTestRunner.detachFrame('frame');
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.completeTest();
})();
