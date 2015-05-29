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
 * Creates the Album view controller.
 *
 * @param {camera.View.Context} context Context object.
 * @param {camera.Router} router View router to switch views.
 * @extends {camera.views.GalleryBase}
 * @constructor
 */
camera.views.Album = function(context, router) {
  camera.views.GalleryBase.call(
      this, context, router, document.querySelector('#album'), 'album');

  /**
   * @type {camera.util.SmoothScroller}
   * @private
   */
  this.scroller_ = new camera.util.SmoothScroller(
      document.querySelector('#album'),
      document.querySelector('#album .padder'));

  /**
   * Makes the album scrollable by dragging with mouse.
   * @type {camera.util.MouseScroller}
   * @private
   */
  this.mouseScroller_ = new camera.util.MouseScroller(this.scroller_);

  /**
   * @type {camera.VerticalScrollBar}
   * @private
   */
  this.scrollBar_ = new camera.VerticalScrollBar(this.scroller_);

  /**
   * Sequential index of the last inserted picture. Used to generate unique
   * identifiers.
   *
   * @type {number}
   * @private
   */
  this.lastPictureIndex_ = 0;

  /**
   * Detects whether scrolling is being performed or not.
   * @type {camera.util.ScrollTracker}
   * @private
   */
  this.scrollTracker_ = new camera.util.ScrollTracker(
      this.scroller_, function() {}, this.onScrollEnded_.bind(this));

  // End of properties, seal the object.
  Object.seal(this);

  // Listen for clicking on the delete button.
  document.querySelector('#album-print').addEventListener(
      'click', this.onPrintButtonClicked_.bind(this));
  document.querySelector('#album-export').addEventListener(
      'click', this.onExportButtonClicked_.bind(this));
  document.querySelector('#album-delete').addEventListener(
      'click', this.onDeleteButtonClicked_.bind(this));
  document.querySelector('#album-back').addEventListener(
      'click', this.router.back.bind(this.router));
};

camera.views.Album.prototype = {
  __proto__: camera.views.GalleryBase.prototype
};

/**
 * Enters the view.
 * @override
 */
camera.views.Album.prototype.onEnter = function() {
  this.onResize();
  this.updateButtons_();
  this.scrollTracker_.start();
};

/**
 * Leaves the view.
 * @override
 */
camera.views.Album.prototype.onLeave = function() {
  this.scrollTracker_.stop();
};

/**
 * @override
 */
camera.views.Album.prototype.onActivate = function() {
  camera.views.GalleryBase.prototype.onActivate.apply(this, arguments);
  if (this.model.currentIndex === null && this.model.length)
    this.model.currentIndex = this.model.length - 1;

  // Update the focus. If scrolling, then it will be updated once scrolling
  // is finished in onScrollEnd().
  if (!this.scrollTracker_.scrolling && !this.scroller_.animating)
    this.synchronizeFocus();
};

/**
 * @override
 */
camera.views.Album.prototype.onResize = function() {
  if (this.currentPicture()) {
    camera.util.ensureVisible(this.currentPicture().element,
                              this.scroller_,
                              camera.util.SmoothScroller.Mode.INSTANT);
  }
};

/**
 * Handles clicking on the print button.
 * @param {Event} event Click event.
 * @private
 */
camera.views.Album.prototype.onPrintButtonClicked_ = function(event) {
  window.print();
};

/**
 * Handles clicking on the export button.
 * @param {Event} event Click event.
 * @private
 */
camera.views.Album.prototype.onExportButtonClicked_ = function(event) {
  this.exportSelection();
};

/**
 * Handles clicking on the delete button.
 * @param {Event} event Click event.
 * @private
 */
camera.views.Album.prototype.onDeleteButtonClicked_ = function(event) {
  this.deleteSelection();
};

/**
 * Updates visibility of the album buttons.
 * @private
 */
camera.views.Album.prototype.updateButtons_ = function() {
  var pictureSelected = this.model.currentIndex !== null;
  if (pictureSelected) {
    document.querySelector('#album-print').removeAttribute('disabled');
    document.querySelector('#album-export').removeAttribute('disabled');
    document.querySelector('#album-delete').removeAttribute('disabled');
  } else {
    document.querySelector('#album-print').setAttribute('disabled', '');
    document.querySelector('#album-export').setAttribute('disabled', '');
    document.querySelector('#album-delete').setAttribute('disabled', '');
  }
};

/**
 * Opens the current picture in the browser view.
 * @private
 */
camera.views.Album.prototype.openCurrentInBrowser_ = function() {
  this.router.navigate(
      camera.Router.ViewIdentifier.BROWSER,
      undefined /* opt_arguments */,
      function() {
        if (this.entered && !this.currentPicture())
          this.router.back();
      }.bind(this));
};

/**
 * @override
 */
camera.views.Album.prototype.onCurrentIndexChanged = function(
    oldIndex, newIndex) {
  camera.views.GalleryBase.prototype.onCurrentIndexChanged.apply(
      this, arguments);
  if (newIndex !== null) {
    camera.util.ensureVisible(this.currentPicture().element,
                              this.scroller_);
  }

  // Update visibility of the album buttons.
  this.updateButtons_();

  // Synchronize focus with the selected item immediately if not scrolling.
  // Otherwise, synchronization will be invoked from onScrollEnd().
  if (!this.scrollTracker_.scrolling && !this.scroller_.animating)
    this.synchronizeFocus();
};

/**
 * @override
 */
camera.views.Album.prototype.onKeyPressed = function(event) {
  var currentPicture = this.currentPicture();
  switch (camera.util.getShortcutIdentifier(event)) {
    case 'Down':
      if (this.model.length) {
        if (!currentPicture) {
          this.model.currentIndex = this.model.length - 1;
        } else {
          for (var index = this.model.currentIndex - 1; index >= 0; index--) {
            if (currentPicture.element.offsetLeft ==
                this.pictures[index].element.offsetLeft) {
              this.model.currentIndex = index;
              break;
            }
          }
        }
      }
      event.preventDefault();
      return;
    case 'Up':
      if (this.model.length) {
        if (!currentPicture) {
          this.model.currentIndex = 0;
        } else {
          for (var index = this.model.currentIndex + 1;
               index < this.model.length;
               index++) {
            if (currentPicture.element.offsetLeft ==
                this.pictures[index].element.offsetLeft) {
              this.model.currentIndex = index;
              break;
            }
          }
        }
      }
      event.preventDefault();
      return;
    case 'Enter':
      // Do not handle enter, if other elements (such as buttons) are focused.
      if (document.activeElement != document.body &&
          !document.querySelector('#album .padder').contains(
              document.activeElement)) {
        return;
      }
      if (currentPicture)
        this.openCurrentInBrowser_();
      event.preventDefault();
      return;
    case 'Ctrl-U+0053':  // Ctrl+S for saving.
      this.exportSelection();
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
 * Handles end of scrolling.
 * @private
 */
camera.views.Album.prototype.onScrollEnded_ = function() {
  this.synchronizeFocus();
};

/**
 * @override
 */
camera.views.Album.prototype.addPictureToDOM = function(picture) {
  var album = document.querySelector('#album .padder');
  var img = document.createElement('img');
  img.id = 'album-picture-' + (this.lastPictureIndex_++);
  img.tabIndex = -1;
  img.setAttribute('aria-role', 'option');
  img.setAttribute('aria-selected', 'false');
  album.insertBefore(img, album.firstChild);

  // Add to the collection.
  this.pictures.push(new camera.views.GalleryBase.DOMPicture(picture, img));

  // Add handlers.
  img.addEventListener('mousedown', function(event) {
    event.preventDefault();  // Prevent focusing.
  });
  img.addEventListener('click', function() {
    this.model.currentIndex = this.model.pictures.indexOf(picture);
  }.bind(this));
  img.addEventListener('focus', function() {
    this.model.currentIndex = this.model.pictures.indexOf(picture);
  }.bind(this));

  img.addEventListener('dblclick', function() {
    this.openCurrentInBrowser_();
  }.bind(this));
};

/**
 * @override
 */
camera.views.Album.prototype.ariaListNode = function() {
  return document.querySelector('#album');
};

