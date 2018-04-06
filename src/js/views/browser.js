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

  /**
   * @type {?function()}
   * @private
   */
  this.selectionChanged_ = null;

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
 * Enters the view. Assumes, that the arguments may be provided.
 * @param {Object=} opt_arguments Arguments for the browser.
 * @override
 */
camera.views.Browser.prototype.onEnter = function(opt_arguments) {
  var index = null;
  if (opt_arguments) {
    if (opt_arguments.picture)
      index = this.pictureIndex(opt_arguments.picture);
    if (opt_arguments.selectionChanged)
      this.selectionChanged_ = opt_arguments.selectionChanged;
  }
  // Navigate to the newest picture if no arguments are provided.
  if (index == null && this.pictures.length)
    index = this.pictures.length - 1;
  this.setSelectedIndex(index);

  this.onResize();
  this.scrollTracker_.start();
  document.body.classList.add('browser');
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
 * Leaves the view.
 * @override
 */
camera.views.Browser.prototype.onLeave = function() {
  this.scrollTracker_.stop();
  this.selectionChanged_ = null;
  this.setSelectedIndex(null);
};

/**
 * @override
 */
camera.views.Browser.prototype.onResize = function() {
  this.pictures.forEach(function(picture) {
    this.updateElementSize(picture.element);
  }.bind(this));

  this.scrollBar_.onResize();
  var selectedPicture = this.lastSelectedPicture();
  if (selectedPicture) {
    camera.util.scrollToCenter(selectedPicture.element,
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
  window.matchMedia('print').addListener(function(media) {
    if (!media.matches) {
      for (var index = 0; index < this.pictures.length; index++) {
        // Force the div wrappers to redraw by accessing their display and
        // offsetWidth property. Otherwise, they may not have the same width as
        // their child image or video element after printing.
        var wrapper = this.pictures[index].element;
        wrapper.style.display = 'none';
        wrapper.offsetWidth;  // Reference forces width recalculation.
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
 *
 * @private
 */
camera.views.Browser.prototype.updatePicturesResolutions_ = function() {
  var selectedPicture = this.lastSelectedPicture();

  var wrappedElement = function(wrapper, tagName) {
    return (wrapper.firstElementChild.tagName == tagName) ?
        wrapper.firstElementChild : null;
  };

  var updateImage = function(wrapper, url) {
    var img = wrappedElement(wrapper, 'IMG');
    if (!img) {
      // Remove the existing video element if any.
      img = document.createElement('img');
      img.tabIndex = -1;
      img.onload = function() {
        var video = wrappedElement(wrapper, 'VIDEO');
        if (video) {
          wrapper.replaceChild(img, video);
          this.updateElementSize(wrapper);
        }
      }.bind(this);
      img.src = url;
    } else if (img.src != url) {
      img.onload = function() {
        this.updateElementSize(wrapper);
      }.bind(this);
      img.src = url;
    }
  }.bind(this);

  var updateVideo = function(wrapper, url) {
    // Use a video element to play back the selected motion picture.
    var video = document.createElement('video');
    video.tabIndex = -1;
    video.controls = true;
    video.setAttribute('controlsList', 'nodownload nofullscreen');
    video.onloadeddata = function() {
      // Add the video element only if the selection has not been changed and
      // there is still the image element after loading.
      if (wrapper == this.lastSelectedPicture().element) {
        var img = wrappedElement(wrapper, 'IMG');
        if (img) {
          wrapper.replaceChild(video, img);
          this.updateElementSize(wrapper);
        }
      }
    }.bind(this);
    video.src = url;
  }.bind(this);

  var updateResolutions = function() {
    for (var index = 0; index < this.pictures.length; index++) {
      var wrapper = this.pictures[index].element;
      var picture = this.pictures[index].picture;
      if (this.pictures[index] == selectedPicture) {
        if (picture.pictureType == camera.models.Gallery.PictureType.MOTION) {
          updateVideo(wrapper, picture.pictureURL);
        } else {
          updateImage(wrapper, picture.pictureURL);
        }
      } else {
        // Show thumbnails for both unselected still and motion pictures.
        updateImage(wrapper, picture.thumbnailURL);
      }
    }
  }.bind(this);

  // Wait until the zoom-in transition is finished, and then update pictures'
  // resolutions.
  if (selectedPicture !== null) {
    camera.util.waitForTransitionCompletion(
        selectedPicture.element,
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
camera.views.Browser.prototype.setSelectedIndex = function(index) {
  camera.views.GalleryBase.prototype.setSelectedIndex.apply(this, arguments);

  var selectedPicture = this.lastSelectedPicture();
  if (selectedPicture) {
    camera.util.scrollToCenter(selectedPicture.element, this.scroller_);
  }
  if (this.selectionChanged_) {
    this.selectionChanged_(selectedPicture ? selectedPicture.picture : null);
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
camera.views.Browser.prototype.onPictureDeleting = function(picture) {
  camera.views.GalleryBase.prototype.onPictureDeleting.apply(this, arguments);
  this.updateScrollbarThumb_();
};

/**
 * @override
 */
camera.views.Browser.prototype.addPictureToDOM = function(picture) {
  var browser = document.querySelector('#browser .padder');
  var boundsPadder = browser.querySelector('.bounds-padder');
  var wrapper = document.createElement('div');
  wrapper.className = 'media-wrapper';
  wrapper.id = 'browser-picture-' + (this.lastPictureIndex_++);
  wrapper.tabIndex = -1;
  wrapper.setAttribute('aria-role', 'option');
  wrapper.setAttribute('aria-selected', 'false');

  var img = document.createElement('img');
  img.tabIndex = -1;
  img.onload = function() {
    this.updateElementSize(wrapper);
  }.bind(this);
  img.src = picture.thumbnailURL;

  wrapper.appendChild(img);
  browser.insertBefore(wrapper, boundsPadder.nextSibling);

  // Add to the collection.
  var domPicture = new camera.views.GalleryBase.DOMPicture(picture, wrapper);
  this.pictures.push(domPicture);
  this.updateScrollbarThumb_();

  wrapper.addEventListener('mousedown', function(event) {
    event.preventDefault();  // Prevent focusing.
  });
  wrapper.addEventListener('click', function(event) {
    // If scrolled while clicking, then discard this selection, since another
    // one will be choosen in the onScrollEnded handler.
    if (this.scrollTracker_.scrolling &&
        Math.abs(this.scrollTracker_.delta[0]) > 16) {
      return;
    }

    this.setSelectedIndex(this.pictures.indexOf(domPicture));
  }.bind(this));
  wrapper.addEventListener('focus', function() {
    var index = this.pictures.indexOf(domPicture);
    if (this.lastSelectedIndex() != index)
      this.setSelectedIndex(index);
  }.bind(this));
};

/**
 * @override
 */
camera.views.Browser.prototype.updateElementSize = function(wrapper) {
  // Set the picture element's max dimension (not larger than the window size).
  // Scaling up the selected element is still done in css rather than here as
  // calculating the element size after loading the high-resolution content
  // makes DOM tree relayout and unsmooth UI experience.
  // TODO(yuli): Fix the blurriness in CSS scaled-up content.
  var maxWidth = wrapper.parentElement.clientWidth * 0.72;
  var maxHeight = wrapper.parentElement.clientHeight * 0.72;

  camera.views.GalleryBase.prototype.updateElementSize.call(
      this, wrapper, maxWidth, maxHeight, false);
};

/**
 * @override
 */
camera.views.Browser.prototype.ariaListNode = function() {
  return document.querySelector('#browser');
};

