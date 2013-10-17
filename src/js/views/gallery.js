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
 * Creates the Gallery view controller.
 *
 * @param {camera.View.Context} context Context object.
 * @param {camera.Router} router View router to switch views.
 * @extends {camera.View}
 * @implements {camera.models.Gallery.Observer}
 * @constructor
 */
camera.views.Gallery = function(context, router) {
  camera.View.call(this, context, router);

  /**
   * @type {camera.models.Gallery}
   * @private
   */
  this.model_ = null;

  /**
   * Contains pictures' views.
   * @type {Array.<camera.views.Gallery.DOMPicture>}
   * @private
   */
  this.pictures_ = [];

  /**
   * @type {camera.util.SmoothScroller}
   * @private
   */
  this.scroller_ = new camera.util.SmoothScroller(
      document.querySelector('#gallery'),
      document.querySelector('#gallery .padder'));

  /**
   * @type {camera.VerticalScrollBar}
   * @private
   */
  this.scrollBar_ = new camera.VerticalScrollBar(this.scroller_);

  // End of properties, seal the object.
  Object.seal(this);

  // Listen for clicking on the delete button.
  document.querySelector('#gallery-delete').addEventListener(
      'click', this.onDeleteButtonClicked_.bind(this));
};

/**
 * Represents a picture attached to the DOM by combining the picture data
 * object with the DOM element.
 *
 * @param {camera.models.Gallery.Picture} picture Picture data.
 * @param {HTMLImageElement} element DOM element holding the picture.
 * @constructor
 */
camera.views.Gallery.DOMPicture = function(picture, element) {
  /**
   * @type {camera.models.Gallery.Picture}
   * @private
   */
  this.picture_ = picture;

  /**
   * @type {HTMLElement}
   * @private
   */
  this.element_ = element;

  /**
   * @type {camera.views.Gallery.DOMPicture.DisplayResolution}
   * @private
   */
  this.displayResolution_ =
      camera.views.Gallery.DOMPicture.DisplayResolution.LOW;

  // End of properties. Seal the object.
  Object.seal(this);

  // Load the image.
  this.updateImage_();
};

/**
 * Sets resolution of the picture to be displayed.
 * @enum {number}
 */
camera.views.Gallery.DOMPicture.DisplayResolution = {
  LOW: 0,
  HIGH: 1
};

camera.views.Gallery.DOMPicture.prototype = {
  get picture() {
    return this.picture_;
  },
  get element() {
    return this.element_;
  },
  set displayResolution(value) {
    if (this.displayResolution_ == value)
      return;
    this.displayResolution_ = value;
    this.updateImage_();
  },
  get displayResolution() {
    return this.displayResolution_;
  }
};

/**
 * Loads the picture into the DOM element.
 * @private
 */
camera.views.Gallery.DOMPicture.prototype.updateImage_ = function() {
  switch (this.displayResolution_) {
    case camera.views.Gallery.DOMPicture.DisplayResolution.LOW:
      this.element_.src = this.picture_.thumbnailURL;
      break;
    case camera.views.Gallery.DOMPicture.DisplayResolution.HIGH:
      this.element_.src = this.picture_.imageURL;
      break;
  }
};

camera.views.Gallery.prototype = {
  __proto__: camera.View.prototype
};

/**
 * @override
 */
camera.views.Gallery.prototype.initialize = function(callback) {
  camera.models.Gallery.getInstance(function(model) {
    this.model_ = model;
    this.model_.addObserver(this);
    this.renderPictures_();
    callback();
  }.bind(this), function() {
    // TODO(mtomasz): Add error handling.
    console.error('Unable to initialize the file system.');
    callback();
  });
};

/**
 * Renders pictures from the model onto the DOM.
 * @private
 */
camera.views.Gallery.prototype.renderPictures_ = function() {
  for (var index = 0; index < this.model_.length; index++) {
    this.addPictureToDOM_(this.model_.pictures[index]);
  }
}

/**
 * Enters the view.
 * @override
 */
camera.views.Gallery.prototype.onEnter = function() {
  this.onResize();
  this.updateButtons_();
  document.body.classList.add('gallery');
};

/**
 * Leaves the view.
 * @override
 */
camera.views.Gallery.prototype.onLeave = function() {
  document.body.classList.remove('gallery');
};

/**
 * @override
 */
camera.views.Gallery.prototype.onResize = function() {
  if (this.currentPicture_()) {
    camera.util.ensureVisible(this.currentPicture_().element,
                              this.scroller_,
                              camera.util.SmoothScroller.Mode.INSTANT);
  }
};

/**
 * Handles clicking on the delete button.
 * @param {Event} event Click event.
 * @private
 */
camera.views.Gallery.prototype.onDeleteButtonClicked_ = function(event) {
  this.deleteSelection_();
};

/**
 * Deletes the currently selected picture. If nothing selected, then nothing
 * happens.
 * @private
 */
camera.views.Gallery.prototype.deleteSelection_ = function() {
  if (!this.currentPicture_())
    return;

  this.model_.deletePicture(this.currentPicture_().picture,
      function() {},
      function() {
        // TODO(mtomasz): Handle errors.
      });
};

/**
 * Returns a picture view by index.
 *
 * @param {number} index Index of the picture.
 * @return {camera.views.Gallery.Picture}
 * @private
 */
camera.views.Gallery.prototype.pictureByIndex_ = function(index) {
  return this.pictures_[index];
};

/**
 * Returns the currently selected picture view.
 *
 * @return {camera.views.Gallery.Picture}
 * @private
 */
camera.views.Gallery.prototype.currentPicture_ = function() {
  return this.pictureByIndex_(this.model_.currentIndex);
};

/**
 * Updates visibility of the gallery buttons.
 * @private
 */
camera.views.Gallery.prototype.updateButtons_ = function() {
  var pictureSelected = this.model_.currentIndex !== null;
  if (pictureSelected)
    document.querySelector('#gallery-delete').removeAttribute('disabled');
  else
    document.querySelector('#gallery-delete').setAttribute('disabled', '');
};

/**
 * @override
 */
camera.views.Gallery.prototype.onCurrentIndexChanged = function(
    oldIndex, newIndex) {
  if (oldIndex !== null && oldIndex < this.model_.length)
    this.pictureByIndex_(oldIndex).element.classList.remove('selected');
  if (newIndex !== null && newIndex < this.model_.length)
    this.pictureByIndex_(newIndex).element.classList.add('selected');

  if (newIndex !== null) {
    camera.util.ensureVisible(this.currentPicture_().element,
                              this.scroller_);
  }

  // Update visibility of the gallery buttons.
  this.updateButtons_();
};

/**
 * @override
 */
camera.views.Gallery.prototype.onPictureDeleting = function(index) {
  this.pictures_[index].element.parentNode.removeChild(
    this.pictures_[index].element);
  this.pictures_.splice(index, 1);
};

/**
 * @override
 */
camera.views.Gallery.prototype.onKeyPressed = function(event) {
  var currentPicture = this.currentPicture_();
  switch (event.keyIdentifier) {
    case 'Right':
      this.model_.currentIndex = Math.max(0, this.model_.currentIndex - 1);
      break;
    case 'Left':
      this.model_.currentIndex =
          Math.min(this.model_.length - 1, this.model_.currentIndex + 1);
      break;
    case 'Down':
      for (var index = this.model_.currentIndex - 1; index >= 0; index--) {
        if (currentPicture.element.offsetLeft ==
            this.pictures_[index].element.offsetLeft) {
          this.model_.currentIndex = index;
          break;
        }
      }
      event.preventDefault();
      break;
    case 'Up':
      for (var index = this.model_.currentIndex + 1;
           index < this.model_.length;
           index++) {
        if (currentPicture.element.offsetLeft ==
            this.pictures_[index].element.offsetLeft) {
          this.model_.currentIndex = index;
          break;
        }
      }
      event.preventDefault();
      break;
    case 'End':
      this.model_.currentIndex = 0;
      break;
    case 'Home':
      this.model_.currentIndex = this.model_.length - 1;
      break;
    case 'U+007F':
      this.deleteSelection_();
      break;
    case 'Enter':
      if (this.model_.length)
        this.router.switchView(camera.Router.ViewIdentifier.BROWSER);
      break;
    case 'U+001B':
      this.router.switchView(camera.Router.ViewIdentifier.CAMERA);
      break;
  }
};

/**
 * @override
 */
camera.views.Gallery.prototype.onPictureAdded = function(index) {
  this.addPictureToDOM_(this.model_.pictures[index]);
};

/**
 * Adds a picture represented by a model to DOM, and stores in the internal
 * mapping hash array (model -> view).
 *
 * @param {camera.model.Gallery.Picture} picture
 * @private
 */
camera.views.Gallery.prototype.addPictureToDOM_ = function(picture) {
  var gallery = document.querySelector('#gallery .padder');
  var img = document.createElement('img');
  gallery.insertBefore(img, gallery.firstChild);

  // Add to the collection.
  this.pictures_.push(new camera.views.Gallery.DOMPicture(picture, img));

  // Add handlers.
  img.addEventListener('click', function() {
    this.model_.currentIndex = this.model_.pictures.indexOf(picture);
  }.bind(this));

  img.addEventListener('dblclick', function() {
    this.router.switchView(camera.Router.ViewIdentifier.BROWSER);
  }.bind(this));
};

