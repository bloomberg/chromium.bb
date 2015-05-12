// Copyright 2015  The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('image.collections.extension.domextractor.AdElementFilter');

goog.require('image.collections.extension.domextractor.DomUtils');
goog.require('image.collections.extension.domextractor.ElementFilter');

goog.scope(function() {
var DomUtils = image.collections.extension.domextractor.DomUtils;



/**
 * Filters out potential ad elements.
 * @constructor
 * @implements {image.collections.extension.domextractor.ElementFilter}
 */
image.collections.extension.domextractor.AdElementFilter = function() {
  /** @private {!Array.<string>} */
  this.adWords_ = ['ad', 'advertisement', 'ads', 'sponsor', 'sponsored'];
};
var AdElementFilter = image.collections.extension.domextractor.AdElementFilter;


/** @override */
AdElementFilter.prototype.filter = function(element) {
  var ancestorElement = element;
  while (ancestorElement) {
    var classNames = ancestorElement.classList;
    for (var i = 0; i < classNames.length; ++i) {
      var tokens = classNames[i].split(/\W+/);
      for (var j = 0; j < tokens.length; ++j) {
        if (this.adWords_.indexOf(tokens[j].toLowerCase()) >= 0) {
          return false;
        }
      }
    }
    ancestorElement = DomUtils.getParentElement(ancestorElement);
  }
  return true;
};
});  // goog.scope
