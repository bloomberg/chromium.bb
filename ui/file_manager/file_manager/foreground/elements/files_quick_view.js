// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var FilesQuickView = Polymer({
  is: 'files-quick-view',

  properties: {
    image: String,
    video: String,
    videoPoster: String,
    audio: String,
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
});
