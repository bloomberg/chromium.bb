// Copyright 2015  The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('image.collections.extension.domextractor.DocumentFeatureExtractor');

goog.require('image.collections.extension.domextractor.DocumentFeature');
goog.require('image.collections.extension.domextractor.ElementFilter');

goog.scope(function() {
var DocumentFeature = image.collections.extension.domextractor.DocumentFeature;
var ElementFilter = image.collections.extension.domextractor.ElementFilter;



/**
 * A base class for document feature (title, snippet, image) extractors.
 * @constructor
 */
image.collections.extension.domextractor.DocumentFeatureExtractor = function() {
  /** @private {!Array.<!ElementFilter>} */
  this.filters_ = [];
};
var DocumentFeatureExtractor =
    image.collections.extension.domextractor.DocumentFeatureExtractor;


/**
 * Installs an element filter.
 * @param {!ElementFilter} filter
 * @protected
 */
DocumentFeatureExtractor.prototype.addFilter = function(filter) {
  this.filters_.push(filter);
};


/**
 * Returns true iff an element passes all the filters.
 * @param {!Element} element
 * @return {boolean}
 */
DocumentFeatureExtractor.prototype.filter = function(element) {
  for (var i = 0; i < this.filters_.length; ++i) {
    if (!this.filters_[i].filter(element)) {
      return false;
    }
  }
  return true;
};


/**
 * Given an HTML document, returns a feature with a highest relevance value.
 * TODO(busaryev): look at the mean, median relevance etc.
 * @param {!Document} doc HTML document.
 * @return {DocumentFeature}
 */
DocumentFeatureExtractor.prototype.extractBestFromDocument = function(doc) {
  var features = this.extractAllFromDocument(doc);
  if (features.length == 0) return null;
  var best = features[0];
  for (var i = 1; i < features.length; ++i) {
    var feature = features[i];
    if (feature.getRelevance() > best.getRelevance()) {
      best = feature;
    }
  }
  return best;
};


/**
 * Given an HTML document, returns an array of features sorted by relevance.
 * By default, this function tries to extract a feature from every DOM element.
 * Derived classes may override this function to look at a subset of DOM (e.g.
 * only <meta> and <link> elements).
 * @param {!Document} doc HTML document.
 * @return {!Array.<!DocumentFeature>}
 */
DocumentFeatureExtractor.prototype.extractAllFromDocument = function(doc) {
  return this.extractFromNodeList(doc.getElementsByTagName('*'));
};


/**
 * This function tries to extract a feature from every node in a nodelist
 * and returns an array of features.
 * @param {!NodeList} nodeList DOM node list.
 * @param {!Object=} opt_context Optional context.
 * @return {!Array.<!DocumentFeature>}
 */
DocumentFeatureExtractor.prototype.extractFromNodeList = function(
    nodeList, opt_context) {
  var result = [];
  var nodeListLength = nodeList.length;
  for (var j = 0; j < nodeListLength; ++j) {
    var feature = this.extractFromElement(nodeList[j], opt_context);
    if (feature) {
      result.push(feature);
    }
  }
  return result;
};


/**
 * This function extracts a feature from an HTML element. It should be
 * overridden in derived classes, unless the feature can be extracted without
 * looking at DOM.
 * @param {!Element} element HTML element.
 * @param {!Object=} opt_context Optional context.
 * @return {DocumentFeature}
 */
DocumentFeatureExtractor.prototype.extractFromElement = function(
    element, opt_context) {
  return null;
};
});  // goog.scope
