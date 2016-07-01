// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var FilesMetadataBox = Polymer({
  is: 'files-metadata-box',

  properties: {
    // File media type, e.g. image, video.
    type: String,
    size: String,
    modiifcationTime: String,
    filePath: String,
    mediaMimeType: String,

    // File type specific metadata below.
    imageWidth: Number,
    imageHeight: Number,
    mediaTitle: String,
    mediaArtist: String,
  },

  // Clears fields.
  clear: function() {
    this.type = '';
    this.size = '';
    this.modiifcationTime = '';
    this.mediaMimeType = '';
    this.filePath = '';

    this.imageWidth = 0;
    this.imageHeight = 0;
    this.mediaTitle = '';
    this.mediaArtist = '';
  },

  /**
   * @param {string} type
   * @return {boolean}
   *
   * @private
   */
  isImage_: function(type) { return type === 'image'; },

  /**
   * @param {string} type
   * @return {boolean}
   *
   * @private
   */
  isVideo_: function(type) { return type === 'video'; },

  /**
   * @param {string} type
   * @return {boolean}
   *
   * @private
   */
  isAudio_: function(type) { return type === 'audio'; },

  /**
   * @param {number} imageWidth
   * @param {number} imageHeight
   * @param {string} mediaTitle
   * @param {string} mediaArtist
   * @return {boolean}
   *
   * @private
   */
  hasFileSpecificInfo_: function(
      imageWidth, imageHeight, mediaTitle, mediaArtist) {
    var result = (imageWidth && imageHeight) || mediaTitle || mediaArtist;
    return !!result;
  },

  /**
   * @param {number} imageWidth
   * @param {number} imageHeight
   * @return {string}
   *
   * @private
   */
  resolution_: function(imageWidth, imageHeight) {
    if (imageWidth && imageHeight)
      return imageWidth + " x " + imageHeight;
    return '';
  },

});
