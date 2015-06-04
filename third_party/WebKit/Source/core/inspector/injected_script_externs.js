// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
 * @param {string} script
 * @param {!Object=} scopeExtension
 * @return {*}
 */
JavaScriptCallFrame.prototype.evaluateWithExceptionDetails = function(script, scopeExtension) {}

/**
 * @return {*}
 */
JavaScriptCallFrame.prototype.restart = function() {}

/**
 * @param {number=} scopeIndex
 * @param {?string=} variableName
 * @param {*=} newValue
 * @return {*}
 */
JavaScriptCallFrame.prototype.setVariableValue = function(scopeIndex, variableName, newValue) {}

/**
 * @param {number} scopeIndex
 * @return {number}
 */
JavaScriptCallFrame.prototype.scopeType = function(scopeIndex) {}

