// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var FilesQuickView = Polymer({
  is: 'files-quick-view',

  properties: {
    filePath: String,
    image: String,
    video: String,
    videoPoster: String,
    audio: String,

    // metadata-box-active-changed event is fired on attribute change.
    metadataBoxActive: {
      value: true,
      type: Boolean,
      notify: true,
    }
  },

  listeners: {
    'iron-overlay-closed': 'clear_',
  },

  setImageURL: function(url) {
    this.clear_();
    this.image = url;
  },

  setVideoURL: function(url) {
    this.clear_();
    this.video = url;
  },

  setAudioURL: function(url) {
    this.clear_();
    this.audio = url;
  },

  clear_: function() {
    this.image = '';
    this.video = '';
    this.audio = '';
  },

  // Open the dialog.
  open: function() {
    if (!this.isOpened())
      this.$.dialog.open();
  },

  close: function() {
    if (this.isOpened())
      this.$.dialog.close();
  },

  isOpened: function() {
    // TODO(oka): This is a workaround to satisfy closure compiler.
    // Update ['opened'] to .opened.
    return (/** @type{Object} */ (this.$.dialog))['opened'];
  },

  getFilesMetadataBox: function() {
    return this.$['metadata-box'];
  },

  // Client should assign the function to open the file.
  onOpenInNewButtonTap: function(event) {},

  onCloseButtonTap_: function(event) {
    this.close();
  },

});
