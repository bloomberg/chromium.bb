// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`This test passes if it does not crash.\n`);
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #node {
          transition: left 100s;
          left: 0px;
      }
      </style>
      <div id="node"></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function startCSSTransition() {
          // Force style recalcs that will trigger a transition.
          getComputedStyle(node).left;
          node.style.left = "100px";
          getComputedStyle(node).left;
      }
  `);

  await UI.viewManager.showView('animations');
  var timeline = self.runtime.sharedInstance(Animation.AnimationTimeline);
  TestRunner.evaluateInPage('startCSSTransition()');
  ElementsTestRunner.waitForAnimationAdded(animationAdded);
  function animationAdded(group) {
    group.animations()[0].setTiming(1, 0);
    TestRunner.completeTest();
  }
})();
