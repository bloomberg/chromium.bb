// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains various hacks needed to inform JSCompiler of various
// WebKit- and Chrome-specific properties and methods. It is used only with
// JSCompiler to verify the type-correctness of our code.

/** @type {Array<string>} */
ClipboardData.prototype.types;

/** @type {HTMLElement} */
Document.prototype.activeElement;

/** @type {Array<HTMLElement>} */
Document.prototype.all;

/** @type {boolean} */
Document.prototype.hidden;

/** @return {void} Nothing. */
Document.prototype.exitPointerLock = function() {};

/** @type {boolean} */
Document.prototype.webkitIsFullScreen;

/** @type {boolean} */
Document.prototype.webkitHidden;

/** @type {Element} */
Document.prototype.firstElementChild;

/** @return {void} Nothing. */
Element.prototype.requestPointerLock = function() {};

/** @type {boolean} */
Element.prototype.disabled;

/** @type {boolean} */
Element.prototype.hidden;

/** @type {string} */
Element.prototype.innerText;

/** @type {string} */
Element.prototype.localName;

/** @type {number} */
Element.prototype.offsetRight;

/** @type {number} */
Element.prototype.offsetBottom;

/** @type {string} */
Element.prototype.textContent;

/** @type {DOMTokenList} */
Element.prototype.classList;

/** @type {boolean} */
Element.prototype.checked;


/** @type {Window} */
HTMLIFrameElement.prototype.contentWindow;

/**
 * @param {string} selector
 * @return {?HTMLElement}.
 */
HTMLElement.prototype.querySelector = function(selector) {};

/**
 * @param {string} name
 * @return {string}
 */
Node.prototype.getAttribute = function(name) { };

/** @type {string} */
Node.prototype.value;

/** @type {{top: string, left: string, bottom: string, right: string}} */
Node.prototype.style;

/** @type {boolean} */
Node.prototype.hidden;


/** @type {{getRandomValues: function(!ArrayBufferView):!ArrayBufferView}} */
Window.prototype.crypto;


/**
 * @type {DataTransfer}
 */
Event.prototype.dataTransfer = null;

/**
 * @type {number}
 */
Event.prototype.movementX = 0;

/**
 * @type {number}
 */
Event.prototype.movementY = 0;

/**
 * @param {string} type
 * @param {boolean} canBubble
 * @param {boolean} cancelable
 * @param {Window} view
 * @param {number} detail
 * @param {number} screenX
 * @param {number} screenY
 * @param {number} clientX
 * @param {number} clientY
 * @param {boolean} ctrlKey
 * @param {boolean} altKey
 * @param {boolean} shiftKey
 * @param {boolean} metaKey
 * @param {number} button
 * @param {EventTarget} relatedTarget
 */
Event.prototype.initMouseEvent = function(
    type, canBubble, cancelable, view, detail,
    screenX, screenY, clientX, clientY,
    ctrlKey, altKey, shiftKey, metaKey,
    button, relatedTarget) {};

/** @type {Object} */
Event.prototype.data = {};

/**
 * @param {*} value
 * @return {boolean} whether value is an integer or not.
 */
Number.isInteger = function(value) {};

// Chrome implements XMLHttpRequest.responseURL starting from Chrome 37.
/** @type {string} */
XMLHttpRequest.prototype.responseURL = "";
