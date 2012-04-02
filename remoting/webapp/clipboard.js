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
remoting.Clipboard.prototype.itemToHostText = "";

/**
 * @private
 * @type {string}
 */
remoting.Clipboard.prototype.itemFromHostText = "";

/**
 * @private
 * @type {boolean}
 */
remoting.Clipboard.prototype.itemFromHostTextPending = false;

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
      // Don't send an item that's already been sent to the host.
      // And don't send an item that we received from the host: if we do, we
      // may send the same item to and fro indefinitely.
      if ((item != this.itemToHostText) && (item != this.itemFromHostText)) {
        // The plugin's JSON reader emits UTF-8.
        plugin.sendClipboardItem(this.ItemTypes.TEXT_UTF8_TYPE, item);
        this.itemToHostText = item;
      }
    }
  }
};

/**
 * Accepts a clipboard item from the host, and stores it so that toOs() will
 * subsequently send it to the OS clipboard.
 *
 * @param {string} mimeType The MIME type of the clipboard item.
 * @param {string} item The clipboard item.
 * @return {void} Nothing.
 */
remoting.Clipboard.prototype.fromHost = function(mimeType, item) {
  // The plugin's JSON layer will correctly convert only UTF-8 data sent from
  // the host.
  if (mimeType != this.ItemTypes.TEXT_UTF8_TYPE) {
    return;
  }
  if (item == this.itemToHostText) {
    return;
  }
  this.itemFromHostText = item;
  this.itemFromHostTextPending = true;
  document.execCommand("copy");
};

/**
 * Moves any pending clipboard items to a ClipboardData object.
 *
 * @param {remoting.ClipboardData} clipboardData
 * @return {boolean} Whether any clipboard items were moved to the ClipboardData
 *     object.
 */
remoting.Clipboard.prototype.toOs = function(clipboardData) {
  if (!this.itemFromHostTextPending) {
    return false;
  }
  // The JSON layer between the plugin and this webapp converts UTF-8 to the
  // JS string encoding. The browser will convert JS strings to the correct
  // encoding, per OS and locale conventions, provided the data type is
  // 'text/plain'.
  clipboardData.setData(this.ItemTypes.TEXT_TYPE, this.itemFromHostText);
  this.itemFromHostTextPending = false;
  return true;
};

/** @type {remoting.Clipboard} */
remoting.clipboard = null;
