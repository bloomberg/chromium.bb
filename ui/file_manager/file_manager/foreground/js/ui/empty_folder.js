// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Empty folder UI.
 * @param {!HTMLElement} emptyFolder DOM element of empty folder.
 * @constructor
 * @struct
 */
function EmptyFolder(emptyFolder) {
  /**
   * @private {!HTMLElement}
   */
  this.emptyFolder_ = emptyFolder;
}

/**
 * Shows empty folder UI.
 */
EmptyFolder.prototype.show = function() {
  this.emptyFolder_.hidden = false;
};

/**
 * Hides empty folder UI.
 */
EmptyFolder.prototype.hide = function() {
  this.emptyFolder_.hidden = true;
};
