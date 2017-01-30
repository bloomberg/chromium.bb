// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
* @fileoverview Partial definitions for ECMAScript 6. To compile the Files app,
*     some definitions are defined in this file. They should be removed once
*     they are ready in closure compiler by default.
* @externs
*/

/**
 * @param {string} searchString
 * @param {number=} opt_position
 */
String.prototype.startsWith = function(searchString, opt_position) {};

/**
 * @param {?function(this:S, T, number, !NodeList<T>): ?} callback
 * @template T,S
 * @return {undefined}
 */
NodeList.prototype.forEach = function(callback) {};
