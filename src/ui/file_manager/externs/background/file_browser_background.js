// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @interface
 * @struct
 */
var FileBrowserBackground = function() {};

/**
 * @param {function()} callback
 */
FileBrowserBackground.prototype.ready = function(callback) {};

/** @type {!Object<!Window>} */
FileBrowserBackground.prototype.dialogs;
