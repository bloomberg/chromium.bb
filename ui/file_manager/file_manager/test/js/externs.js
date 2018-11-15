// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview External objects and functions required for compiling.
 */

/** @param {...*} var_args */
ChromeEvent.prototype.dispatchEvent = (var_args) => {};

/** @constructor */
function FileManager() {
  /** @type {Crostini} */
  this.crostini;
}
FileManager.prototype.setupCrostini_ = () => {};

/** @type {string} */
let FILE_MANAGER_ROOT;

/** @type {FileManager} */
let fileManager;

/** @type {FileBrowserBackground} */
window.background;
