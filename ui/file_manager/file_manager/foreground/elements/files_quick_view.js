// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var FilesQuickView = Polymer({
  is: 'files-quick-view',

  properties: {
    // File media type, e.g. image, video.
    type: String,
    filePath: String,
    contentUrl: String,
    videoPoster: String,
    audioArtwork: String,
    autoplay: Boolean,

    // metadata-box-active-changed event is fired on attribute change.
    metadataBoxActive: {
      value: true,
      type: Boolean,
      notify: true,
    }
  },

  listeners: {
    'iron-overlay-closed': 'clear',
  },

  // Clears fields.
  clear: function() {
    this.type = '';
    this.filePath = '';
    this.contentUrl = '';
    this.videoPoster = '';
    this.audioArtwork = '';
    this.autoplay = false;
  },

  // Opens the dialog.
  open: function() {
    if (!this.isOpened())
      this.$.dialog.open();
  },

  // Closes the dialog.
  close: function() {
    if (this.isOpened())
      this.$.dialog.close();
  },

  /**
   * @return {boolean}
   */
  isOpened: function() {
    // TODO(oka): This is a workaround to satisfy closure compiler.
    // Update ['opened'] to .opened.
    return (/** @type{Object} */ (this.$.dialog))['opened'];
  },

  /**
   * @return {!FilesMetadataBox}
   */
  getFilesMetadataBox: function() {
    return this.$['metadata-box'];
  },

  /**
   * Client should assign the function to open the file.
   *
   * @param {!Event} event
   */
  onOpenInNewButtonTap: function(event) {},

  /**
   * @param {!Event} event
   *
   * @private
   */
  onCloseButtonTap_: function(event) {
    this.close();
  },

  /**
   * @param {!Event} event tap event.
   *
   * @private
   */
  onContentPanelTap_: function(event) {
    var target = event.detail.sourceEvent.target;
    if (target.classList.contains('close-on-click'))
      this.close();
  },

  /**
   * @param {string} type
   * @return {boolean}
   *
   * @private
   */
  isImage_: function(type) {
    return type === 'image';
  },

  /**
   * @param {string} type
   * @return {boolean}
   *
   * @private
   */
  isVideo_: function(type) {
    return type === 'video';
  },

  /**
   * @param {string} type
   * @return {boolean}
   *
   * @private
   */
  isAudio_: function(type) {
    return type === 'audio';
  },

  /**
   * @param {string} type
   * @return {boolean}
   *
   * @private
   */
  isUnsupported_: function(type) {
    return !this.isImage_(type) && !this.isVideo_(type) && !this.isAudio_(type);
  },

});
