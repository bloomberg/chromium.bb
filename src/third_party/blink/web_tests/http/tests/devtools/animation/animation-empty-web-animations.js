// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the empty web animations do not show up in animation timeline.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="node" style="background-color: red; height: 100px"></div>
    `);

  await UI.viewManager.showView('animations');
  var timeline = self.runtime.sharedInstance(Animation.AnimationTimeline);
  TestRunner.evaluateInPage('document.getElementById("node").animate([], { duration: 200, delay: 100 })');
  TestRunner.addSniffer(Animation.AnimationModel.prototype, 'animationStarted', animationStarted);

  function animationStarted() {
    TestRunner.addResult(timeline._previewMap.size);
    TestRunner.completeTest();
  }
})();
