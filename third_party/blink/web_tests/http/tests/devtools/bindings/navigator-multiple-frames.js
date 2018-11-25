// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verify that navigator is properly rendered in case of multiple iframes.\n`);
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadModule('bindings_test_runner');

  var sourcesNavigator = new Sources.NetworkNavigatorView();
  sourcesNavigator.show(UI.inspectorView.element);

  TestRunner.markStep('dumpInitialNavigator');
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('createIframes');
  await Promise.all([
    BindingsTestRunner.attachFrame('frame1', './resources/magic-frame.html'),
    BindingsTestRunner.attachFrame('frame2', './resources/magic-frame.html'),
  ]);
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('detachFrame1');
  await BindingsTestRunner.detachFrame('frame1');
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('detachFrame2');
  await BindingsTestRunner.detachFrame('frame2');
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.completeTest();
})();
