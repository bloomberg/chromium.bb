// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var sinon = sinon || {};

/** @type {Object} */
sinon.assert = {};

/**
 * @param {(sinon.Spy|Function)} f
 */
sinon.assert.called = function(f) {};

/**
 * @param {(sinon.Spy|Function)} f
 */
sinon.assert.calledOnce = function(f) {};

/**
 * @param {(sinon.Spy|Function)} f
 * @param {...} data
 */
sinon.assert.calledWith = function(f, data) {};

/**
 * @param {(sinon.Spy|Function)} f
 */
sinon.assert.notCalled = function(f) {};

/** @constructor */
sinon.Expectation = function() {};

/** @return {sinon.Expectation} */
sinon.Expectation.prototype.once = function() {};

/**
 * @param {...} data
 * @return {sinon.Expectation}
 */
sinon.Expectation.prototype.withArgs = function(data) {};

/** @return {boolean} */
sinon.Expectation.prototype.verify = function() {};

/** @param {...} data */
sinon.Expectation.prototype.returns = function(data) {};

/**
 * @param {Object} obj
 * @return {sinon.Mock}
 */
sinon.mock = function(obj) {};

/** @constructor */
sinon.Mock = function() {};

/**
 * @param {string} method
 * @return {sinon.Expectation}
 */
sinon.Mock.prototype.expects = function(method) {};

/** @type {function(...):Function} */
sinon.spy = function() {};

/**
 * This is a jscompile type that can be OR'ed with the actual type to make
 * jscompile aware of the sinon.spy functions that are added to the base
 * type.
 * Example: Instead of specifying a type of
 *   {function():void}
 * the following can be used to add the sinon.spy functions:
 *   {(sinon.Spy|function():void)}
 *
 * @constructor
 */
sinon.Spy = function() {};

/** @type {number} */
sinon.Spy.prototype.callCount;

/** @type {boolean} */
sinon.Spy.prototype.called = false;

/** @type {boolean} */
sinon.Spy.prototype.calledOnce = false;

/** @type {boolean} */
sinon.Spy.prototype.calledTwice = false;

/** @type {function(...):boolean} */
sinon.Spy.prototype.calledWith = function() {};

/** @type {function(number):{args:Array}} */
sinon.Spy.prototype.getCall = function(index) {};

sinon.Spy.prototype.reset = function() {};

sinon.Spy.prototype.restore = function() {};

/**
 * @param {Object} obj
 * @param {string} method
 * @param {Function=} opt_stubFunction
 * @return {sinon.TestStub}
 */
sinon.stub = function(obj, method, opt_stubFunction) {};

/** @constructor */
sinon.TestStub = function() {};

/** @type {function(number):{args:Array}} */
sinon.TestStub.prototype.getCall = function(index) {};

sinon.TestStub.prototype.restore = function() {};

/** @param {*} a */
sinon.TestStub.prototype.returns = function(a) {};

/** @type {function(...):sinon.Expectation} */
sinon.TestStub.prototype.withArgs = function() {};

/** @type {function(...):sinon.Expectation} */
sinon.TestStub.prototype.onFirstCall = function() {};

/** @returns {Object}  */
sinon.createStubInstance = function (/** * */ constructor) {};
