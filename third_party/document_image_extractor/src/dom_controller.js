// Copyright 2015  The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('image.collections.extension.DomController');

goog.require('goog.Timer');
goog.require('goog.array');
goog.require('goog.dom');
goog.require('goog.events.EventType');
goog.require('goog.log');
goog.require('gws.collections.common.Constants');
goog.require('image.collections.extension.Controller');
goog.require('image.collections.extension.DocumentImage');
goog.require('image.collections.extension.DocumentVideo');
goog.require('image.collections.extension.DomEvent');

goog.scope(function() {
var Constants = gws.collections.common.Constants;
var DocumentImage = image.collections.extension.DocumentImage;
var DocumentVideo = image.collections.extension.DocumentVideo;
var DomEvent = image.collections.extension.DomEvent;


/**
 * This class handles page DOM events and implements DOM manipulation.
 * It should be instantiated by a content script.
 * TODO(busaryev): preloading may not be the best choice for mobile clients.
 * @extends {image.collections.extension.Controller}
 * @constructor
 */
image.collections.extension.DomController = function() {
  DomController.base(this, 'constructor');

  /** @private {number} Number of DOM elements left. */
  this.numElementsToProcess_ = 0;

  /** @private {number} The timeout id for goog.Timer.callOnce. */
  this.timeoutId_ = -1;
};
goog.inherits(image.collections.extension.DomController,
              image.collections.extension.Controller);
var DomController = image.collections.extension.DomController;


/** @private {goog.log.Logger} */
DomController.logger_ = goog.log.getLogger(
    'image.collections.extension.DomController');


/**
 * @private {number} The number of milliseconds to wait for the load
 * event to occur before giving up. This ensures we are not wasting too much
 * time trying to get the clip.
 */
DomController.LOAD_TIMEOUT_MS_ = 5000;


/** @override */
DomController.prototype.initialize = function(parentEventTarget) {
  DomController.base(this, 'initialize', parentEventTarget);

  this.eventHandler.
      listen(parentEventTarget, DomEvent.Type.INITIALIZE_DOM,
             this.handleInitializeDom_);
};


/**
 * @param {DomEvent} e
 * @private
 */
DomController.prototype.handleInitializeDom_ = function(e) {
  if (this.numElementsToProcess_ == 0) {
    // Find <meta> and <link> tags that specify canonical page images, compute
    // image sizes with preloading and store them in element attributes.
    var doc = goog.dom.getDocument();
    var metaElements = doc.getElementsByTagName('meta');
    var linkElements = doc.getElementsByTagName('link');
    this.numElementsToProcess_ = metaElements.length + linkElements.length;
    if (this.numElementsToProcess_ > 0) {
      goog.array.forEach(metaElements, this.processMetaElement_, this);
      goog.array.forEach(linkElements, this.processLinkElement_, this);
      this.timeoutId_ = goog.Timer.callOnce(
          goog.bind(this.dispatchEvent, this, DomEvent.Type.DOM_INITIALIZED),
          DomController.LOAD_TIMEOUT_MS_);
    } else {
      this.dispatchEvent(DomEvent.Type.DOM_INITIALIZED);
    }
  }
};


/**
 * Tries to compute the size of the image specified in a <meta> element.
 * @param {Element} element The element to process.
 * @param {number} index Index of the element in the array.
 * @param {goog.array.ArrayLike} array The array.
 * @private
 */
DomController.prototype.processMetaElement_ = function(element, index, array) {
  var url = '';
  if (element.hasAttribute('property')) {
    switch (element.getAttribute('property').toLowerCase()) {
      case 'og:image':
        url = element.getAttribute('content');
        var siblings = goog.dom.getChildren(goog.dom.getParentElement(element));
        var width = this.getPropertyContent_(siblings, 'og:image:width');
        var height = this.getPropertyContent_(siblings, 'og:image:height');
        if (width > 0 && height > 0) {
          element.setAttribute(DocumentImage.CustomAttribute.WIDTH, width);
          element.setAttribute(DocumentImage.CustomAttribute.HEIGHT, height);
        }
        break;
      case 'og:video':
        var children = goog.dom.getChildren(goog.dom.getParentElement(element));
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
 * @param {number} index Index of the element in the array.
 * @param {goog.array.ArrayLike} array The array.
 * @private
 */
DomController.prototype.processLinkElement_ = function(element, index, array) {
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
    this.computeImageSize_(url, goog.bind(this.storeImageSize_, this, element));
  } else {
    this.maybeDispatchDomInitialized_();
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
  this.eventHandler.listenOnce(image,
      [goog.events.EventType.LOAD, goog.events.EventType.ERROR],
      goog.bind(this.handleImageLoadOrError_, this, callback));
  image.src = url;
};


/**
 * Handles image LOAD and ERROR events.
 * @param {!function(number, number)} callback A callback.
 * @param {goog.events.Event} e Image event.
 * @private
 */
DomController.prototype.handleImageLoadOrError_ = function(callback, e) {
  var image = /** @type {!Image} */ (e.target);
  if (e.type == goog.events.EventType.LOAD) {
    callback(image.naturalWidth, image.naturalHeight);
  } else {
    goog.log.warning(DomController.logger_,
        'Failed to load image ' + image.src);
  }
  this.maybeDispatchDomInitialized_();
};


/**
 * Dispatches the DOM_INITIALIZED event if all elements have been processed.
 * @private
 */
DomController.prototype.maybeDispatchDomInitialized_ = function() {
  if (--this.numElementsToProcess_ == 0) {
    if (this.timeoutId_ != -1) {
      goog.Timer.clear(this.timeoutId_);
      this.timeoutId_ = -1;
    }
    this.dispatchEvent(DomEvent.Type.DOM_INITIALIZED);
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
