// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-picture-list' is a Polymer element used to show a selectable list of
 * profile pictures plus a camera selector, file picker, and the current
 * profile image.
 */

Polymer({
  is: 'cr-picture-list',

  properties: {
    cameraPresent: Boolean,

    /**
     * The default user images.
     * @type {!Array<!{title: string, url: string}>}
     */
    defaultImages: {
      type: Array,
      observer: 'onDefaultImagesChanged_',
    },

    /** Strings provided by host */
    chooseFileLabel: String,
    oldImageLabel: String,
    profileImageLabel: String,
    profileImageLoadingLabel: String,
    takePhotoLabel: String,

    /**
     * The currently selected item. This property is bound to the iron-selector
     * and never directly assigned. This may be undefined momentarily as
     * the selection changes due to iron-selector implementation details.
     * @private {?CrPicture.ImageElement}
     */
    selectedItem: {
      type: Object,
      notify: true,
    },

    /**
     * The url of the currently set profile picture image.
     * @private {string}
     */
    profileImageUrl_: {
      type: String,
      value: 'chrome://theme/IDR_LOGIN_DEFAULT_USER',
    },

    /**
     * The url of the old image, which is either the existing image sourced from
     * the camera, a file, or a deprecated default image.
     * @private {string}
     */
    oldImageUrl_: {
      type: String,
      value: '',
    },

    /** @private */
    selectionTypesEnum_: {
      type: Object,
      value: CrPicture.SelectionTypes,
      readOnly: true,
    },
  },

  /** @private {boolean} */
  cameraSelected_: false,

  /** @private {string} */
  selectedImageUrl_: '',

  /**
   * The fallback image to be selected when the user discards the 'old' image.
   * This may be null if the user started with the old image.
   * @private {?CrPicture.ImageElement}
   */
  fallbackImage_: null,

  setFocus: function() {
    this.$.container.focus();
  },

  /**
   * @param {string} imageUrl
   * @param {boolean} selected
   */
  setProfileImageUrl: function(imageUrl, selected) {
    this.profileImageUrl_ = imageUrl;
    this.$.profileImage.title = this.profileImageLabel;
    if (!selected)
      return;
    this.setSelectedImage_(this.$.profileImage);
  },

  /**
   * @param {string} imageUrl
   */
  setSelectedImageUrl(imageUrl) {
    var image = this.$.selector.items.find(function(image) {
      return image.src == imageUrl;
    });
    if (image) {
      this.setSelectedImage_(image);
      this.selectedImageUrl_ = '';
    } else {
      this.selectedImageUrl_ = imageUrl;
    }
  },

  /**
   * @param {string} imageUrl
   */
  setOldImageUrl(imageUrl) {
    this.oldImageUrl_ = imageUrl;
    if (imageUrl) {
      this.$.selector.select(this.$.selector.indexOf(this.$.oldImage));
      this.selectedImageUrl_ = imageUrl;
    } else if (this.cameraSelected_) {
      this.$.selector.select(this.$.selector.indexOf(this.$.cameraImage));
    } else if (this.fallbackImage_) {
      this.selectImage_(this.fallbackImage_, true /* activate */);
    }
  },

  /**
   * @param {!CrPicture.ImageElement} image
   */
  setSelectedImage_(image) {
    this.fallbackImage_ = image;
    // If the user is currently taking a photo, do not change the focus.
    if (!this.selectedItem ||
        this.selectedItem.dataset.type != CrPicture.SelectionTypes.CAMERA) {
      this.$.selector.select(this.$.selector.indexOf(image));
      this.selectedItem = image;
    }
  },

  /** @private */
  onDefaultImagesChanged_: function() {
    if (this.selectedImageUrl_)
      this.setSelectedImageUrl(this.selectedImageUrl_);
  },

  /**
   * Handler for when accessibility-specific keys are pressed.
   * @param {!{detail: !{key: string, keyboardEvent: Object}}} e
   */
  onKeysPressed_: function(e) {
    if (!this.selectedItem)
      return;

    var selector = /** @type {IronSelectorElement} */ (this.$.selector);
    var prevSelected = this.selectedItem;
    var activate = false;
    switch (e.detail.key) {
      case 'enter':
      case 'space':
        activate = true;
        break;
      case 'up':
      case 'left':
        do {
          selector.selectPrevious();
        } while (this.selectedItem.hidden && this.selectedItem != prevSelected);
        break;
      case 'down':
      case 'right':
        do {
          selector.selectNext();
        } while (this.selectedItem.hidden && this.selectedItem != prevSelected);
        break;
      default:
        return;
    }
    this.selectImage_(this.selectedItem, activate);
    e.detail.keyboardEvent.preventDefault();
  },

  /**
   * @param {!CrPicture.ImageElement} selected
   * @param {boolean} activate
   * @private
   */
  selectImage_(selected, activate) {
    this.cameraSelected_ =
        selected.dataset.type == CrPicture.SelectionTypes.CAMERA;
    this.selectedItem = selected;
    if (activate && selected.dataset.type == CrPicture.SelectionTypes.OLD)
      this.fire('discard-image');
    else if (activate || selected.dataset.type != CrPicture.SelectionTypes.FILE)
      this.fire('image-activate', selected);
  },

  /**
   * @param {!Event} event
   * @private
   */
  onIronActivate_: function(event) {
    var type = event.detail.item.dataset.type;
    // When clicking on the 'old' (current) image, do not activate (discard) it.
    var activate = type != CrPicture.SelectionTypes.OLD;
    this.selectImage_(event.detail.item, activate);
  },

  /**
   * Returns the 2x (high dpi) image to use for 'srcset'. Note: 'src' will still
   * be used as the 1x candidate as per the HTML spec.
   * @param {string} url
   * @return {string}
   * @private
   */
  getImgSrc2x_: function(url) {
    return url + '@2x 2x';
  },
});
