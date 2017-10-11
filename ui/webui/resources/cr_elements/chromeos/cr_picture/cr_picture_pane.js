// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-picture-pane' is a Polymer element used to show either a profile
 * picture or a camera image preview.
 */

Polymer({
  is: 'cr-picture-pane',

  properties: {
    /** Whether the camera is present / available */
    cameraPresent: Boolean,

    /** Whether video mode is allowed */
    allowVideoMode: Boolean,

    /** Image source to show when imageType != CAMERA. */
    imageSrc: String,

    /**
     * The type of image to display in the preview.
     * @type {CrPicture.SelectionTypes}
     */
    imageType: {
      type: String,
      value: CrPicture.SelectionTypes.NONE,
    },

    /** Strings provided by host */
    discardImageLabel: String,
    previewAltText: String,
    takePhotoLabel: String,
    switchModeLabel: String,

    /** Whether the camera should be shown and active (started). */
    cameraActive_: {
      type: Boolean,
      computed: 'getCameraActive_(cameraPresent, imageType)',
      observer: 'cameraActiveChanged_',
    },
  },

  /**
   * Tells the camera to take a photo; the camera will fire a 'photo-taken'
   * event when the photo is completed.
   */
  takePhoto: function() {
    var camera = /** @type {?CrCameraElement} */ (this.$$('#camera'));
    if (camera)
      camera.takePhoto();
  },

  /**
   * @return {boolean}
   * @private
   */
  getCameraActive_() {
    return this.cameraPresent &&
        this.imageType == CrPicture.SelectionTypes.CAMERA;
  },

  /** @private */
  cameraActiveChanged_: function() {
    var camera = /** @type {?CrCameraElement} */ (this.$$('#camera'));
    if (!camera)
      return;  // Camera will be started when attached.
    if (this.cameraActive_)
      camera.startCamera();
    else
      camera.stopCamera();
  },

  /**
   * @return {boolean}
   * @private
   */
  showImagePreview_: function() {
    return !this.cameraActive_ && !!this.imageSrc;
  },

  /**
   * @return {boolean}
   * @private
   */
  showDiscard_: function() {
    return this.imageType == CrPicture.SelectionTypes.OLD;
  },

  /** @private */
  onTapDiscardImage_: function() {
    this.fire('discard-image');
  },

  /**
   * Returns the image to use for 'src'.
   * @param {string} url
   * @return {string}
   * @private
   */
  getImgSrc_: function(url) {
    // Always use 2x user image for preview.
    if (url.startsWith('chrome://theme'))
      return url + '@2x';

    return url;
  },
});
