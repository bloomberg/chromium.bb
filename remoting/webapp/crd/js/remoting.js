// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @type {base.EventSourceImpl} An event source object for handling global
 *    events. This is an interim hack.  Eventually, we should move
 *    functionalities away from the remoting namespace and into smaller objects.
 */
remoting.testEvents;

/**
 * Initialization tasks that are common to all remoting apps.
 */
remoting.initGlobalObjects = function() {
  if (base.isAppsV2()) {
    var htmlNode = /** @type {HTMLElement} */ (document.body.parentNode);
    htmlNode.classList.add('apps-v2');
  }

  console.log(remoting.getExtensionInfo());
  l10n.localize();

  remoting.stats = new remoting.ConnectionStats(
      document.getElementById('statistics'));
  remoting.formatIq = new remoting.FormatIq();

  remoting.clipboard = new remoting.Clipboard();
  var sandbox =
      /** @type {HTMLIFrameElement} */ (document.getElementById('wcs-sandbox'));
  remoting.wcsSandbox = new remoting.WcsSandboxContainer(sandbox.contentWindow);

  // The plugin's onFocus handler sends a paste command to |window|, because
  // it can't send one to the plugin element itself.
  window.addEventListener('paste', pluginGotPaste_, false);
  window.addEventListener('copy', pluginGotCopy_, false);

  remoting.initModalDialogs();

  remoting.testEvents = new base.EventSourceImpl();
  /** @enum {string} */
  remoting.testEvents.Names = {
    uiModeChanged: 'uiModeChanged'
  };
  remoting.testEvents.defineEvents(base.values(remoting.testEvents.Names));
}

/**
 * @return {string} Information about the current extension.
 */
remoting.getExtensionInfo = function() {
  var v2OrLegacy = base.isAppsV2() ? " (v2)" : " (legacy)";
  var manifest = chrome.runtime.getManifest();
  if (manifest && manifest.version) {
    var name = remoting.app.getApplicationName();
    return name + ' version: ' + manifest.version + v2OrLegacy;
  } else {
    return 'Failed to get product version. Corrupt manifest?';
  }
};

/**
 * Callback function called when the browser window gets a paste operation.
 *
 * @param {Event} event
 * @return {void} Nothing.
 */
function pluginGotPaste_(event) {
  if (event && event.clipboardData) {
    remoting.clipboard.toHost(event.clipboardData);
  }
}

/**
 * Callback function called when the browser window gets a copy operation.
 *
 * @param {Event} event
 * @return {void} Nothing.
 */
function pluginGotCopy_(event) {
  if (event && event.clipboardData) {
    if (remoting.clipboard.toOs(event.clipboardData)) {
      // The default action may overwrite items that we added to clipboardData.
      event.preventDefault();
    }
  }
}

/**
 * Return the current time as a formatted string suitable for logging.
 *
 * @return {string} The current time, formatted as [mmdd/hhmmss.xyz]
 */
remoting.timestamp = function() {
  /**
   * @param {number} num A number.
   * @param {number} len The required length of the answer.
   * @return {string} The number, formatted as a string of the specified length
   *     by prepending zeroes as necessary.
   */
  var pad = function(num, len) {
    var result = num.toString();
    if (result.length < len) {
      result = new Array(len - result.length + 1).join('0') + result;
    }
    return result;
  };
  var now = new Date();
  var timestamp = pad(now.getMonth() + 1, 2) + pad(now.getDate(), 2) + '/' +
      pad(now.getHours(), 2) + pad(now.getMinutes(), 2) +
      pad(now.getSeconds(), 2) + '.' + pad(now.getMilliseconds(), 3);
  return '[' + timestamp + ']';
};
