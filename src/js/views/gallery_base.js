// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
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
 * Creates the Gallery Base view controller.
 * @param {cca.Router} router View router to switch views.
 * @param {cca.models.Gallery} model Model object.
 * @param {HTMLElement} rootElement Root element of the view.
 * @param {string} name View name.
 * @extends {cca.View}
 * @implements {cca.models.Gallery.Observer}
 * @constructor
 */
cca.views.GalleryBase = function(router, model, rootElement, name) {
  cca.View.call(this, router, rootElement, name);

  /**
   * @type {cca.models.Gallery}
   * @private
   */
  this.model_ = model;

  /**
   * Contains pictures' views.
   * @type {Array.<cca.views.GalleryBase.DOMPicture>}
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
 * @param {cca.models.Gallery.Picture} picture Picture data.
 * @param {HTMLImageElement} element DOM element holding the picture.
 * @constructor
 */
cca.views.GalleryBase.DOMPicture = function(picture, element) {
  /**
   * @type {cca.models.Gallery.Picture}
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

cca.views.GalleryBase.DOMPicture.prototype = {
  get picture() {
    return this.picture_;
  },
  get element() {
    return this.element_;
  },
};

cca.views.GalleryBase.prototype = {
  __proto__: cca.View.prototype,
};

/**
 * Exports the selected pictures. If nothing selected, then nothing happens.
 * @protected
 */
cca.views.GalleryBase.prototype.exportSelection = function() {
  var selectedIndexes = this.selectedIndexes;
  if (!selectedIndexes.length)
    return;

  chrome.fileSystem.chooseEntry({type: 'openDirectory'}, dirEntry => {
    if (!dirEntry)
      return;

    this.selectedPictures().forEach(domPicture => {
      var picture = domPicture.picture;
      // TODO(yuli): Use FileSystem.getFile_ to handle name conflicts.
      dirEntry.getFile(
          cca.models.FileSystem.regulatePictureName(picture.pictureEntry),
          {create: true, exclusive: false}, entry => {
        this.model_.exportPicture(picture, entry).catch(error => {
          console.error(error);
          cca.toast.show(chrome.i18n.getMessage(
              'errorMsgGalleryExportFailed', entry.name));
        });
      });
    });
  });
};

/**
 * Deletes the currently selected pictures. If nothing selected, then nothing
 * happens.
 * @protected
 */
cca.views.GalleryBase.prototype.deleteSelection = function() {
  var selectedIndexes = this.selectedIndexes;
  if (!selectedIndexes.length)
    return;

  var multi = selectedIndexes.length > 1;
  var param = multi ? selectedIndexes.length.toString() :
      this.lastSelectedPicture().picture.pictureEntry.name;
  this.router.navigate(cca.Router.ViewIdentifier.DIALOG, {
    type: cca.views.Dialog.Type.CONFIRMATION,
    message: chrome.i18n.getMessage(multi ?
        'deleteMultiConfirmationMsg' : 'deleteConfirmationMsg', param),
  }, result => {
    if (!result.isPositive)
      return;
    var selectedPictures = this.selectedPictures();
    for (var i = selectedPictures.length - 1; i >= 0; i--) {
      this.model_.deletePicture(selectedPictures[i].picture).catch(error => {
        console.error(error);
        // TODO(yuli): Move Toast out of views/ and show a toast message here.
      });
    }
  });
};

/**
 * Returns the last selected picture index from the current selections.
 * @return {?number}
 * @protected
 */
cca.views.GalleryBase.prototype.lastSelectedIndex = function() {
  var selectedIndexes = this.selectedIndexes;
  if (!selectedIndexes.length)
    return null;

  return selectedIndexes[selectedIndexes.length - 1];
};

/**
 * Returns the last selected picture from the current selections.
 * @return {cca.views.GalleryBase.DOMPicture}
 * @protected
 */
cca.views.GalleryBase.prototype.lastSelectedPicture = function() {
  var leadIndex = this.lastSelectedIndex();
  return (leadIndex !== null) ? this.pictures[leadIndex] : null;
};

/**
 * Returns the currently selected picture views sorted in the added order.
 * @return {Array.<cca.views.GalleryBase.DOMPicture>}
 * @protected
 */
cca.views.GalleryBase.prototype.selectedPictures = function() {
  var indexes = this.selectedIndexes.slice();
  indexes.sort((a, b) => a - b);
  return indexes.map((i) => this.pictures[i]);
};

/**
 * Returns the picture's index in the picture views.
 * @param {cca.models.Gallery.Picture} picture Picture to be indexed.
 * @return {?number}
 * @protected
 */
cca.views.GalleryBase.prototype.pictureIndex = function(picture) {
  for (var index = 0; index < this.pictures.length; index++) {
    if (this.pictures[index].picture == picture)
      return index;
  }
  return null;
};

/**
 * @override
 */
cca.views.GalleryBase.prototype.onActivate = function() {
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
cca.views.GalleryBase.prototype.setPictureSelected = function(index) {
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
cca.views.GalleryBase.prototype.setPictureUnselected = function(index) {
  this.pictures[index].element.tabIndex = -1;
  this.pictures[index].element.classList.remove('selected');
  this.pictures[index].element.setAttribute('aria-selected', 'false');
};

/**
 * Sets the selected index.
 * @param {number} index Index of the picture to be selected.
 * @protected
 */
cca.views.GalleryBase.prototype.setSelectedIndex = function(index) {
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
cca.views.GalleryBase.prototype.onPictureDeleted = function(picture) {
  var index = this.pictureIndex(picture);
  if (index == null)
    return;

  // Hack to restore focus after removing an element. Note, that we restore
  // focus only if there was something focused before. However, if the focus
  // was on the selected element, then after removing it from DOM, there will
  // be nothing focused, while we still want to restore the focus.
  var element = this.pictures[index].element;
  this.forceUpcomingFocusSync_ = document.activeElement == element;
  element.parentNode.removeChild(element);
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
  if (!this.selectedIndexes.length) {
    if (this.pictures.length > 0) {
      this.setSelectedIndex(Math.max(0, index - 1));
    } else {
      this.setSelectedIndex(null);
      if (this.entered) {
        this.router.back();
      }
    }
  }
};

/**
 * @override
 */
cca.views.GalleryBase.prototype.onKeyPressed = function(event) {
  switch (cca.util.getShortcutIdentifier(event)) {
    case 'Delete':
    case 'Meta-Backspace':
      this.deleteSelection();
      event.preventDefault();
      break;
    case 'Escape':
      this.router.back();
      event.preventDefault();
      break;
    case 'Ctrl-S': // Ctrl+S for saving.
      this.exportSelection();
      event.preventDefault();
      break;
    case 'Ctrl-P': // Ctrl+P for printing.
      window.print();
      event.preventDefault();
      break;
  }
};

/**
 * @override
 */
cca.views.GalleryBase.prototype.onPictureAdded = function(picture) {
  this.addPictureToDOM(picture);
};

/**
 * Adds the picture to DOM. Should be overriden by inheriting classes.
 * @param {cca.models.Gallery.Picture} picture Model's picture to be added.
 * @protected
 */
cca.views.GalleryBase.prototype.addPictureToDOM = function(picture) {
  throw new Error('Not implemented.');
};

/**
 * Provides node for the picture list to be used to set list aria attributes.
 * @return {HTMLElement}
 * @protected
 */
cca.views.GalleryBase.prototype.ariaListNode = function() {
  throw new Error('Not implemented.');
};

/**
 * Synchronizes focus with the selection, if it was previously set on anything
 * else.
 * @protected
 */
cca.views.GalleryBase.prototype.synchronizeFocus = function() {
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
