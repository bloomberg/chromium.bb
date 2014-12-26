// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Root class of the background page.
 * @constructor
 * @struct
 */
function BackgroundBase() {
  /**
   * Map of all currently open app windows. The key is an app ID.
   * @type {Object.<string, chrome.app.window.AppWindow>}
   */
  this.appWindows = {};
}

/**
 * Checks the current condition of background page.
 * @return {boolean} True if the background page can be closed. False if not.
 */
BackgroundBase.prototype.canClose = function() {
  return true;
};

/**
 * Checks the current condition of background page and closes it if possible.
 */
BackgroundBase.prototype.tryClose = function() {
  if (this.canClose())
    window.close();
};

/**
 * Gets similar windows, it means with the same initial url.
 * @param {string} url URL that the obtained windows have.
 * @return {Array.<chrome.app.window.AppWindow>} List of similar windows.
 */
BackgroundBase.prototype.getSimilarWindows = function(url) {
  var result = [];
  for (var appID in this.appWindows) {
    if (this.appWindows[appID].contentWindow.appInitialURL === url)
      result.push(this.appWindows[appID]);
  }
  return result;
};
