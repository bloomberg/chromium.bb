// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

QUnit.module('base.inherits');

/**
 * @constructor
 * @extends {ChildClass}
 */
var GrandChildClass = function() {
  base.inherits(this, ChildClass);
  this.name = 'grandChild';
}

/**
 * @param {string} arg
 * @return {string}
 */
GrandChildClass.prototype.overrideMethod = function(arg) {
  return 'overrideMethod - grandChild - ' + arg;
}

/**
 * @constructor
 * @extends {ParentClass}
 */
var ChildClass = function() {
  base.inherits(this, ParentClass, 'parentArg');
  this.name = 'child';
  this.childOnly = 'childOnly';
}

/**
 * @param {string} arg
 * @return {string}
 */
ChildClass.prototype.overrideMethod = function(arg) {
  return 'overrideMethod - child - ' + arg;
}

/** @return {string} */
ChildClass.prototype.childMethod = function() {
  return 'childMethod';
}

/**
 * @param {string} arg
 * @constructor
 */
var ParentClass = function(arg) {
  /** @type  {string} */
  this.name = 'parent';
  /** @type {string} */
  this.parentOnly = 'parentOnly';
  /** @type {string} */
  this.parentConstructorArg = arg;
}

/** @return  {string} */
ParentClass.prototype.parentMethod = function() {
  return 'parentMethod';
}

/**
 * @param {string} arg
 * @return {string}
 */
ParentClass.prototype.overrideMethod = function(arg) {
  return 'overrideMethod - parent - ' + arg;
}

QUnit.test('should invoke parent constructor with the correct arguments',
  function(assert) {
  var child = new ChildClass();
  assert.equal(child.parentConstructorArg, 'parentArg');
});

QUnit.test('should preserve parent property and method', function(assert) {
  var child = new ChildClass();
  assert.equal(child.parentOnly, 'parentOnly');
  assert.equal(child.parentMethod(), 'parentMethod');
});

QUnit.test('should preserve instanceof', function(assert) {
  var child = new ChildClass();
  var grandChild = new GrandChildClass();
  assert.ok(child instanceof ParentClass);
  assert.ok(child instanceof ChildClass);
  assert.ok(grandChild instanceof ParentClass);
  assert.ok(grandChild instanceof ChildClass);
  assert.ok(grandChild instanceof GrandChildClass);
});

QUnit.test('should override parent property and method', function(assert) {
  var child = new ChildClass();
  assert.equal(child.name, 'child');
  assert.equal(child.overrideMethod('123'), 'overrideMethod - child - 123');
  assert.equal(child.childOnly, 'childOnly');
  assert.equal(child.childMethod(), 'childMethod');
});

QUnit.test('should work on an inheritance chain', function(assert) {
  var grandChild = new GrandChildClass();
  assert.equal(grandChild.name, 'grandChild');
  assert.equal(grandChild.overrideMethod('246'),
               'overrideMethod - grandChild - 246');
  assert.equal(grandChild.childOnly, 'childOnly');
  assert.equal(grandChild.childMethod(), 'childMethod');
  assert.equal(grandChild.parentOnly, 'parentOnly');
  assert.equal(grandChild.parentMethod(), 'parentMethod');
});

QUnit.test('should be able to access parent class methods', function(assert) {
  var grandChild = new GrandChildClass();

  assert.equal(grandChild.overrideMethod('789'),
               'overrideMethod - grandChild - 789');

  var childMethod = ChildClass.prototype.overrideMethod.call(grandChild, '81');
  assert.equal(childMethod, 'overrideMethod - child - 81');

  var parentMethod = ParentClass.prototype.overrideMethod.call(grandChild, '4');
  assert.equal(parentMethod, 'overrideMethod - parent - 4');
});

})();
