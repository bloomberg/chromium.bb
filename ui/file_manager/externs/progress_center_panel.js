// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @struct
 */
function ProgressCenterPanel() {}

/**
 * @param {string} id
 */
ProgressCenterPanel.prototype.dismissErrorItem = function(id) {};

/**
 * @param {!ProgressCenterItem} item
 */
ProgressCenterPanel.prototype.updateItem = function(item) {};

/** @type {?function(string)} */
ProgressCenterPanel.prototype.cancelCallback;

/** @type {?function(string)} */
ProgressCenterPanel.prototype.dismissErrorItemCallback;
