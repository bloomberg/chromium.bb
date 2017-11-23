// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests overlay layers are not present in the layer tree`);

  await TestRunner.loadModule('layers_test_runner');
  await TestRunner.loadHTML(`
      <style>
      .layer {
          transform: translateZ(10px);
          opacity: 0.8;
      }
      </style>
      <div id="a" style="width: 200px; height: 200px" class="layer">
        CONTENT
      </div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function updateGeometry()
      {
          document.getElementById("a").style.width = "300px";
      }
  `);

  await LayersTestRunner.requestLayers();

  var layersBeforeHighlight = [];
  LayersTestRunner.layerTreeModel().layerTree().forEachLayer(function(layer) {
    layersBeforeHighlight.push(layer.id());
  });
  TestRunner.OverlayAgent.highlightRect(0, 0, 200, 200, {r: 255, g: 0, b: 0});

  await LayersTestRunner.evaluateAndWaitForTreeChange('updateGeometry()');

  var layersAfterHighlight = [];
  LayersTestRunner.layerTreeModel().layerTree().forEachLayer(function(layer) {
    layersAfterHighlight.push(layer.id());
  });

  layersBeforeHighlight.sort();
  layersAfterHighlight.sort();

  TestRunner.assertEquals(JSON.stringify(layersBeforeHighlight), JSON.stringify(layersAfterHighlight));
  TestRunner.addResult('DONE');
  TestRunner.completeTest();
})();
