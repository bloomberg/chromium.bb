// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * A class for moving clipboard items between the plugin and the OS.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @constructor
 */
remoting.Clipboard = function() {
};

/**
 * @private
 * @enum {string}
 */
remoting.Clipboard.prototype.ItemTypes = {
  TEXT_TYPE: 'text/plain',
  TEXT_UTF8_TYPE: 'text/plain; charset=UTF-8'
};

/**
 * @private
 * @type {string}
 */
remoting.Clipboard.prototype.recentItemText = "";

/**
 * Accepts a clipboard from the OS, and sends any changed clipboard items to
 * the host.
 *
 * Currently only text items are supported.
 *
 * @param {remoting.ClipboardData} clipboardData
 * @return {void} Nothing.
 */
remoting.Clipboard.prototype.toHost = function(clipboardData) {
  if (!clipboardData || !clipboardData.types || !clipboardData.getData) {
    return;
  }
  if (!remoting.clientSession || !remoting.clientSession.plugin) {
    return;
  }
  var plugin = remoting.clientSession.plugin;
  for (var i = 0; i < clipboardData.types.length; i++) {
    var type = clipboardData.types[i];
    // The browser presents text clipboard items as 'text/plain'.
    if (type == this.ItemTypes.TEXT_TYPE) {
      var item = clipboardData.getData(type);
      if (!item) {
        item = "";
      }
      if (item != this.recentItemText) {
        // The plugin's JSON reader emits UTF-8.
        plugin.sendClipboardItem(this.ItemTypes.TEXT_UTF8_TYPE, item);
        this.recentItemText = item;
      }
    }
  }
};

/** @type {remoting.Clipboard} */
remoting.clipboard = null;
