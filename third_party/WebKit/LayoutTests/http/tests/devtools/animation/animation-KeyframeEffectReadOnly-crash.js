// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests that animations can be created with KeyframeEffect and KeyframeEffectReadOnly without crashing.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <p>
      Tests that animations can be created with KeyframeEffect and
      KeyframeEffectReadOnly without crashing.
      </p>

      <div id="node" style="background-color: red; height: 100px"></div>
      <div id="nodeRO" style="background-color: red; height: 100px"></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function startAnimationWithKeyframeEffect()
      {
          var effect = new KeyframeEffect(node, { opacity : [ 0, 0.9 ] }, 1000);
          var anim = node.animate(null);
          anim.effect = effect;
      }

      function startAnimationWithKeyframeEffectReadOnly()
      {
          var effect = new KeyframeEffectReadOnly(nodeRO, { opacity : [ 0, 0.9 ] }, 1000);
          var anim = nodeRO.animate(null);
          anim.effect = effect;
      }
  `);

  await UI.viewManager.showView('animations');
  var timeline = self.runtime.sharedInstance(Animation.AnimationTimeline);
  TestRunner.evaluateInPage('startAnimationWithKeyframeEffect()');
  ElementsTestRunner.waitForAnimationAdded(step2);

  function step2(group) {
    ElementsTestRunner.waitForAnimationAdded(step3);
    TestRunner.evaluateInPage('startAnimationWithKeyframeEffectReadOnly()');
  }

  function step3(group) {
    TestRunner.completeTest();
  }
})();
