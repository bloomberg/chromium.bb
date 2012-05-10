// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains various hacks needed to inform JSCompiler of various
// WebKit-specific properties and methods. It is used only with JSCompiler
// to verify the type-correctness of our code.

/** @type {HTMLElement} */
Document.prototype.activeElement;

/** @type {Array.<HTMLElement>} */
Document.prototype.all;

/** @type {function(string): void} */
Document.prototype.execCommand = function(command) {};

/** @return {void} Nothing. */
Document.prototype.webkitCancelFullScreen = function() {};

/** @type {boolean} */
Document.prototype.webkitIsFullScreen;

/** @type {boolean} */
Document.prototype.webkitHidden;

/** @type {number} */
Element.ALLOW_KEYBOARD_INPUT;

/** @param {number} flags
/** @return {void} Nothing. */
Element.prototype.webkitRequestFullScreen = function(flags) {};

/** @type {{getRandomValues: function(Uint16Array):void}} */
Window.prototype.crypto;

/** @constructor
    @extends {HTMLElement} */
var HTMLEmbedElement = function() { };

/** @type {number} */
HTMLEmbedElement.prototype.height;

/** @type {number} */
HTMLEmbedElement.prototype.width;

/** @constructor */
var MutationRecord = function() {};

/** @type {string} */
MutationRecord.prototype.attributeName;

/** @type {Element} */
MutationRecord.prototype.target;

/** @type {string} */
MutationRecord.prototype.type;

/** @constructor
    @param {function(Array.<MutationRecord>):void} callback */
var WebKitMutationObserver = function(callback) {};

/** @param {Element} element
    @param {Object} options */
WebKitMutationObserver.prototype.observe = function(element, options) {};

// This string is replaced with the actual value in build-webapp.py.
var OAUTH2_USE_OFFICIAL_CLIENT_ID = false;
