// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains type definitions for the viewer plugin. It is used only
// with JSCompiler to verify the type-correctness of our code.

/** @suppress {duplicate} */
var remoting = remoting || {};

/** @constructor
 *  @extends HTMLElement
 */
remoting.ViewerPlugin = function() {};

/** @param {string} iq The Iq stanza received from the host. */
remoting.ViewerPlugin.onIq = function(iq) {};

/** @param {boolean} scale True to enable scaling, false to disable. */
remoting.ViewerPlugin.setScaleToFit = function(scale) {};

/** @type {number} */ remoting.ViewerPlugin.prototype.apiMinVersion;
/** @type {number} */ remoting.ViewerPlugin.prototype.apiVersion;

/** @type {number} */ remoting.ViewerPlugin.desktopHeight;
/** @type {number} */ remoting.ViewerPlugin.desktopWidth;

/** @type {number} */ remoting.ViewerPlugin.STATUS_UNKNOWN;
/** @type {number} */ remoting.ViewerPlugin.STATUS_CONNECTING;
/** @type {number} */ remoting.ViewerPlugin.STATUS_INITIALIZING;
/** @type {number} */ remoting.ViewerPlugin.STATUS_CONNECTED;
/** @type {number} */ remoting.ViewerPlugin.STATUS_CLOSED;
/** @type {number} */ remoting.ViewerPlugin.STATUS_FAILED;

/** @type {number} */ remoting.ViewerPlugin.videoBandwidth;
/** @type {number} */ remoting.ViewerPlugin.videoCaptureLatency;
/** @type {number} */ remoting.ViewerPlugin.videoEncodeLatency;
/** @type {number} */ remoting.ViewerPlugin.videoDecodeLatency;
/** @type {number} */ remoting.ViewerPlugin.videoRenderLatency;
/** @type {number} */ remoting.ViewerPlugin.roundTripLatency;
