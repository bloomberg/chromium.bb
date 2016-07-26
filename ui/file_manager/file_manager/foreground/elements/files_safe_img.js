// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var FILES_APP_ORIGIN = 'chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj';

/**
 * Polymer element to render an image securely inside webview.
 * When tapped, files-safe-img-tap-inside or
 * files-safe-img-tap-outside events are fired depending on the position
 * of the tap.
 */
var FilesSafeImg = Polymer({
  is: 'files-safe-img',

  properties: {
    // URL accessible from webview.
    src: {
      type: String,
      observer: 'onSrcChange_',
      reflectToAttribute: true
    }
  },

  listeners: {'src-changed': 'onSrcChange_'},

  onSrcChange_: function() {
    if (!this.src && this.webview_) {
      // Remove webview to clean up unnecessary processes.
      Polymer.dom(this.$.content).removeChild(this.webview_);
      this.webview_ = null;
    } else if (this.src && !this.webview_) {
      // Create webview node only if src exists to save resouces.
      var webview = document.createElement('webview');
      this.webview_ = webview;
      webview.partition = 'trusted';
      webview.allowtransparency = 'true';
      Polymer.dom(this.$.content).appendChild(webview);
      webview.addEventListener(
          'contentload', this.onSrcChange_.bind(this));
      webview.src = 'foreground/elements/files_safe_img_webview_content.html';
    } else if (this.src && this.webview_.contentWindow) {
      this.webview_.contentWindow.postMessage(this.src, FILES_APP_ORIGIN);
    }
  },

  created: function() {
    /**
     * @type {HTMLElement}
     */
    this.webview_ = null;
  },

  ready: function() {
    window.addEventListener('message', function(event) {
      if (event.origin !== FILES_APP_ORIGIN) {
        console.error('Unknown origin.');
        return;
      }
      if (event.data === 'tap-inside') {
        this.fire('files-safe-img-tap-inside');
      } else if (event.data === 'tap-outside') {
        this.fire('files-safe-img-tap-outside');
      }
    }.bind(this));
  }
});
