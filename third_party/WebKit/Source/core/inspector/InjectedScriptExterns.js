/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @constructor
 */
function InjectedScriptHostClass() { }
InjectedScriptHostClass.prototype.storageId = function(object) { }
InjectedScriptHostClass.prototype.getInternalProperties = function(object) { }
/**
 * @param {Function} func
 */
InjectedScriptHostClass.prototype.functionDetails = function(func) { }
/**
 * @param {!Object} obj
 * @return {!Array.<!Object>|undefined}
 */
InjectedScriptHostClass.prototype.collectionEntries = function(obj) { }
/**
 * @param {!Function} func
 * @param {*} receiver
 * @param {!Array.<*>=} args
 */
InjectedScriptHostClass.prototype.callFunction = function(func, receiver, args) { }
/**
 * @param {!Function} func
 * @param {*} receiver
 * @param {!Array.<*>=} args
 */
InjectedScriptHostClass.prototype.suppressWarningsAndCallFunction = function(func, receiver, args) { }
/**
 * @param {*} object
 */
InjectedScriptHostClass.prototype.isHTMLAllCollection = function(object) { }
/**
 * @param {*} object
 */
InjectedScriptHostClass.prototype.internalConstructorName = function(object) { }

InjectedScriptHostClass.prototype.clearConsoleMessages = function() { }
/**
 * @param {number} index
 */
InjectedScriptHostClass.prototype.inspectedObject = function(index) { }
/**
 * @param {*} object
 * @return {number}
 */
InjectedScriptHostClass.prototype.objectId = function(object) { }
/**
 * @param {*} object
 */
InjectedScriptHostClass.prototype.releaseObjectId = function(object) { }
/**
 * @param {*} object
 */
InjectedScriptHostClass.prototype.databaseId = function(object) { }
/**
 * @param {*} object
 * @param {Object} hints
 */
InjectedScriptHostClass.prototype.inspect = function(object, hints) { }
/**
 * @param {*} object
 */
InjectedScriptHostClass.prototype.subtype = function(object) { }
/**
 * @param {*} object
 */
InjectedScriptHostClass.prototype.getEventListeners = function(object) { }
/**
 * @param {string} expression
 */
InjectedScriptHostClass.prototype.eval = function(expression) { }
/**
 * @param {string} expression
 */
InjectedScriptHostClass.prototype.evaluateWithExceptionDetails = function(expression) { }
/**
 * @param {*} fn
 */
InjectedScriptHostClass.prototype.debugFunction = function(fn) { }
/**
 * @param {*} fn
 */
InjectedScriptHostClass.prototype.undebugFunction = function(fn) { }
/**
 * @param {*} fn
 */
InjectedScriptHostClass.prototype.monitorFunction = function(fn) { }
/**
 * @param {*} fn
 */
InjectedScriptHostClass.prototype.unmonitorFunction = function(fn) { }

/**
 * @param {function(...)} fun
 * @param {number} scopeNumber
 * @param {string} variableName
 * @param {*} newValue
 */
InjectedScriptHostClass.prototype.setFunctionVariableValue = function(fun, scopeNumber, variableName, newValue) { }

/**
 * @param {!Object} obj
 * @param {string} key
 * @param {*} value
 */
InjectedScriptHostClass.prototype.setNonEnumProperty = function(obj, key, value) { }


/** @type {!InjectedScriptHostClass} */
var InjectedScriptHost;
/** @type {!Window} */
var inspectedWindow;
/** @type {number} */
var injectedScriptId;
var __commandLineAPI;
var __scopeChainForEval;

/**
 * @constructor
 */
function JavaScriptCallFrame()
{
    /** @type {number} */
    this.sourceID;
    /** @type {number} */
    this.line;
    /** @type {number} */
    this.column;
    /** @type {*} */
    this.thisObject;
    /** @type {string} */
    this.stepInPositions;
}

/**
 * @param {number} index
 */
JavaScriptCallFrame.prototype.scopeType = function(index) { }

JavaScriptCallFrame.prototype.restart = function() { }
/**
 * @param {string} expression
 */
JavaScriptCallFrame.prototype.evaluateWithExceptionDetails = function(expression) { }
/**
 * @param {number} scopeNumber
 * @param {string} variableName
 * @param {*} newValue
 */
JavaScriptCallFrame.prototype.setVariableValue = function(scopeNumber, variableName, newValue) {}

/**
 * @constructor
 */
function JavaScriptFunction()
{
    /** @type {Array} */
    this.rawScopes;
}

// FIXME: Remove once ES6 is supported natively by JS compiler.

/** @typedef {string} */
var symbol;

/**
 * @param {string} description
 * @return {symbol}
 */
function Symbol(description) {}
