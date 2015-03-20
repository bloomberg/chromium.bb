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

/** @return {string} */
GrandChildClass.prototype.overrideMethod = function() {
  return 'overrideMethod - grandChild';
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

/** @return {string} */
ChildClass.prototype.overrideMethod = function() {
  return 'overrideMethod - child';
}

/** @return {string} */
ChildClass.prototype.childMethod = function() {
  return 'childMethod';
}

/**
 * @constructor
 * @param {string} arg
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

/** @return  {string} */
ParentClass.prototype.overrideMethod = function() {
  return 'overrideMethod - parent';
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
  assert.equal(child.overrideMethod(), 'overrideMethod - child');
  assert.equal(child.childOnly, 'childOnly');
  assert.equal(child.childMethod(), 'childMethod');
});

QUnit.test('should works on an inheritance chain', function(assert) {
  var grandChild = new GrandChildClass();
  assert.equal(grandChild.name, 'grandChild');
  assert.equal(grandChild.overrideMethod(), 'overrideMethod - grandChild');
  assert.equal(grandChild.childOnly, 'childOnly');
  assert.equal(grandChild.childMethod(), 'childMethod');
  assert.equal(grandChild.parentOnly, 'parentOnly');
  assert.equal(grandChild.parentMethod(), 'parentMethod');
});

})();