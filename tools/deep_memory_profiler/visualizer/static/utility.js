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
 * Return the intersection set of two sorted arrays.
 * @param {Array.<*>} left
 * @param {Array.<*>} right
 * @return {Array.<*>}
 */
var intersectionOfSorted = function(left, right) {
  var from = 0;
  return left.reduce(function(previous, current) {
    var idx = right.indexOf(current, from);
    if (idx != -1) {
      previous.push(current);
      from = idx;
    }
    return previous;
  }, []);
};

/**
 * Output object with indented format.
 * @param  {Object} obj
 * @param  {string} title
 */
var inspect = function(obj, title) {
  if (title) console.log(title);
  console.log(JSON.stringify(obj, null, 2));
};
