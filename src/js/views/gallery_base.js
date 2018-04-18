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
 * Creates the Gallery Base view controller.
 *
 * @param {camera.View.Context} context Context object.
 * @param {camera.Router} router View router to switch views.
 * @param {HTMLElement} rootElement Root element containing elements of the
 *     view.
 * @param {string} name View name.
 * @extends {camera.View}
 * @implements {camera.models.Gallery.Observer}
 * @constructor
 */
camera.views.GalleryBase = function(context, router, rootElement, name) {
  camera.View.call(this, context, router, rootElement, name);

  /**
   * @type {camera.models.Gallery}
   * @protected
   */
  this.model = null;

  /**
   * Contains pictures' views.
   * @type {Array.<camera.views.GalleryBase.DOMPicture>}
   * @protected
   */
  this.pictures = [];

  /**
   * Contains selected pictures' indexes sorted in the selection order.
   * @type {Array.<number>}
   * @protected
   */
  this.selectedIndexes = [];

  /**
   * @type {boolean}
   * @private
   */
  this.forceUpcomingFocusSync_ = false;

};

/**
 * Represents a picture attached to the DOM by combining the picture data
 * object with the DOM element.
 *
 * @param {camera.models.Gallery.Picture} picture Picture data.
 * @param {HTMLImageElement} element DOM element holding the picture.
 * @constructor
 */
camera.views.GalleryBase.DOMPicture = function(picture, element) {
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

  // End of properties. Seal the object.
  Object.seal(this);
};

camera.views.GalleryBase.DOMPicture.prototype = {
  get picture() {
    return this.picture_;
  },
  get element() {
    return this.element_;
  }
};

camera.views.GalleryBase.prototype = {
  __proto__: camera.View.prototype
};

/**
 * Initializes the view. Call the callback on completion.
 * @param {function()} callback Completion callback.
 */
camera.views.GalleryBase.prototype.initialize = function(callback) {
  camera.models.Gallery.getInstance(function(model) {
    this.model = model;
    this.model.addObserver(this);
    this.renderPictures_();
    callback();
  }.bind(this), function() {
    // TODO(yuli): Add error handling.
    callback();
  });
};

/**
 * Renders pictures from the model onto the DOM.
 * @private
 */
camera.views.GalleryBase.prototype.renderPictures_ = function() {
  for (var index = 0; index < this.model.length; index++) {
    this.addPictureToDOM(this.model.pictures[index]);
  }
};

/**
 * Exports the selected pictures. If nothing selected, then nothing happens.
 * @protected
 */
camera.views.GalleryBase.prototype.exportSelection = function() {
  var selectedIndexes = this.selectedIndexes;
  if (!selectedIndexes.length)
    return;

  var onError = function(filename) {
    // TODO(yuli): Show a non-intrusive toast message instead.
    this.context_.onError(
        'gallery-export-error',
        chrome.i18n.getMessage('errorMsgGalleryExportFailed', filename));
    setTimeout(function() {
      this.context_.onErrorRecovered('gallery-export-error');
    }.bind(this), 2000);
  }.bind(this);

  var exportPicture = function(fileEntry, picture) {
    this.model.exportPicture(
        picture,
        fileEntry,
        function() {},
        function() { onError(fileEntry.name); });
  }.bind(this);

  chrome.fileSystem.chooseEntry({
    type: 'openDirectory'
  }, function(dirEntry) {
    if (!dirEntry)
      return;

    var savePictureAsFile = function(picture) {
      dirEntry.getFile(picture.pictureEntry.name, {
        create: true, exclusive: false
      }, function(fileEntry) {
          exportPicture(fileEntry, picture);
      });
    };

    var selectedPictures = this.selectedPictures();
    for (var i = 0; i < selectedPictures.length; i++) {
      savePictureAsFile(selectedPictures[i].picture);
    }
  }.bind(this));
};

/**
 * Deletes the currently selected pictures. If nothing selected, then nothing
 * happens.
 * @protected
 */
camera.views.GalleryBase.prototype.deleteSelection = function() {
  var selectedIndexes = this.selectedIndexes;
  if (!selectedIndexes.length)
    return;

  var multi = selectedIndexes.length > 1;
  var param = multi ? selectedIndexes.length.toString() :
      this.lastSelectedPicture().picture.pictureEntry.name;
  this.router.navigate(camera.Router.ViewIdentifier.DIALOG, {
    type: camera.views.Dialog.Type.CONFIRMATION,
    message: chrome.i18n.getMessage(multi ?
        'deleteMultiConfirmationMsg' : 'deleteConfirmationMsg', param)
  }, function(result) {
    if (!result.isPositive)
      return;
    var selectedPictures = this.selectedPictures();
    var lastDeleteIndex = this.pictures.indexOf(selectedPictures[0]);
    for (var i = selectedPictures.length - 1; i >= 0; i--) {
      this.model.deletePicture(selectedPictures[i].picture,
          function() {
            // Update the selection after all selected pictures are deleted.
            if (!this.selectedIndexes.length) {
              if (this.pictures.length > 0) {
                this.setSelectedIndex(Math.max(0, lastDeleteIndex - 1));
              }
              else {
                this.setSelectedIndex(null);
                if (this.entered)
                  this.router.back();
              }
            }
          }.bind(this),
          function() {
            // TODO(mtomasz): Handle errors.
          });
    }
  }.bind(this));
};

/**
 * Returns the last selected picture index from the current selections.
 * @return {?number}
 * @protected
 */
camera.views.GalleryBase.prototype.lastSelectedIndex = function() {
  var selectedIndexes = this.selectedIndexes;
  if (!selectedIndexes.length)
    return null;

  return selectedIndexes[selectedIndexes.length - 1];
};

/**
 * Returns the last selected picture from the current selections.
 * @return {camera.views.GalleryBase.DOMPicture}
 * @protected
 */
camera.views.GalleryBase.prototype.lastSelectedPicture = function() {
  var leadIndex = this.lastSelectedIndex();
  return (leadIndex !== null) ? this.pictures[leadIndex] : null;
};

/**
 * Returns the currently selected picture views sorted in the added order.
 * @return {Array.<camera.views.GalleryBase.DOMPicture>}
 * @protected
 */
camera.views.GalleryBase.prototype.selectedPictures = function() {
  var indexes = this.selectedIndexes.slice();
  indexes.sort(function(a, b) { return a - b; });
  return indexes.map(function(i) { return this.pictures[i]; }.bind(this));
};

/**
 * Returns the picture's index in the picture views.
 * @param {camera.models.Gallery.Picture} picture Picture to be indexed.
 * @return {?number}
 * @protected
 */
camera.views.GalleryBase.prototype.pictureIndex = function(picture) {
  for (var index = 0; index < this.pictures.length; index++) {
    if (this.pictures[index].picture == picture)
      return index;
  }
  return null;
};

/**
 * @override
 */
camera.views.GalleryBase.prototype.onActivate = function() {
  // Tab indexes have to be recalculated, since they might have changed while
  // the view wasn't active. Therefore, the restoring logic in the View class
  // might have restored old tabIndex values.
  var selectedPicture = this.lastSelectedPicture();
  for (var index = 0; index < this.pictures.length; index++) {
    var picture = this.pictures[index];
    picture.element.tabIndex = (picture == selectedPicture) ? 0 : -1;
  }
};

/**
 * Sets the picture as selected.
 * @param {number} index Index of the picture to be selected.
 * @protected
 */
camera.views.GalleryBase.prototype.setPictureSelected = function(index) {
  this.pictures[index].element.tabIndex = this.active ? 0 : -1;
  this.pictures[index].element.classList.add('selected');
  this.pictures[index].element.setAttribute('aria-selected', 'true');

  this.ariaListNode().setAttribute('aria-activedescendant',
      this.pictures[index].element.id);
};

/**
 * Sets the picture as unselected.
 * @param {number} index Index of the picture to be unselected.
 * @protected
 */
camera.views.GalleryBase.prototype.setPictureUnselected = function(index) {
  this.pictures[index].element.tabIndex = -1;
  this.pictures[index].element.classList.remove('selected');
  this.pictures[index].element.setAttribute('aria-selected', 'false');
};

/**
 * Sets the selected index.
 * @param {number} index Index of the picture to be selected.
 * @protected
 */
camera.views.GalleryBase.prototype.setSelectedIndex = function(index) {
  var selectedIndexes = this.selectedIndexes;
  for (var i = 0; i < selectedIndexes.length; i++) {
    this.setPictureUnselected(selectedIndexes[i]);
  }
  selectedIndexes.splice(0, selectedIndexes.length);

  if (index !== null) {
    this.setPictureSelected(index);
    selectedIndexes.push(index);
  }
};

/**
 * @override
 */
camera.views.GalleryBase.prototype.onPictureDeleting = function(picture) {
  var index = this.pictureIndex(picture);
  if (index == null)
    return;

  var element = this.pictures[index].element;
  this.pictures.splice(index, 1);

  // Update the selection if the deleted picture is selected.
  var removal = this.selectedIndexes.indexOf(index);
  if (removal != -1) {
    this.selectedIndexes.splice(removal, 1);
    for (var i = 0; i < this.selectedIndexes.length; i++) {
      if (this.selectedIndexes[i] > index)
        this.selectedIndexes[i]--;
    }
  }

  // Hack to restore focus after removing an element. Note, that we restore
  // focus only if there was something focused before. However, if the focus
  // was on the selected element, then after removing it from DOM, there will
  // be nothing focused, while we still want to restore the focus.
  this.forceUpcomingFocusSync_ = document.activeElement == element;

  element.parentNode.removeChild(element);
};

/**
 * @override
 */
camera.views.GalleryBase.prototype.onKeyPressed = function(event) {
  switch (camera.util.getShortcutIdentifier(event)) {
    case 'Delete':
    case 'Meta-Backspace':
      this.deleteSelection();
      event.preventDefault();
      break;
    case 'Escape':
      this.router.back();
      event.preventDefault();
      break;
    case 'Ctrl-S':  // Ctrl+S for saving.
      this.exportSelection();
      event.preventDefault();
      break;
    case 'Ctrl-P':  // Ctrl+P for printing.
      window.print();
      event.preventDefault();
      break;
  }
};

/**
 * @override
 */
camera.views.GalleryBase.prototype.onPictureAdded = function(picture) {
  this.addPictureToDOM(picture);
};

/**
 * Adds the picture to DOM. Should be overriden by inheriting classes.
 * @param {camera.models.Gallery.Picture} picture Model of the picture to be
 *     added.
 * @protected
 */
camera.views.GalleryBase.prototype.addPictureToDOM = function(picture) {
  throw new Error('Not implemented.');
};

/**
 * Updates the picture's content element size according to the given bounds.
 * The wrapped content (either img or video child element) should keep the
 * aspect ratio and is either filled up or letterboxed inside the picture's
 * wrapper element.
 * @param {HTMLElement}  wrapper Picture element to be updated.
 * @param {number} boundWidth Bound width in pixels.
 * @param {number} boundHeight Bound height in pixels.
 * @param {boolean} fill True to fill up and crop the content to the bounds,
 *     false to letterbox the content within the bounds.
 * @protected
 */
camera.views.GalleryBase.prototype.updateElementSize = function(
    wrapper, boundWidth, boundHeight, fill) {
  // Assume the wrapped child is either img or video element.
  var child = wrapper.firstElementChild;
  var srcWidth = child.naturalWidth ? child.naturalWidth : child.videoWidth;
  var srcHeight = child.naturalHeight ? child.naturalHeight : child.videoHeight;
  var f = fill ? Math.max : Math.min;
  var scale = f(boundHeight / srcHeight, boundWidth / srcWidth);

  // Corresponding CSS should handle the adjusted sizes for proper display.
  child.width = scale * srcWidth;
  child.height = scale * srcHeight;
};

/**
 * Provides node for the picture list to be used to set list aria attributes.
 * @return {HTMLElement}
 * @protected
 */
camera.views.GalleryBase.prototype.ariaListNode = function() {
  throw new Error('Not implemented.');
};

/**
 * Synchronizes focus with the selection, if it was previously set on anything
 * else.
 * @protected
 */
camera.views.GalleryBase.prototype.synchronizeFocus = function() {
  // Force focusing only once, after deleting a picture. This is because, the
  // focus might be lost since the deleted picture is removed from DOM.
  var force = this.forceUpcomingFocusSync_;
  this.forceUpcomingFocusSync_ = false;

  // Synchronize focus on the last selected picture.
  var selectedPicture = this.lastSelectedPicture();
  if (selectedPicture && (document.activeElement != document.body || force) &&
      selectedPicture.element.getAttribute('tabindex') !== undefined &&
      selectedPicture.element.getAttribute('tabindex') != -1) {
    selectedPicture.element.focus();
  }
};
