// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This definition is required by
 * ui/file_manager/file_manager/common/js/util.js.
 * @type {string}
 */
Window.prototype.appID;

/**
 * @typedef {?{
 *   position: (number|undefined),
 *   time: (number|undefined),
 *   expanded: (boolean|undefined),
 *   items: (!Array<string>|undefined)
 * }}
 */
var Playlist;

/**
 * @type {(Playlist|Object|undefined)}
 */
Window.prototype.appState;

/**
 * @type {(boolean|undefined)}
 */
Window.prototype.appReopen;
