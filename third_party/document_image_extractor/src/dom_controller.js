// Copyright 2015  The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('image.collections.extension.domextractor.DomController');

goog.require('image.collections.extension.domextractor.DocumentImage');
goog.require('image.collections.extension.domextractor.DocumentVideo');
goog.require('image.collections.extension.domextractor.DomUtils');

goog.scope(function() {
var DocumentImage = image.collections.extension.domextractor.DocumentImage;
var DocumentVideo = image.collections.extension.domextractor.DocumentVideo;
var DomUtils = image.collections.extension.domextractor.DomUtils;



/**
 * This class handles page DOM events and implements DOM manipulation.
 * It should be instantiated by a content script.
 * TODO(busaryev): preloading may not be the best choice for mobile clients.
 * @constructor
 */
image.collections.extension.domextractor.DomController = function() {
  /** @private {number} Number of DOM elements left. */
  this.numElementsToProcess_ = 0;

  /**
   * Promise returned by initialize() call. Resolves when all elements have been
   * processed, or alternatively when a timeout has been reached.
   * @private {!Promise}
   */
  this.initializedPromise_ = Promise.resolve();

  /** @private {?function()} Resolve function for the initialized promise. */
  this.initializedPromiseResolve_ = null;
};
var DomController = image.collections.extension.domextractor.DomController;


/**
 * @private {number} The number of milliseconds to wait for the load
 * event to occur before giving up. This ensures we are not wasting too much
 * time trying to get the clip.
 */
DomController.LOAD_TIMEOUT_MS_ = 5000;


/**
 * Initializes the DomController.
 * @return {!Promise} A promise that resolves when all elements have been
 *     processed, or alternatively after a timeout has expired.
 */
DomController.prototype.initialize = function() {
  if (this.numElementsToProcess_ == 0) {
    // Find <meta> and <link> tags that specify canonical page images, compute
    // image sizes with preloading and store them in element attributes.
    var metaElements = document.getElementsByTagName('meta');
    var linkElements = document.getElementsByTagName('link');
    this.numElementsToProcess_ = metaElements.length + linkElements.length;
    if (this.numElementsToProcess_ > 0) {
      this.initializedPromise_ =
          new Promise(function(resolve, reject) {
            this.initializedPromiseResolve_ = resolve;
          }.bind(this));
      for (var i = 0; i < metaElements.length; i++) {
        this.processMetaElement_(metaElements[i]);
      }
      for (var i = 0; i < linkElements.length; i++) {
        this.processLinkElement_(linkElements[i]);
      }
      setTimeout(this.initializedPromiseResolve_,
          DomController.LOAD_TIMEOUT_MS_);
      return this.initializedPromise_;
    } else {
      this.initializedPromise_ = Promise.resolve();
      return this.initializedPromise_;
    }
  } else {
    return this.initializedPromise_;
  }
};


/**
 * Tries to compute the size of the image specified in a <meta> element.
 * @param {Element} element The element to process.
 * @private
 */
DomController.prototype.processMetaElement_ = function(element) {
  var url = '';
  if (element.hasAttribute('property')) {
    switch (element.getAttribute('property').toLowerCase()) {
      case 'og:image':
        url = element.getAttribute('content');
        var siblings = DomUtils.getParentElement(element).children;
        var width = this.getPropertyContent_(siblings, 'og:image:width');
        var height = this.getPropertyContent_(siblings, 'og:image:height');
        if (width > 0 && height > 0) {
          element.setAttribute(DocumentImage.CustomAttribute.WIDTH, width);
          element.setAttribute(DocumentImage.CustomAttribute.HEIGHT, height);
        }
        break;
      case 'og:video':
        var children = DomUtils.getParentElement(element).children;
        var width = this.getPropertyContent_(children, 'og:video:width');
        var height = this.getPropertyContent_(children, 'og:video:height');
        if (width > 0 && height > 0) {
          element.setAttribute(DocumentVideo.CustomAttribute.WIDTH, width);
          element.setAttribute(DocumentVideo.CustomAttribute.HEIGHT, height);
        }
    }
  } else if (element.hasAttribute('name')) {
    switch (element.getAttribute('name').toLowerCase()) {
      case 'msapplication-tileimage':
      case 'twitter:image':
        url = element.getAttribute('content');
    }
  } else if (element.hasAttribute('itemprop')) {
    switch (element.getAttribute('itemprop').toLowerCase()) {
      case 'thumbnailurl':
        url = element.getAttribute('href') ||
            element.getAttribute('content');
    }
  }
  this.maybeComputeAndStoreImageSize_(url, element);
};


/**
 * Tries to compute the size of the image specified in a <link> element.
 * @param {Element} element The element to process.
 * @private
 */
DomController.prototype.processLinkElement_ = function(element) {
  var url = '';
  if (element.hasAttribute('rel')) {
    switch (element.getAttribute('rel').toLowerCase()) {
      case 'apple-touch-icon-precomposed':
      case 'apple-touch-icon':
      case 'image_src':
        url = element.getAttribute('href');
    }
  } else if (element.hasAttribute('itemprop')) {
    switch (element.getAttribute('itemprop').toLowerCase()) {
      case 'thumbnailurl':
        url = element.getAttribute('href') ||
            element.getAttribute('content');
    }
  }
  this.maybeComputeAndStoreImageSize_(url, element);
};


/**
 * Given a node list, tries to find an element with a 'property' attribute
 * set to a given value and returns the value of its 'content' attribute.
 * @param {Array|NodeList} elements Node list.
 * @param {string} property Expected property value.
 * @return {string}
 * @private
 */
DomController.prototype.getPropertyContent_ = function(elements, property) {
  for (var i = 0; i < elements.length; ++i) {
    var element = elements[i];
    if (element.hasAttribute('property') &&
        element.getAttribute('property').toLowerCase() == property &&
        element.hasAttribute('content')) {
      return element.getAttribute('content');
    }
  }
  return '';
};


/**
 * Tries to store the size of the element image in custom element tags.
 * @param {string} url Image url (empty if the element defines no image).
 * @param {!Element} element Element.
 * @private
 */
DomController.prototype.maybeComputeAndStoreImageSize_ = function(
    url, element) {
  var CustomAttribute = DocumentImage.CustomAttribute;
  if (url && (!element.hasAttribute(CustomAttribute.WIDTH) ||
      !element.hasAttribute(CustomAttribute.HEIGHT))) {
    this.computeImageSize_(url, this.storeImageSize_.bind(this, element));
  } else {
    this.maybeResolveInitializedPromise_();
  }
};


/**
 * Computes the image size with preloading and returns it via a callback.
 * @param {string} url Image url.
 * @param {!function(number, number)} callback A callback.
 * @private
 */
DomController.prototype.computeImageSize_ = function(url, callback) {
  var image = new Image();

  var that = this;
  var handleImageLoadOrError = function(e) {
    if (e.type == 'load') {
      callback(image.naturalWidth, image.naturalHeight);
    }
    that.maybeResolveInitializedPromise_();
    image.removeEventListener('load', handleImageLoadOrError);
    image.removeEventListener('error', handleImageLoadOrError);
  };

  image.addEventListener('load', handleImageLoadOrError);
  image.addEventListener('error', handleImageLoadOrError);
  image.src = url;
};


/**
 * Resolves the initialized promise if all elements have been processed.
 * @private
 */
DomController.prototype.maybeResolveInitializedPromise_ = function() {
  if (--this.numElementsToProcess_ == 0) {
    this.initializedPromiseResolve_();
  }
};


/**
 * Stores the image size in custom element attributes.
 * @param {!Element} element An element defining the image url.
 * @param {number} width Image width.
 * @param {number} height Image height.
 * @private
 */
DomController.prototype.storeImageSize_ = function(element, width, height) {
  var CustomAttribute = DocumentImage.CustomAttribute;
  element.setAttribute(CustomAttribute.WIDTH, width);
  element.setAttribute(CustomAttribute.HEIGHT, height);
};

});  // goog.scope
