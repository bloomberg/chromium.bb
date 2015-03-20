// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains various hacks needed to inform JSCompiler of various
// QUnit-specific properties and methods. It is used only with JSCompiler to
// verify the type-correctness of our code.


/** @type {Object} */
var QUnit = QUnit || {};

/** @constructor */
QUnit.Test = function() {};

/** @type {QUnit.Clock} */
QUnit.Test.prototype.clock = new QUnit.Clock();

/** @constructor */
QUnit.Clock = function() {};

/** @param {number} ticks */
QUnit.Clock.prototype.tick = function(ticks) {};

/** @param {Function} f */
QUnit.testStart = function(f) {};

/**
 * @interface
 */
QUnit.Assert = function() {};

/**
 * @return {function():void}
 */
QUnit.Assert.prototype.async = function() {};

/**
 * @param {*} a
 * @param {*} b
 * @param {string=} opt_desc
 */
QUnit.Assert.prototype.notEqual = function(a, b, opt_desc) {};

/**
 * @param {boolean} cond
 * @param {string=} desc
 * @return {boolean}
 */
QUnit.Assert.prototype.ok = function(cond, desc) {};

/**
 * @param {*} a
 * @param {*} b
 * @param {string=} opt_message
 */
QUnit.Assert.prototype.deepEqual = function(a, b, opt_message) {};

/**
 * @param {*} a
 * @param {*} b
 * @param {string=} opt_message
 */
QUnit.Assert.prototype.equal = function(a, b, opt_message) {};

/**
 * @param {number} assertionCount
 */
QUnit.Assert.prototype.expect = function(assertionCount) {};

/**
 * @param {*} a
 * @param {*} b
 * @param {string=} opt_message
 */
QUnit.Assert.prototype.strictEqual = function(a, b, opt_message) {};

/**
 * @param {function()} a
 * @param {*=} opt_b
 * @param {string=} opt_message
 */
QUnit.Assert.prototype.throws = function(a, opt_b, opt_message) {};

/**
 * @typedef {{
 *   beforeEach: (function(QUnit.Assert=) | undefined),
 *   afterEach: (function(QUnit.Assert=) | undefined)
 * }}
 */
QUnit.ModuleArgs;

/**
 * @param {string} desc
 * @param {QUnit.ModuleArgs=} opt_args=
 */
QUnit.module = function(desc, opt_args) {};

/**
 * @param {string} desc
 * @param {function(QUnit.Assert)} f
 */
QUnit.test = function(desc, f) {};
