// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains various hacks needed to inform JSCompiler of various
// WebKit- and Chrome-specific properties and methods. It is used only with
// JSCompiler to verify the type-correctness of our code.

/** @type {HTMLElement} */
Document.prototype.activeElement;

/** @type {Array.<HTMLElement>} */
Document.prototype.all;

/** @type {boolean} */
Document.prototype.hidden;

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

/** @type {boolean} */
Element.prototype.hidden;


/** @constructor
    @extends {HTMLElement} */
var HTMLEmbedElement = function() { };

/** @type {number} */
HTMLEmbedElement.prototype.height;

/** @type {number} */
HTMLEmbedElement.prototype.width;

/** @type {Window} */
HTMLIFrameElement.prototype.contentWindow;


/** @type {Object} */
var JSON = {};

/**
 * @param {string} jsonStr The string to parse.
 * @param {(function(string, *) : *)=} opt_reviver
 * @return {*} The JSON object.
 */
JSON.parse = function(jsonStr, opt_reviver) {};

/**
 * @param {*} jsonObj Input object.
 * @param {(Array.<string>|(function(string, *) : *)|null)=} opt_replacer
 * @param {(number|string)=} opt_space
 * @return {string} json string which represents jsonObj.
 */
JSON.stringify = function(jsonObj, opt_replacer, opt_space) {};


/**
 * @param {string} name
 * @return {string}
 */
Node.prototype.getAttribute = function(name) { };

/** @type {string} */
Node.prototype.value;

/** @type {{top: string, left: string, bottom: string, right: string}} */
Node.prototype.style;


/**
 * @constructor
 * @param {function(Array.<MutationRecord>):void} callback
 */
var MutationObserver = function(callback) {};

/**
 * @param {Element} element
 * @param {Object} options
 */
MutationObserver.prototype.observe = function(element, options) {};


/** @constructor */
var MutationRecord = function() {};

/** @type {string} */
MutationRecord.prototype.attributeName;

/** @type {Element} */
MutationRecord.prototype.target;

/** @type {string} */
MutationRecord.prototype.type;


/** @type {{getRandomValues: function((Uint16Array|Uint8Array)):void}} */
Window.prototype.crypto;


/**
 * @constructor
 * @implements {EventTarget} */
var EventTargetStub = function() {};

/**
 * @param {string} type
 * @param {(EventListener|function(Event): (boolean|undefined|null))} listener
 * @param {boolean=} opt_useCapture
 */
EventTargetStub.prototype.addEventListener =
    function(type, listener, opt_useCapture) {}

/**
 * @param {string} type
 * @param {(EventListener|function(Event): (boolean|undefined|null))} listener
 * @param {boolean=} opt_useCapture
 */
EventTargetStub.prototype.removeEventListener =
    function(type, listener, opt_useCapture) {}

/**
 * @param {Event} event
 */
EventTargetStub.prototype.dispatchEvent =
    function(event) {}

/**
 * @constructor
 * @extends {EventTargetStub}
 */
var SourceBuffer = function() {}

/** @type {boolean} */
SourceBuffer.prototype.updating;

/** @type {TimeRanges} */
SourceBuffer.prototype.buffered;

/**
 * @param {ArrayBuffer} buffer
 */
SourceBuffer.prototype.appendBuffer = function(buffer) {}

/**
 * @param {number} start
 * @param {number} end
 */
SourceBuffer.prototype.remove = function(start, end) {}

/**
 * @constructor
 * @extends {EventTargetStub}
 */
var MediaSource = function() {}

/**
 * @param {string} format
 * @return {SourceBuffer}
 */
MediaSource.prototype.addSourceBuffer = function(format) {}

/**
 * @constructor
 * @param {function(function(*), function(*)) : void} init
 */
var Promise = function (init) {};

/**
 * @param {function(*) : void} onFulfill
 * @param {function(*) : void} onReject
 * @return {Promise}
 */
Promise.prototype.then = function (onFulfill, onReject) {};

/**
 * @param {function(*) : void} onReject
 * @return {Promise}
 */
Promise.prototype['catch'] = function (onReject) {};

/**
 * @param {Array.<Promise>} promises
 * @return {Promise}
 */
Promise.prototype.race = function (promises) {}

/**
 * @param {Array.<Promise>} promises
 * @return {Promise}
 */
Promise.prototype.all = function (promises) {};

/**
 * @param {*} reason
 * @return {Promise}
 */
Promise.reject = function (reason) {};

/**
 * @param {*} value
 * @return {Promise}
 */
Promise.resolve = function (value) {};
