// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains type definitions for the remoting.ClipboardEvent class,
// which is a subclass of Event.
// It is used only with JSCompiler to verify the type-correctness of our code.

/** @suppress {duplicate} */
var remoting = remoting || {};

/** @constructor
 *  @extends Event
 */
remoting.ClipboardData = function() {};

/** @type {Array.<string>} */
remoting.ClipboardData.prototype.types;

/** @type {function(string): string} */
remoting.ClipboardData.prototype.getData;

/** @type {function(string, string): void} */
remoting.ClipboardData.prototype.setData;

/** @constructor
 */
remoting.ClipboardEvent = function() {};

/** @type {remoting.ClipboardData} */
remoting.ClipboardEvent.prototype.clipboardData;

/** @type {function(): void} */
remoting.ClipboardEvent.prototype.preventDefault;
