// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This function returns the first element index that >= target, when no element
 * in the array >= target, return array.length.
 * This function must be called in the shape of binarySearch(array, target).
 * @param {number} target
 * @return {number}
 */
var binarySearch = function(target) {
  'use strict';

  var left = 0;
  var right = this.length - 1;
  while (left <= right) {
    var mid = Math.floor((left + right) / 2);
    if (this[mid] < target)
      left = mid + 1;
    else if (this[mid] > target)
      right = mid - 1;
    else
      return mid;
  }
  return left;
};

/**
 * Return the intersection set of two arrays.
 * @param {Array.<*>} left
 * @param {Array.<*>} right
 * @return {Array.<*>}
 */
var intersection = function(left, right) {
  return left.reduce(function(previous, current) {
    if (right.indexOf(current) != -1)
      previous.push(current);
    return previous;
  }, []);
};

/**
 * Return the difference set of left with right.
 * Notice: difference(left, right) differentiates with difference(right, left).
 * @param {Array.<*>} left
 * @param {Array.<*>} right
 * @return {Array.<*>}
 */
var difference = function(left, right) {
  return left.reduce(function(previous, current) {
    if (right.indexOf(current) == -1)
      previous.push(current);
    return previous;
  }, []);
};