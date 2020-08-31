// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Test fixture for rect_utils.js.
 */
SelectToSpeakRectUtilsUnitTest = class extends testing.Test {};

/** @override */
SelectToSpeakRectUtilsUnitTest.prototype.extraLibraries = [
  'rect_utils.js',
];


TEST_F('SelectToSpeakRectUtilsUnitTest', 'Overlaps', function() {
  var rect1 = {left: 0, top: 0, width: 100, height: 100};
  var rect2 = {left: 80, top: 0, width: 100, height: 20};
  var rect3 = {left: 0, top: 80, width: 20, height: 100};

  assertTrue(RectUtils.overlaps(rect1, rect1));
  assertTrue(RectUtils.overlaps(rect2, rect2));
  assertTrue(RectUtils.overlaps(rect3, rect3));
  assertTrue(RectUtils.overlaps(rect1, rect2));
  assertTrue(RectUtils.overlaps(rect1, rect3));
  assertFalse(RectUtils.overlaps(rect2, rect3));
});

TEST_F('SelectToSpeakRectUtilsUnitTest', 'RectFromPoints', function() {
  var rect = {left: 10, top: 20, width: 50, height: 60};

  assertNotEquals(
      JSON.stringify(rect),
      JSON.stringify(RectUtils.rectFromPoints(0, 0, 10, 10)));
  assertEquals(
      JSON.stringify(rect),
      JSON.stringify(RectUtils.rectFromPoints(10, 20, 60, 80)));
  assertEquals(
      JSON.stringify(rect),
      JSON.stringify(RectUtils.rectFromPoints(60, 20, 10, 80)));
  assertEquals(
      JSON.stringify(rect),
      JSON.stringify(RectUtils.rectFromPoints(10, 80, 60, 20)));
  assertEquals(
      JSON.stringify(rect),
      JSON.stringify(RectUtils.rectFromPoints(60, 80, 10, 20)));
});
