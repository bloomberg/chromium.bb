// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @interface */
function InjectedScriptHostClass()
{
}

/**
 */
InjectedScriptHostClass.prototype.clearConsoleMessages = function() {}

/**
 * @param {*} objectId
 * @param {!Object} hints
 */
InjectedScriptHostClass.prototype.inspect = function(objectId, hints) {}

/**
 * @param {number} num
 * @return {*}
 */
InjectedScriptHostClass.prototype.inspectedObject = function(num) {}

/**
 * @param {*} obj
 * @return {string}
 */
InjectedScriptHostClass.prototype.internalConstructorName = function(obj) {}

/**
 * @param {*} obj
 * @return {boolean}
 */
InjectedScriptHostClass.prototype.formatAccessorsAsProperties = function(obj) {}

/**
 * @param {*} obj
 * @return {boolean}
 */
InjectedScriptHostClass.prototype.isTypedArray = function(obj) {}

/**
 * @param {*} obj
 * @return {string}
 */
InjectedScriptHostClass.prototype.subtype = function(obj) {}

/**
 * @param {!Object} obj
 * @return {?Array.<*>}
 */
InjectedScriptHostClass.prototype.collectionEntries = function(obj) {}

/**
 * @param {*} obj
 * @return {!Array.<*>}
 */
InjectedScriptHostClass.prototype.getInternalProperties = function(obj) {}

/**
 * @param {!EventTarget} target
 * @return {!Object|undefined}
 */
InjectedScriptHostClass.prototype.getEventListeners = function(target) {}

/**
 * @param {*} fn
 */
InjectedScriptHostClass.prototype.debugFunction = function(fn) {}

/**
 * @param {*} fn
 */
InjectedScriptHostClass.prototype.undebugFunction = function(fn) {}

/**
 * @param {*} fn
 */
InjectedScriptHostClass.prototype.monitorFunction = function(fn) {}

/**
 * @param {*} fn
 */
InjectedScriptHostClass.prototype.unmonitorFunction = function(fn) {}

/**
 * @param {!Function} fn
 * @param {*} receiver
 * @param {!Array.<*>=} argv
 * @return {*}
 */
InjectedScriptHostClass.prototype.callFunction = function(fn, receiver, argv) {}

/**
 * @param {!Function} fn
 * @param {*} receiver
 * @param {!Array.<*>=} argv
 * @return {*}
 */
InjectedScriptHostClass.prototype.suppressWarningsAndCallFunction = function(fn, receiver, argv) {}

/**
 * @param {!Object} obj
 * @param {string} key
 * @param {*} value
 */
InjectedScriptHostClass.prototype.setNonEnumProperty = function(obj, key, value) {}

/**
 * @param {*} value
 * @param {string} groupName
 * @return {number}
 */
InjectedScriptHostClass.prototype.bind = function(value, groupName) {}

/** @type {!InjectedScriptHostClass} */
var InjectedScriptHost;

/** @interface */
function JavaScriptCallFrame()
{
    /** @type {!JavaScriptCallFrame} */
    this.caller;
    /** @type {number} */
    this.sourceID;
    /** @type {number} */
    this.line;
    /** @type {number} */
    this.column;
    /** @type {!Array.<!Object>} */
    this.scopeChain;
    /** @type {!Object} */
    this.thisObject;
    /** @type {string} */
    this.stepInPositions;
    /** @type {string} */
    this.functionName;
    /** @type {number} */
    this.functionLine;
    /** @type {number} */
    this.functionColumn;
    /** @type {boolean} */
    this.isAtReturn;
    /** @type {*} */
    this.returnValue;
}

/**
 * @param {number} scopeIndex
 * @return {!DebuggerAgent.ScopeType<string>}
 */
JavaScriptCallFrame.prototype.scopeType = function(scopeIndex) {}

/**
 * @param {number} scopeIndex
 * @return {string}
 */
JavaScriptCallFrame.prototype.scopeName = function(scopeIndex) {}

/**
 * @param {number} scopeIndex
 * @return {?DebuggerAgent.Location}
 */
JavaScriptCallFrame.prototype.scopeStartLocation = function(scopeIndex) {}

/**
 * @param {number} scopeIndex
 * @return {?DebuggerAgent.Location}
 */
JavaScriptCallFrame.prototype.scopeEndLocation = function(scopeIndex) {}


/** @type {!Window} */
var inspectedGlobalObject;
/** @type {number} */
var injectedScriptId;
