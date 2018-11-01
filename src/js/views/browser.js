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
 * TODO(yuli): Merge GalleryBase into Browser.
 * @param {camera.Router} router View router to switch views.
 * @param {camera.models.Gallery} model Model object.
 * @extends {camera.view.GalleryBase}
 * @constructor
 */
camera.views.Browser = function(router, model) {
  camera.views.GalleryBase.call(
      this, router, model, document.querySelector('#browser'), 'browser');

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
      'click', this.router.back.bind(this.router));
};

camera.views.Browser.prototype = {
  __proto__: camera.views.GalleryBase.prototype,
};

/**
 * Prepares the view.
 */
camera.views.Browser.prototype.prepare = function() {
  // Hide export-button if using external file system.
  if (camera.models.FileSystem.externalFs) {
    document.querySelector('#browser-export').hidden = true;
  }
};

/**
 * Enters the view. Assumes, that the arguments may be provided.
 * @param {Object=} opt_arguments Arguments for the browser.
 * @override
 */
camera.views.Browser.prototype.onEnter = function(opt_arguments) {
  var index = null;
  if (opt_arguments && opt_arguments.picture) {
    index = this.pictureIndex(opt_arguments.picture);
  }
  // Navigate to the newest picture if the given picture isn't found.
  if (index == null && this.pictures.length) {
    index = this.pictures.length - 1;
  }
  this.setSelectedIndex(index);

  this.onResize();
  this.scrollTracker_.start();
};

/**
 * @override
 */
camera.views.Browser.prototype.onActivate = function() {
  camera.views.GalleryBase.prototype.onActivate.apply(this, arguments);
  if (!this.scroller_.animating)
    this.synchronizeFocus();
};

/**
 * @override
 */
camera.views.Browser.prototype.onLeave = function() {
  this.scrollTracker_.stop();
  this.setSelectedIndex(null);
};

/**
 * @override
 */
camera.views.Browser.prototype.onResize = function() {
  this.pictures.forEach(function(picture) {
    camera.views.Browser.updateElementSize_(picture.element);
  });

  this.scrollBar_.onResize();
  var selectedPicture = this.lastSelectedPicture();
  if (selectedPicture) {
    camera.util.scrollToCenter(selectedPicture.element,
                               this.scroller_,
                               camera.util.SmoothScroller.Mode.INSTANT);
  }
};

/**
 * Handles clicking on the print button.
 * @param {Event} event Click event.
 * @private
 */
camera.views.Browser.prototype.onPrintButtonClicked_ = function(event) {
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
camera.views.Browser.prototype.onExportButtonClicked_ = function(event) {
  this.exportSelection();
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
  if (minIndex != -1)
    this.setSelectedIndex(minIndex);
};

/**
 * Updates visibility of the browser buttons.
 * @private
 */
camera.views.Browser.prototype.updateButtons_ = function() {
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
camera.views.Browser.prototype.updateScrollbarThumb_ = function() {
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
camera.views.Browser.prototype.updatePicturesResolutions_ = function() {
  var wrappedElement = function(wrapper, tagName) {
    var wrapped = wrapper.firstElementChild;
    return (wrapped.tagName == tagName) ? wrapped : null;
  };

  var replaceElement = function(wrapper, element) {
    wrapper.replaceChild(element, wrapper.firstElementChild);
    camera.views.Browser.updateElementSize_(wrapper);
  };

  var updateImage = function(wrapper, url) {
    var img = wrappedElement(wrapper, 'IMG');
    if (!img) {
      img = document.createElement('img');
      img.tabIndex = -1;
      img.onload = function() {
        replaceElement(wrapper, img);
      };
      img.src = url;
    } else if (img.src != url) {
      img.onload = function() {
        camera.views.Browser.updateElementSize_(wrapper);
      };
      img.src = url;
    }
  };

  var updateVideo = function(wrapper, url) {
    var video = document.createElement('video');
    video.tabIndex = -1;
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
        picture.pictureURL().then(url => {
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
camera.views.Browser.prototype.setSelectedIndex = function(index) {
  camera.views.GalleryBase.prototype.setSelectedIndex.apply(this, arguments);

  var selectedPicture = this.lastSelectedPicture();
  if (selectedPicture) {
    camera.util.scrollToCenter(selectedPicture.element, this.scroller_);
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
camera.views.Browser.prototype.onKeyPressed = function(event) {
  switch (camera.util.getShortcutIdentifier(event)) {
    case 'Right':
      if (this.pictures.length) {
        var leadIndex = this.lastSelectedIndex();
        if (leadIndex === null) {
          this.setSelectedIndex(this.pictures.length - 1);
        } else {
          this.setSelectedIndex(Math.max(0, leadIndex - 1));
        }
      }
      event.preventDefault();
      return;
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
      event.preventDefault();
      return;
    case 'End':
      if (this.pictures.length)
        this.setSelectedIndex(0);
      event.preventDefault();
      return;
    case 'Home':
      if (this.pictures.length)
        this.setSelectedIndex(this.pictures.length - 1);
      event.preventDefault();
      return;
  }

  // Call the base view for unhandled keys.
  camera.views.GalleryBase.prototype.onKeyPressed.apply(this, arguments);
};

/**
 * @override
 */
camera.views.Browser.prototype.onPictureDeleted = function(picture) {
  camera.views.GalleryBase.prototype.onPictureDeleted.apply(this, arguments);
  this.updateScrollbarThumb_();
};

/**
 * @override
 */
camera.views.Browser.prototype.addPictureToDOM = function(picture) {
  var wrapper = document.createElement('div');
  wrapper.className = 'media-wrapper';
  wrapper.id = 'browser-picture-' + (this.lastPictureIndex_++);
  wrapper.tabIndex = -1;
  wrapper.setAttribute('role', 'option');
  wrapper.setAttribute('aria-selected', 'false');

  // Display high-res picture if no cached thumbnail.
  // TODO(yuli): Fix wrappers' size to avoid scrolling for changed elements.
  var thumbnailURL = picture.thumbnailURL;
  Promise.resolve(thumbnailURL || picture.pictureURL()).then(url => {
    var isVideo = !thumbnailURL && picture.isMotionPicture;
    var element = wrapper.appendChild(document.createElement(
        isVideo ? 'video' : 'img'));
    var updateElementSize = () => {
      camera.views.Browser.updateElementSize_(wrapper);
    };
    if (isVideo) {
      element.controls = true;
      element.setAttribute('controlsList', 'nodownload nofullscreen');
      element.onloadeddata = updateElementSize;
    } else {
      element.onload = updateElementSize;
    }
    element.tabIndex = -1;
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

    var domPicture = new camera.views.GalleryBase.DOMPicture(picture, wrapper);
    this.pictures.splice(index + 1, 0, domPicture);

    wrapper.addEventListener('mousedown', event => {
      event.preventDefault(); // Prevent focusing.
    });
    wrapper.addEventListener('click', event => {
      // If scrolled while clicking, then discard this selection, since another
      // one will be choosen in the onScrollEnded handler.
      if (this.scrollTracker_.scrolling &&
          Math.abs(this.scrollTracker_.delta[0]) > 16) {
        return;
      }
      this.setSelectedIndex(this.pictures.indexOf(domPicture));
    });
    wrapper.addEventListener('focus', () => {
      var index = this.pictures.indexOf(domPicture);
      if (this.lastSelectedIndex() != index)
        this.setSelectedIndex(index);
    });
  });
};

/**
 * Updates the picture element's size.
 * @param {HTMLElement} wrapper Element to be updated.
 * @private
 */
camera.views.Browser.updateElementSize_ = function(wrapper) {
  // Make the picture element not too large to overlap the buttons.
  var browserPadder = document.querySelector('#browser .padder');
  var maxWidth = browserPadder.clientWidth * 0.7;
  var maxHeight = browserPadder.clientHeight * 0.7;
  camera.util.updateElementSize(wrapper, maxWidth, maxHeight, false);
};

/**
 * @override
 */
camera.views.Browser.prototype.ariaListNode = function() {
  return document.querySelector('#browser');
};
