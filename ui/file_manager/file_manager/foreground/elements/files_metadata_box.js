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
    imageWidth: {
      type: Number,
      observer: 'metadataUpdated_',
    },
    imageHeight: {
      type: Number,
      observer: 'metadataUpdated_',
    },
    mediaAlbum: {
      type: String,
      observer: 'metadataUpdated_',
    },
    mediaArtist: {
      type: String,
      observer: 'metadataUpdated_',
    },
    mediaDuration: {
      type: Number,
      observer: 'metadataUpdated_',
    },
    mediaGenre: {
      type: String,
      observer: 'metadataUpdated_',
    },
    mediaTitle: {
      type: String,
      observer: 'metadataUpdated_',
    },
    mediaTrack: {
      type: Number,
      observer: 'metadataUpdated_',
    },

    // Whether the size is the middle of loading.
    isSizeLoading: Boolean,

    /**
     * @private
     */
    hasFileSpecificInfo_: Boolean,
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
    this.mediaAlbum = '';
    this.mediaDuration = 0;
    this.mediaGenre = '';
    this.mediaTrack = 0;

    this.isSizeLoading = false;
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
   * Update private properties computed from metadata.
   * @private
   */
  metadataUpdated_: function() {
    this.hasFileSpecificInfo_ =
        !!(this.imageWidth && this.imageHeight || this.mediaTitle ||
           this.mediaArtist || this.mediaAlbum || this.mediaDuration ||
           this.mediaGenre || this.mediaTrack);
  },

  /**
   * Converts the duration into human friendly string.
   * @param {number} time the duration in seconds.
   * @return {string} String representation of the given duration.
   **/
  time2string_: function(time) {
    if (!time)
      return '';

    var seconds = time % 60;
    var minutes = Math.floor(time / 60) % 60;
    var hours = Math.floor(time / 60 / 60);

    if (hours === 0) {
      return minutes + ':' + ('0' + seconds).slice(-2);
    }
    return hours + ':' + ('0' + minutes).slice(-2) + ('0' + seconds).slice(-2);
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
