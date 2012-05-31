// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains type definitions for the viewer plugin. It is used only
// with JSCompiler to verify the type-correctness of our code.

/** @suppress {duplicate} */
var remoting = remoting || {};

/** @constructor
 *  @extends HTMLEmbedElement
 */
remoting.ViewerPlugin = function() { };

/** @param {string} message The message to send to the host. */
remoting.ViewerPlugin.prototype.postMessage = function(message) {};
