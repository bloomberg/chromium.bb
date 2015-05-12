// Copyright 2015  The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('image.collections.extension.domextractor.VisibleElementFilter');

goog.require('image.collections.extension.domextractor.DomUtils');
goog.require('image.collections.extension.domextractor.ElementFilter');

goog.scope(function() {
var DomUtils = image.collections.extension.domextractor.DomUtils;



/**
 * Filters elements by visibility.
 * @implements {image.collections.extension.domextractor.ElementFilter}
 * @constructor
 */
image.collections.extension.domextractor.VisibleElementFilter = function() {};
var VisibleElementFilter =
    image.collections.extension.domextractor.VisibleElementFilter;


/** @override */
VisibleElementFilter.prototype.filter = function(element) {
  var ancestorElement = element;
  while (ancestorElement) {
    if (DomUtils.getComputedStyle(ancestorElement, 'display') == 'none') {
      return false;
    }
    var size = DomUtils.getSize(ancestorElement);
    var overflow = DomUtils.getComputedStyle(ancestorElement, 'overflow');
    if ((overflow == 'hidden') && size.area() == 0) {
      return false;
    }
    if (DomUtils.getComputedStyle(ancestorElement, 'visibility') ==
        'hidden') {
      return false;
    }

    ancestorElement = DomUtils.getParentElement(ancestorElement);
  }
  return true;
};
});  // goog.scope
