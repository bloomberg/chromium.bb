// Copyright 2015  The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('image.collections.extension.domextractor.DocumentFeature');

goog.scope(function() {



/**
 * This is a base class for all document features (e.g. title, snippet, image).
 * @param {number} relevance Relevance of this feature to the document.
 * @constructor
 */
image.collections.extension.domextractor.DocumentFeature = function(relevance) {
  /** @private {number} */
  this.relevance_ = relevance;
};
var DocumentFeature = image.collections.extension.domextractor.DocumentFeature;


/**
 * Returns the feature relevance.
 * @return {number}
 */
DocumentFeature.prototype.getRelevance = function() {
  return this.relevance_;
};


/**
 * Compares two document features by their relevance.
 * @param {!DocumentFeature} feature1
 * @param {!DocumentFeature} feature2
 * @return {number}
 */
DocumentFeature.compare = function(feature1, feature2) {
  if (feature1 == feature2) {
    return 0;
  }
  return feature1.getRelevance() - feature2.getRelevance();
};
});  // goog.scope
