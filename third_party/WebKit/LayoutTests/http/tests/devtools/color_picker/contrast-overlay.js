// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that the contrast line algorithm produces good results and terminates.\n`);


  await self.runtime.loadModulePromise('color_picker');
  var colorElement = document.createElement('div');
  var contentElement = document.createElement('div');
  var overlay = new ColorPicker.ContrastOverlay(colorElement, contentElement);
  var svg = colorElement.firstChild;
  TestRunner.assertTrue(svg != null);
  TestRunner.assertEquals(svg.tagName, 'svg');
  var path = svg.firstChild;
  TestRunner.assertTrue(path != null);
  TestRunner.assertEquals(path.tagName, 'path');
  overlay._width = 100;
  overlay._height = 100;

  var colorPairs = [
    // Boring black on white
    {fg: 'black', bg: 'white'},
    // Blue on white - line does not go to RHS
    {fg: 'blue', bg: 'white'},
    // Transparent on white - no possible line
    {fg: 'transparent', bg: 'white'},
    // White on color which previously caused infinite loop
    {fg: 'rgba(255, 255, 255, 1)', bg: 'rgba(157, 83, 95, 1)'}
  ];

  function logLineForColorPair(fgColorString, bgColorString) {
    var contrastInfo = {
      backgroundColors: [bgColorString],
      computedFontSize: '16px',
      computedFontWeight: '400',
      computedBodyFontSize: '16px'
    };
    overlay.setContrastInfo(contrastInfo);
    var fgColor = Common.Color.parse(fgColorString);
    overlay.setColor(fgColor.hsva(), 'rgba(255, 255, 255, 1)');
    overlay._drawContrastRatioLine();
    var d = path.getAttribute('d');
    TestRunner.addResult('');
    TestRunner.addResult(
        'For fgColor ' + fgColorString + ', bgColor ' + bgColorString + ', path was' + (d.length ? '' : ' empty.'));
    if (d.length)
      TestRunner.addResult(path.getAttribute('d'));
  }

  for (var pair of colorPairs)
    logLineForColorPair(pair.fg, pair.bg);

  TestRunner.completeTest();
})();
