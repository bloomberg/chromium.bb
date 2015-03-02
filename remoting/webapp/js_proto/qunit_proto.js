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


/**
 * @param {string} desc
 * @param {Function} f
 */
QUnit.asyncTest = function(desc, f) {};

/**
 * @param {*} a
 * @param {*} b
 */
QUnit.deepEqual = function(a, b) {};

/**
 * @param {*} a
 * @param {*} b
 */
QUnit.equal = function(a, b) {};

/**
 * @param {*} a
 */
QUnit.expect = function(a) {};

/**
 * @param {string} desc
 * @param {Object=} dict
 */
QUnit.module = function(desc, dict) {};

/**
 * @param {*} a
 * @param {*} b
 * @param {string} desc
 */
QUnit.notEqual = function(a, b, desc) {};

/**
 * @param {boolean} cond
 * @param {string=} desc
 * @return {boolean}
 */
QUnit.ok = function(cond, desc) {};

QUnit.start = function() {};

/**
 * @param {string} desc
 * @param {Function} f
 */
QUnit.test = function(desc, f) {};

/** @param {Function} f */
QUnit.testStart = function(f) {};


var deepEqual = QUnit.deepEqual;
var equal = QUnit.equal;
var expect = QUnit.expect;
var module = QUnit.module;
var notEqual = QUnit.notEqual;
var ok = QUnit.ok;
var test = QUnit.test;
