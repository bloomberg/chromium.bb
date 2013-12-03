// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * Namespace for views.
 */
camera.views = camera.views || {};

/**
 * Creates the Browser view controller.
 *
 * @param {camera.View.Context} context Context object.
 * @param {camera.Router} router View router to switch views.
 * @extends {camera.view.GalleryBase}
 * @constructor
 */
camera.views.Browser = function(context, router) {
  camera.views.GalleryBase.call(
      this, context, router, document.querySelector('#browser'), 'browser');

  /**
   * @type {camera.util.SmoothScroller}
   * @private
   */
  this.scroller_ = new camera.util.SmoothScroller(
      document.querySelector('#browser'),
      document.querySelector('#browser .padder'));

  /**
   * @type {camera.HorizontalScrollBar}
   * @private
   */
  this.scrollBar_ = new camera.HorizontalScrollBar(this.scroller_);

  /**
   * Makes the browser scrollable by dragging with mouse.
   * @type {camera.util.MouseScroller}
   * @private
   */
  this.mouseScroller_ = new camera.util.MouseScroller(this.scroller_);

  /**
   * Monitores when scrolling is ended.
   * @type {camera.util.ScrollTracker}
   * @private
   */
  this.scrollTracker_ = new camera.util.ScrollTracker(
      this.scroller_,
      function() {},  // onScrollStarted
      this.onScrollEnded_.bind(this));

  /**
   * Sequential index of the last inserted picture. Used to generate unique
   * identifiers.
   *
   * @type {number}
   * @private
   */
  this.lastPictureIndex_ = 0;

  // End of properties, seal the object.
  Object.seal(this);

  // Register clicking on the background to close the browser view.
  document.querySelector('#browser').addEventListener('click',
      this.onBackgroundClicked_.bind(this));

  // Listen for clicking on the browser buttons.
  document.querySelector('#browser-print').addEventListener(
      'click', this.onPrintButtonClicked_.bind(this));
  document.querySelector('#browser-export').addEventListener(
      'click', this.onExportButtonClicked_.bind(this));
  document.querySelector('#browser-delete').addEventListener(
      'click', this.onDeleteButtonClicked_.bind(this));
  document.querySelector('#browser-back').addEventListener(
      'click', this.router.back.bind(this.router));
};

camera.views.Browser.prototype = {
  __proto__: camera.views.GalleryBase.prototype
};

/**
 * Enters the view.
 * @override
 */
camera.views.Browser.prototype.onEnter = function() {
  this.onResize();
  this.scrollTracker_.start();
  this.updateButtons_();
  if (!this.scroller_.animating)
    this.updatePicturesResolutions_();
  document.body.classList.add('browser');
};

/**
 * Leaves the view.
 * @override
 */
camera.views.Browser.prototype.onLeave = function() {
  this.scrollTracker_.stop();
};

/**
 * @override
 */
camera.views.Browser.prototype.onActivate = function() {
  document.querySelector('#browser').focus();
};

/**
 * @override
 */
camera.views.Browser.prototype.onResize = function() {
  if (this.currentPicture()) {
    camera.util.scrollToCenter(this.currentPicture().element,
                               this.scroller_,
                               camera.util.SmoothScroller.Mode.INSTANT);
  }
};

/**
 * Handles clicking (or touching) on the background.
 * @param {Event} event Click event.
 * @private
 */
camera.views.Browser.prototype.onBackgroundClicked_ = function(event) {
  if (event.target == document.querySelector('#browser'))
    this.router.back();
};

/**
 * Handles clicking on the print button.
 * @param {Event} event Click event.
 * @private
 */
camera.views.Browser.prototype.onPrintButtonClicked_ = function(event) {
  window.print();
};

/**
 * Handles clicking on the export button.
 * @param {Event} event Click event.
 * @private
 */
camera.views.Browser.prototype.onExportButtonClicked_ = function(event) {
  this.exportSelection_();
};

/**
 * Handles clicking on the delete button.
 * @param {Event} event Click event.
 * @private
 */
camera.views.Browser.prototype.onDeleteButtonClicked_ = function(event) {
  this.deleteSelection();
};

/**
 * Handles ending of scrolling.
 * @private
 */
camera.views.Browser.prototype.onScrollEnded_ = function() {
  var center = this.scroller_.scrollLeft + this.scroller_.clientWidth / 2;

  // Find the closest picture.
  var minDistance = -1;
  var minIndex = -1;
  for (var index = 0; index < this.model.length; index++) {
    var element = this.pictures[index].element;
    var distance = Math.abs(
        element.offsetLeft + element.offsetWidth / 2 - center);
    if (minIndex == -1 || distance < minDistance) {
      minDistance = distance;
      minIndex = index;
    }
  }

  // Select the closest picture to the center of the window.
  // This may invoke scrolling, to center the currently selected picture.
  if (minIndex != -1)
    this.model.currentIndex = minIndex;

  // Update resolutions only if updating the currentIndex didn't cause any
  // scrolling.
  setTimeout(function() {
    // If animating, then onScrollEnded() will be called again after it is
    // finished. Therefore, we can ignore this call.
    if (!this.scroller_.animating)
      this.updatePicturesResolutions_();
  }.bind(this), 0);
};

/**
 * Updates visibility of the browser buttons.
 * @private
 */
camera.views.Browser.prototype.updateButtons_ = function() {
  var pictureSelected = this.model.currentIndex !== null;
  if (pictureSelected) {
    document.querySelector('#browser-print').removeAttribute('disabled');
    document.querySelector('#browser-export').removeAttribute('disabled');
    document.querySelector('#browser-delete').removeAttribute('disabled');
  } else {
    document.querySelector('#browser-print').setAttribute('disabled', '');
    document.querySelector('#browser-export').setAttribute('disabled', '');
    document.querySelector('#browser-delete').setAttribute('disabled', '');
  }
};

/**
 * Updates resolutions of the pictures. The selected picture will be high
 * resolution, and all the rest low. This method waits until CSS transitions
 * are finished (if any).
 *
 * @private
 */
camera.views.Browser.prototype.updatePicturesResolutions_ = function() {
 var updateResolutions = function() {
    for (var index = 0; index < this.pictures.length; index++) {
      this.pictures[index].displayResolution =
          (index == this.model.currentIndex) ?
              camera.views.GalleryBase.DOMPicture.DisplayResolution.HIGH :
              camera.views.GalleryBase.DOMPicture.DisplayResolution.LOW;
    }
  }.bind(this);

  // Wait until the zoom-in transition is finished, and then update pictures'
  // resolutions.
  if (this.model.currentIndex !== null) {
    camera.util.waitForTransitionCompletion(
        this.pictures[this.model.currentIndex].element,
        250,  // Timeout in ms.
        updateResolutions);
  } else {
    // No selection.
    updateResolutions();
  }
};

/**
 * @override
 */
camera.views.Browser.prototype.onCurrentIndexChanged = function(
    oldIndex, newIndex) {
  camera.views.GalleryBase.prototype.onCurrentIndexChanged.apply(
      this, arguments);

  if (newIndex !== null) {
   camera.util.scrollToCenter(this.currentPicture().element,
                              this.scroller_);
  }

  this.updateButtons_();
};

/**
 * @override
 */
camera.views.Browser.prototype.onPictureDeleting = function(index) {
  camera.views.GalleryBase.prototype.onPictureDeleting.apply(
      this, arguments);

  // Update the resolutions, since the current image might have changed
  // without scrolling and without scrolling. Use a timer, to wait until
  // the picture is deleted from the model.
  setTimeout(this.updatePicturesResolutions_.bind(this), 0);
};

/**
 * Exports the selected picture. If nothing selected, then nothing happens.
 * @private
 */
camera.views.Browser.prototype.exportSelection_ = function() {
  if (!this.currentPicture())
    return;

  var accepts = [{
    description: '*.jpg',
    extensions: ['jpg", "jpeg'],
    mimeTypes: ['image/jpeg']
  }];

  var picture = this.currentPicture().picture;

  var onError = function() {
    // TODO(mtomasz): Check if it works.
    this.context_.onError(
        'gallery-export-error',
        chrome.i18n.getMessage('errorMsgGalleryExportFailed'));
  }.bind(this);

  chrome.fileSystem.chooseEntry({
    type: 'saveFile',
    suggestedName: picture.imageEntry.name,
    accepts: accepts
  }, function(fileEntry) {
      if (!fileEntry)
        return;
      this.model.exportPicture(
          picture,
          fileEntry,
          function() {},
          onError);
  }.bind(this));
};

/**
 * @override
 */
camera.views.Browser.prototype.onKeyPressed = function(event) {
  var currentPicture = this.currentPicture();
  switch (camera.util.getShortcutIdentifier(event)) {
    case 'Ctrl-U+0053':  // Ctrl+S for saving.
      this.exportSelection_();
      event.preventDefault();
      return;
    case 'Ctrl-U+0050':  // Ctrl+P for printing.
      window.print();
      event.preventDefault();
      return;
  }

  // Call the base view for unhandled keys.
  camera.views.GalleryBase.prototype.onKeyPressed.apply(this, arguments);
};

/**
 * @override
 */
camera.views.Browser.prototype.addPictureToDOM = function(picture) {
  var browser = document.querySelector('#browser .padder');
  var boundsPadder = browser.querySelector('.bounds-padder');
  var img = document.createElement('img');
  img.id = 'browser-picture-' + (this.lastPictureIndex_++);
  img.setAttribute('aria-role', 'option');
  img.setAttribute('aria-selected', 'false');
  browser.insertBefore(img, boundsPadder.nextSibling);

  // Add to the collection.
  this.pictures.push(new camera.views.GalleryBase.DOMPicture(picture, img));

  img.addEventListener('click', function(event) {
    // If scrolled while clicking, then discard this selection, since another
    // one will be choosen in the onScrollEnded handler.
    if (this.scrollTracker_.scrolling &&
        Math.abs(this.scrollTracker_.delta[0]) > 16) {
      return;
    }

    this.model.currentIndex = this.model.pictures.indexOf(picture);
  }.bind(this));
};

/**
 * @override
 */
camera.views.Browser.prototype.ariaListNode = function() {
  return document.querySelector('#browser');
};

