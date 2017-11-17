// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that LayerTreeModel successfully imports layers from a trace.\n`);
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.loadModule('layers_test_runner');
  await TestRunner.showPanel('timeline');
  await TestRunner.loadHTML(`
      <div id="a" style="width: 200px; height: 200px" class="layer">
          <div class="layer" id="b1" style="width: 150px; height: 100px"></div>
          <div id="b2" class="layer" style="width: 140px; height: 110px">
              <div id="c" class="layer" style="width: 100px; height: 90px"></div>
          </div>
          <div id="b3" class="layer" style="width: 140px; height: 110px"></div>
      </div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function doActions()
      {
          var div = document.createElement("div");
          div.id = "b4";
          document.getElementById("a").appendChild(div);

          // Make sure to force commit, otherwise some owner nodes will be missing.
          var style = document.createElement("style");
          style.textContent = ".layer { transform: translateZ(10px); opacity: 0.8; }";
          document.head.appendChild(style);
          return generateFrames(3);
      }
  `);

  UI.panels.timeline._captureLayersAndPicturesSetting.set(true);

  PerformanceTestRunner.invokeAsyncWithTimeline('doActions', step1);
  function step1() {
    var frames = PerformanceTestRunner.timelineFrameModel().frames();
    var lastFrame = PerformanceTestRunner.timelineFrameModel().frames().peekLast();
    lastFrame.layerTree.layerTreePromise().then(TestRunner.safeWrap(layerTreeResolved));
  }

  function layerTreeResolved(layerTreeModel) {
    LayersTestRunner.dumpLayerTree(undefined, layerTreeModel.contentRoot());
    TestRunner.completeTest();
  }
})();
