// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Test fixture for rect_helper.js.
 * @constructor
 * @extends {testing.Test}
 */
function SwitchAccessRectHelperUnitTest() {
  testing.Test.call(this);
}

SwitchAccessRectHelperUnitTest.prototype = {
  __proto__: testing.Test.prototype,

  /** @override */
  extraLibraries: [
    'rect_helper.js',
  ],
};

TEST_F('SwitchAccessRectHelperUnitTest', 'Equals', function() {
  const rect1 = {left: 0, top: 0, width: 10, height: 10};
  const rect2 = {left: 0, top: 0, width: 10, height: 10};
  const rect3 = {left: 1, top: 0, width: 10, height: 10};
  const rect4 = {left: 0, top: 1, width: 10, height: 10};
  const rect5 = {left: 0, top: 0, width: 11, height: 10};
  const rect6 = {left: 0, top: 0, width: 10, height: 11};

  assertTrue(RectHelper.areEqual(rect1, rect1), 'areEqual should be reflexive');
  assertTrue(
      RectHelper.areEqual(rect1, rect2), 'Rect1 and Rect2 should be equal');
  assertTrue(
      RectHelper.areEqual(rect2, rect1), 'areEqual should be symmetric (1)');
  assertFalse(
      RectHelper.areEqual(rect1, rect3), 'rect1 and rect3 should not be equal');
  assertFalse(
      RectHelper.areEqual(rect3, rect1), 'areEqual should be symmetric (2)');
  assertFalse(
      RectHelper.areEqual(rect1, rect4), 'rect1 and rect4 should not be equal');
  assertFalse(
      RectHelper.areEqual(rect4, rect1), 'areEqual should be symmetric (3)');
  assertFalse(
      RectHelper.areEqual(rect1, rect5), 'rect1 and rect5 should not be equal');
  assertFalse(
      RectHelper.areEqual(rect5, rect1), 'areEqual should be symmetric (4)');
  assertFalse(
      RectHelper.areEqual(rect1, rect6), 'rect1 and rect6 should not be equal');
  assertFalse(
      RectHelper.areEqual(rect6, rect1), 'areEqual should be symmetric (5)');
});

TEST_F('SwitchAccessRectHelperUnitTest', 'Center', function() {
  const rect1 = {left: 0, top: 0, width: 10, height: 10};
  const rect2 = {left: 10, top: 20, width: 10, height: 40};

  const center1 = RectHelper.center(rect1);
  assertEquals(5, center1.x, 'Center1 x should be 5');
  assertEquals(5, center1.y, 'Center1 y should be 5');

  const center2 = RectHelper.center(rect2);
  assertEquals(15, center2.x, 'Center2 x should be 15');
  assertEquals(40, center2.y, 'Center2 y should be 40');
});

TEST_F('SwitchAccessRectHelperUnitTest', 'Union', function() {
  const rect1 = {left: 0, top: 0, width: 10, height: 10};
  const rect2 = {left: 4, top: 4, width: 2, height: 2};
  const rect3 = {left: 10, top: 20, width: 10, height: 40};
  const rect4 = {left: 0, top: 10, width: 10, height: 10};
  const rect5 = {left: 5, top: 5, width: 10, height: 10};

  // When one rect entirely contains the other, that rect is returned.
  const unionRect1Rect2 = RectHelper.union(rect1, rect2);
  assertTrue(
      RectHelper.areEqual(rect1, unionRect1Rect2),
      'Union of rect1 and rect2 should be rect1');

  const unionRect1Rect3 = RectHelper.union(rect1, rect3);
  let expected = {left: 0, top: 0, width: 20, height: 60};
  assertTrue(
      RectHelper.areEqual(expected, unionRect1Rect3),
      'Union of rect1 and rect3 does not match expected value');

  const unionRect1Rect4 = RectHelper.union(rect1, rect4);
  expected = {left: 0, top: 0, width: 10, height: 20};
  assertTrue(
      RectHelper.areEqual(expected, unionRect1Rect4),
      'Union of rect1 and rect4 does not match expected value');

  const unionRect1Rect5 = RectHelper.union(rect1, rect5);
  expected = {left: 0, top: 0, width: 15, height: 15};
  assertTrue(
      RectHelper.areEqual(expected, unionRect1Rect5),
      'Union of rect1 and rect5 does not match expected value');
});

TEST_F('SwitchAccessRectHelperUnitTest', 'UnionAll', function() {
  const rect1 = {left: 0, top: 0, width: 10, height: 10};
  const rect2 = {left: 0, top: 10, width: 10, height: 10};
  const rect3 = {left: 10, top: 0, width: 10, height: 10};
  const rect4 = {left: 10, top: 10, width: 10, height: 10};
  const rect5 = {left: 0, top: 0, width: 100, height: 10};

  const union1 = RectHelper.unionAll([rect1, rect2, rect3, rect4]);
  let expected = {left: 0, top: 0, width: 20, height: 20};
  assertTrue(
      RectHelper.areEqual(expected, union1),
      'Union of rects 1-4 does not match expected value');

  const union2 = RectHelper.unionAll([rect1, rect2, rect3, rect4, rect5]);
  expected = {left: 0, top: 0, width: 100, height: 20};
  assertTrue(
      RectHelper.areEqual(expected, union2),
      'Union of rects 1-5 does not match expected value');
});

TEST_F('SwitchAccessRectHelperUnitTest', 'ExpandToFitWithPadding', function() {
  const padding = 5;
  let inner = {left: 100, top: 100, width: 100, height: 100};
  let outer = {left: 120, top: 120, width: 20, height: 20};
  let expected = {left: 95, top: 95, width: 110, height: 110};
  assertTrue(
      RectHelper.areEqual(
          expected, RectHelper.expandToFitWithPadding(padding, outer, inner)),
      'When outer is contained in inner, expandToFitWithPadding does not ' +
          'match expected value');

  inner = {left: 100, top: 100, width: 100, height: 100};
  outer = {left: 50, top: 50, width: 200, height: 200};
  assertTrue(
      RectHelper.areEqual(
          outer, RectHelper.expandToFitWithPadding(padding, outer, inner)),
      'When outer contains inner, expandToFitWithPadding should equal outer');

  inner = {left: 100, top: 100, width: 100, height: 100};
  outer = {left: 10, top: 10, width: 10, height: 10};
  expected = {left: 10, top: 10, width: 195, height: 195};
  assertTrue(
      RectHelper.areEqual(
          expected, RectHelper.expandToFitWithPadding(padding, outer, inner)),
      'When there is no overlap, expandToFitWithPadding does not match ' +
          'expected value');

  inner = {left: 100, top: 100, width: 100, height: 100};
  outer = {left: 120, top: 50, width: 200, height: 200};
  expected = {left: 95, top: 50, width: 225, height: 200};
  assertTrue(
      RectHelper.areEqual(
          expected, RectHelper.expandToFitWithPadding(padding, outer, inner)),
      'When there is some overlap, expandToFitWithPadding does not match ' +
          'expected value');

  inner = {left: 100, top: 100, width: 100, height: 100};
  outer = {left: 97, top: 95, width: 108, height: 110};
  expected = {left: 95, top: 95, width: 110, height: 110};
  assertTrue(
      RectHelper.areEqual(
          expected, RectHelper.expandToFitWithPadding(padding, outer, inner)),
      'When outer contains inner but without sufficient padding, ' +
          'expandToFitWithPadding does not match expected value');
});

TEST_F('SwitchAccessRectHelperUnitTest', 'Contains', function() {
  const outer = {left: 10, top: 10, width: 10, height: 10};
  assertTrue(RectHelper.contains(outer, outer), 'Rect should contain itself');

  let inner = {left: 10, top: 12, width: 10, height: 5};
  assertTrue(
      RectHelper.contains(outer, inner),
      'Rect should contain rect with same left/right bounds');
  inner = {left: 12, top: 10, width: 5, height: 10};
  assertTrue(
      RectHelper.contains(outer, inner),
      'Rect should contain rect with same top/bottom bounds');
  inner = {left: 12, top: 12, width: 5, height: 5};
  assertTrue(
      RectHelper.contains(outer, inner),
      'Rect should contain rect that is entirely within its bounds');

  inner = {left: 5, top: 12, width: 10, height: 5};
  assertFalse(
      RectHelper.contains(outer, inner),
      'Rect should not contain rect that extends past the left edge');
  inner = {left: 12, top: 8, width: 5, height: 10};
  assertFalse(
      RectHelper.contains(outer, inner),
      'Rect should not contain rect that extends past the top edge');
  inner = {left: 15, top: 5, width: 10, height: 20};
  assertFalse(
      RectHelper.contains(outer, inner),
      'Rect should not contain rect that extends past right edge and has ' +
          'larger height');
  inner = {left: 5, top: 15, width: 10, height: 10};
  assertFalse(
      RectHelper.contains(outer, inner),
      'Rect should not contain rect that extends below and left of it');
  inner = {left: 2, top: 12, width: 5, height: 5};
  assertFalse(
      RectHelper.contains(outer, inner),
      'Rect should not contain rect directly to its left');
  inner = {left: 12, top: 22, width: 5, height: 5};
  assertFalse(
      RectHelper.contains(outer, inner),
      'Rect should not contain rect directly below it');
  inner = {left: 22, top: 2, width: 5, height: 5};
  assertFalse(
      RectHelper.contains(outer, inner),
      'Rect should not contain rect above the the top-right corner');
  inner = {left: 12, top: 2, width: 5, height: 20};
  assertFalse(
      RectHelper.contains(outer, inner),
      'Rect should not contain rect that has a greater height');
  inner = {left: 2, top: 12, width: 20, height: 5};
  assertFalse(
      RectHelper.contains(outer, inner),
      'Rect should not contain rect that has a larger width');
});


TEST_F('SwitchAccessRectHelperUnitTest', 'Difference', function() {
  const outer = {left: 10, top: 10, width: 10, height: 10};
  assertTrue(
      RectHelper.areEqual(
          RectHelper.ZERO_RECT, RectHelper.difference(outer, outer)),
      'Difference of rect with itself should the zero rect');

  let subtrahend = {left: 2, top: 2, width: 5, height: 5};
  assertTrue(
      RectHelper.areEqual(outer, RectHelper.difference(outer, subtrahend)),
      'Difference of non-overlapping rects should be the outer rect');

  subtrahend = {left: 5, top: 5, width: 20, height: 20};
  assertTrue(
      RectHelper.areEqual(
          RectHelper.ZERO_RECT, RectHelper.difference(outer, subtrahend)),
      'Difference where subtrahend contains outer should be the zero rect');

  subtrahend = {left: 12, top: 15, width: 6, height: 3};
  let expected = {left: 10, top: 10, width: 10, height: 5};
  assertTrue(
      RectHelper.areEqual(expected, RectHelper.difference(outer, subtrahend)),
      'Difference above should be largest');

  subtrahend = {left: 15, top: 8, width: 3, height: 10};
  expected = {left: 10, top: 10, width: 5, height: 10};
  assertTrue(
      RectHelper.areEqual(expected, RectHelper.difference(outer, subtrahend)),
      'Difference to the left should be the largest');

  subtrahend = {left: 5, top: 5, width: 13, height: 10};
  expected = {left: 10, top: 15, width: 10, height: 5};
  assertTrue(
      RectHelper.areEqual(expected, RectHelper.difference(outer, subtrahend)),
      'Difference below should be the largest');

  subtrahend = {left: 8, top: 8, width: 10, height: 15};
  expected = {left: 18, top: 10, width: 2, height: 10};
  assertTrue(
      RectHelper.areEqual(expected, RectHelper.difference(outer, subtrahend)),
      'Difference to the right should be the largest');
});

TEST_F('SwitchAccessRectHelperUnitTest', 'Intersection', function() {
  const rect1 = {left: 10, top: 10, width: 10, height: 10};
  assertTrue(
      RectHelper.areEqual(rect1, RectHelper.intersection(rect1, rect1)),
      'Intersection of a rectangle with itself should be itself');

  let rect2 = {left: 12, top: 12, width: 5, height: 5};
  assertTrue(
      RectHelper.areEqual(rect2, RectHelper.intersection(rect1, rect2)),
      'When one rect contains another, intersection should be the inner rect');
  assertTrue(
      RectHelper.areEqual(rect2, RectHelper.intersection(rect2, rect1)),
      'Intersection should be symmetric');

  rect2 = {left: 5, top: 5, width: 20, height: 20};
  assertTrue(
      RectHelper.areEqual(rect1, RectHelper.intersection(rect1, rect2)),
      'When one rect contains another, intersection should be the inner rect');
  assertTrue(
      RectHelper.areEqual(rect1, RectHelper.intersection(rect2, rect1)),
      'Intersection should be symmetric');

  rect2 = {left: 30, top: 10, width: 10, height: 10};
  assertTrue(
      RectHelper.areEqual(
          RectHelper.ZERO_RECT, RectHelper.intersection(rect1, rect2)),
      'Intersection of non-overlapping rects should be zero rect');
  assertTrue(
      RectHelper.areEqual(
          RectHelper.ZERO_RECT, RectHelper.intersection(rect2, rect1)),
      'Intersection should be symmetric');

  rect2 = {left: 15, top: 10, width: 10, height: 10};
  let expected = {left: 15, top: 10, width: 5, height: 10};
  assertTrue(
      RectHelper.areEqual(expected, RectHelper.intersection(rect1, rect2)),
      'Side-by-side overlap is not computed correctly');
  assertTrue(
      RectHelper.areEqual(expected, RectHelper.intersection(rect2, rect1)),
      'Intersection should be symmetric');

  rect2 = {left: 15, top: 5, width: 10, height: 10};
  expected = {left: 15, top: 10, width: 5, height: 5};
  assertTrue(
      RectHelper.areEqual(expected, RectHelper.intersection(rect1, rect2)),
      'Top corner overlap is not computed correctly');
  assertTrue(
      RectHelper.areEqual(expected, RectHelper.intersection(rect2, rect1)),
      'Intersection should be symmetric');

  rect2 = {left: 5, top: 15, width: 20, height: 10};
  expected = {left: 10, top: 15, width: 10, height: 5};
  assertTrue(
      RectHelper.areEqual(expected, RectHelper.intersection(rect1, rect2)),
      'Bottom overlap is not computed correctly');
  assertTrue(
      RectHelper.areEqual(expected, RectHelper.intersection(rect2, rect1)),
      'Intersection should be symmetric');
});
