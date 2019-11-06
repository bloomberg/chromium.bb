// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Namespace for views.
 */
cca.views = cca.views || {};

/**
 * Creates the Browser view controller.
 * TODO(yuli): Merge GalleryBase into Browser.
 * @param {cca.models.Gallery} model Model object.
 * @extends {camera.view.GalleryBase}
 * @constructor
 */
cca.views.Browser = function(model) {
  cca.views.GalleryBase.call(this, '#browser', model);

  /**
   * @type {cca.util.SmoothScroller}
   * @private
   */
  this.scroller_ = new cca.util.SmoothScroller(
      document.querySelector('#browser'),
      document.querySelector('#browser .padder'));

  /**
   * @type {cca.HorizontalScrollBar}
   * @private
   */
  this.scrollBar_ = new cca.HorizontalScrollBar(this.scroller_);

  /**
   * Makes the browser scrollable by dragging with mouse.
   * @type {cca.util.MouseScroller}
   * @private
   */
  this.mouseScroller_ = new cca.util.MouseScroller(this.scroller_);

  /**
   * Monitores when scrolling is ended.
   * @type {cca.util.ScrollTracker}
   * @private
   */
  this.scrollTracker_ = new cca.util.ScrollTracker(
      this.scroller_,
      function() {}, // onScrollStarted
      this.onScrollEnded_.bind(this));

  /**
   * Sequential index of the last inserted picture. Used to generate unique
   * identifiers.
   * @type {number}
   * @private
   */
  this.lastPictureIndex_ = 0;

  // End of properties, seal the object.
  Object.seal(this);

  // Listen for clicking on the browser buttons.
  document.querySelector('#browser-print').addEventListener(
      'click', this.onPrintButtonClicked_.bind(this));
  document.querySelector('#browser-export').addEventListener(
      'click', this.onExportButtonClicked_.bind(this));
  document.querySelector('#browser-delete').addEventListener(
      'click', this.onDeleteButtonClicked_.bind(this));
  document.querySelector('#browser-back').addEventListener(
      'click', this.leave.bind(this));
};

cca.views.Browser.prototype = {
  __proto__: cca.views.GalleryBase.prototype,
};

/**
 * @param {cca.models.Gallery.Picture} picture Picture to be selected.
 * @override
 */
cca.views.Browser.prototype.entering = function(picture) {
  var index = this.pictureIndex(picture);
  if (index == null && this.pictures.length) {
    // Select the latest picture if the given picture isn't found.
    index = this.pictures.length - 1;
  }
  this.setSelectedIndex(index);
  this.scrollTracker_.start();
};

/**
 * @override
 */
cca.views.Browser.prototype.leaving = function() {
  this.scrollTracker_.stop();
  this.setSelectedIndex(null);
  return true;
};

/**
 * @override
 */
cca.views.Browser.prototype.focus = function() {
  if (!this.scroller_.animating) {
    this.synchronizeFocus();
  }
};

/**
 * @override
 */
cca.views.Browser.prototype.layout = function() {
  this.pictures.forEach(function(picture) {
    cca.views.Browser.updateElementSize_(picture.element);
  });

  this.scrollBar_.redraw();
  var selectedPicture = this.lastSelectedPicture();
  if (selectedPicture) {
    cca.util.scrollToCenter(selectedPicture.element, this.scroller_,
        cca.util.SmoothScroller.Mode.INSTANT);
  }
};

/**
 * Handles clicking on the print button.
 * @param {Event} event Click event.
 * @private
 */
cca.views.Browser.prototype.onPrintButtonClicked_ = function(event) {
  window.matchMedia('print').addListener(function(media) {
    if (!media.matches) {
      for (var index = 0; index < this.pictures.length; index++) {
        // Force the div wrappers to redraw by accessing their display and
        // offsetWidth property. Otherwise, they may not have the same width as
        // their child image or video element after printing.
        var wrapper = this.pictures[index].element;
        wrapper.style.display = 'none';
        wrapper.offsetWidth; // Reference forces width recalculation.
        wrapper.style.display = '';
      }
    }
  }.bind(this));
  window.print();
};

/**
 * Handles clicking on the export button.
 * @param {Event} event Click event.
 * @private
 */
cca.views.Browser.prototype.onExportButtonClicked_ = function(event) {
  this.exportSelection();
};

/**
 * Handles clicking on the delete button.
 * @param {Event} event Click event.
 * @private
 */
cca.views.Browser.prototype.onDeleteButtonClicked_ = function(event) {
  this.deleteSelection();
};

/**
 * Handles ending of scrolling.
 * @private
 */
cca.views.Browser.prototype.onScrollEnded_ = function() {
  var center = this.scroller_.scrollLeft + this.scroller_.clientWidth / 2;

  // Find the closest picture.
  var minDistance = -1;
  var minIndex = -1;
  for (var index = 0; index < this.pictures.length; index++) {
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
  if (minIndex != -1) {
    this.setSelectedIndex(minIndex);
  }
};

/**
 * Updates visibility of the browser buttons.
 * @private
 */
cca.views.Browser.prototype.updateButtons_ = function() {
  if (this.selectedIndexes.length) {
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
 * Updates visibility of the scrollbar thumb.
 * @private
 */
cca.views.Browser.prototype.updateScrollbarThumb_ = function() {
  // Hide the scrollbar thumb if there is only one picture.
  this.scrollBar_.setThumbHidden(this.pictures.length < 2);
};

/**
 * Updates resolutions of the pictures. The selected picture will be high
 * resolution, and all the rest low. This method waits until CSS transitions
 * are finished (if any). Element sizes would also be updated here after
 * updating resolutions.
 * @private
 */
cca.views.Browser.prototype.updatePicturesResolutions_ = function() {
  var wrappedElement = function(wrapper, tagName) {
    var wrapped = wrapper.firstElementChild;
    return (wrapped.tagName == tagName) ? wrapped : null;
  };

  var replaceElement = function(wrapper, element) {
    wrapper.replaceChild(element, wrapper.firstElementChild);
    cca.nav.setTabIndex(this, element, -1);
    cca.views.Browser.updateElementSize_(wrapper);
  }.bind(this);

  var updateImage = function(wrapper, url) {
    var img = wrappedElement(wrapper, 'IMG');
    if (!img) {
      img = document.createElement('img');
      img.onload = function() {
        replaceElement(wrapper, img);
      };
      img.src = url;
    } else if (img.src != url) {
      img.onload = function() {
        cca.views.Browser.updateElementSize_(wrapper);
      };
      img.src = url;
    }
  };

  var updateVideo = function(wrapper, url) {
    var video = document.createElement('video');
    video.controls = true;
    video.setAttribute('controlsList', 'nodownload nofullscreen');
    video.onloadeddata = function() {
      // Add the video element only if the selection has not been changed and
      // there is still no video element after loading video.
      var domPicture = this.lastSelectedPicture();
      if (domPicture && wrapper == domPicture.element &&
          !wrappedElement(wrapper, 'VIDEO')) {
        replaceElement(wrapper, video);
      }
    }.bind(this);
    video.src = url;
  }.bind(this);

  // Update resolutions immediately if no selection; otherwise, wait for pending
  // selection changes before updating the current selection's resolution.
  setTimeout(function() {
    var selectedPicture = this.lastSelectedPicture();
    this.pictures.forEach(function(domPicture) {
      var wrapper = domPicture.element;
      var picture = domPicture.picture;
      var thumbnailURL = picture.thumbnailURL;
      if (domPicture == selectedPicture || !thumbnailURL) {
        picture.pictureURL().then((url) => {
          if (picture.isMotionPicture) {
            updateVideo(wrapper, url);
          } else {
            updateImage(wrapper, url);
          }
        });
      } else {
        // Show cached thumbnails for unselected pictures.
        updateImage(wrapper, thumbnailURL);
      }
    });
  }.bind(this), (this.lastSelectedIndex() !== null) ? 75 : 0);
};

/**
 * @override
 */
cca.views.Browser.prototype.setSelectedIndex = function(index) {
  var scrollMode = (this.lastSelectedIndex() === null) ?
      cca.util.SmoothScroller.Mode.INSTANT :
      cca.util.SmoothScroller.Mode.SMOOTH;
  cca.views.GalleryBase.prototype.setSelectedIndex.apply(this, arguments);

  var selectedPicture = this.lastSelectedPicture();
  if (selectedPicture) {
    cca.util.scrollToCenter(
        selectedPicture.element, this.scroller_, scrollMode);
  }
  this.updateButtons_();

  // Update resolutions only if updating the selected index didn't cause any
  // scrolling.
  setTimeout(function() {
    // If animating, then onScrollEnded() will be called again after it is
    // finished. Therefore, we can ignore this call.
    if (!this.scroller_.animating) {
      this.updatePicturesResolutions_();
      this.synchronizeFocus();
    }
  }.bind(this), 0);
};

/**
 * @override
 */
cca.views.Browser.prototype.handlingKey = function(key) {
  switch (key) {
    case 'Right':
      if (this.pictures.length) {
        var leadIndex = this.lastSelectedIndex();
        if (leadIndex === null) {
          this.setSelectedIndex(this.pictures.length - 1);
        } else {
          this.setSelectedIndex(Math.max(0, leadIndex - 1));
        }
      }
      return true;
    case 'Left':
      if (this.pictures.length) {
        var leadIndex = this.lastSelectedIndex();
        if (leadIndex === null) {
          this.setSelectedIndex(0);
        } else {
          this.setSelectedIndex(
              Math.min(this.pictures.length - 1, leadIndex + 1));
        }
      }
      return true;
    case 'End':
      if (this.pictures.length) {
        this.setSelectedIndex(0);
      }
      return true;
    case 'Home':
      if (this.pictures.length) {
        this.setSelectedIndex(this.pictures.length - 1);
      }
      return true;
  }
  // Call the gallery-base view for unhandled keys.
  return cca.views.GalleryBase.prototype.handlingKey.apply(this, arguments);
};

/**
 * @override
 */
cca.views.Browser.prototype.onPictureDeleted = function(picture) {
  cca.views.GalleryBase.prototype.onPictureDeleted.apply(this, arguments);
  this.updateScrollbarThumb_();
};

/**
 * @override
 */
cca.views.Browser.prototype.addPictureToDOM = function(picture) {
  // Display high-res picture if no cached thumbnail.
  // TODO(yuli): Fix wrappers' size to avoid scrolling for changed elements.
  var thumbnailURL = picture.thumbnailURL;
  Promise.resolve(thumbnailURL || picture.pictureURL()).then((url) => {
    var wrapper = document.createElement('div');
    cca.nav.setTabIndex(this, wrapper, -1);
    cca.util.makeUnfocusableByMouse(wrapper);
    wrapper.className = 'media-wrapper';
    wrapper.id = 'browser-picture-' + (this.lastPictureIndex_++);
    wrapper.setAttribute('role', 'option');
    wrapper.setAttribute('aria-selected', 'false');

    var isVideo = !thumbnailURL && picture.isMotionPicture;
    var element = wrapper.appendChild(document.createElement(
        isVideo ? 'video' : 'img'));
    cca.nav.setTabIndex(this, element, -1);
    var updateElementSize = () => {
      cca.views.Browser.updateElementSize_(wrapper);
    };
    if (isVideo) {
      element.controls = true;
      element.setAttribute('controlsList', 'nodownload nofullscreen');
      element.onloadeddata = updateElementSize;
    } else {
      element.onload = updateElementSize;
    }
    element.src = url;

    // Insert the picture's DOM element in a sorted timestamp order.
    for (var index = this.pictures.length - 1; index >= 0; index--) {
      if (picture.timestamp >= this.pictures[index].picture.timestamp) {
        break;
      }
    }
    var browserPadder = document.querySelector('#browser .padder');
    var nextSibling = (index >= 0) ? this.pictures[index].element :
        browserPadder.lastElementChild;
    browserPadder.insertBefore(wrapper, nextSibling);
    this.updateScrollbarThumb_();

    var domPicture = new cca.views.GalleryBase.DOMPicture(picture, wrapper);
    this.pictures.splice(index + 1, 0, domPicture);

    wrapper.addEventListener('click', (event) => {
      // If scrolled while clicking, then discard this selection, since another
      // one will be choosen in the onScrollEnded handler.
      if (this.scrollTracker_.scrolling &&
          Math.abs(this.scrollTracker_.delta[0]) > 16) {
        return;
      }
      this.setSelectedIndex(this.pictures.indexOf(domPicture));
    });
  });
};

/**
 * Updates the picture element's size.
 * @param {HTMLElement} wrapper Element to be updated.
 * @private
 */
cca.views.Browser.updateElementSize_ = function(wrapper) {
  // Make the picture element not too large to overlap the buttons.
  var browserPadder = document.querySelector('#browser .padder');
  var maxWidth = browserPadder.clientWidth * 0.7;
  var maxHeight = browserPadder.clientHeight * 0.7;
  cca.util.updateElementSize(wrapper, maxWidth, maxHeight, false);
};
