// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

/** @type {base.Ipc} */
var ipc_;

function pass() {
  ok(true);
  QUnit.start();
}

function fail() {
  ok(false);
  QUnit.start();
}

module('base.Ipc', {
  setup: function() {
    chromeMocks.activate(['runtime']);
    ipc_ = base.Ipc.getInstance();
  },
  teardown: function() {
    base.Ipc.deleteInstance();
    ipc_ = null;
    chromeMocks.restore();
  }
});

QUnit.test(
  'register() should return false if the request type was already registered',
  function() {
    var handler1 = function() {};
    var handler2 = function() {};
    QUnit.equal(true, ipc_.register('foo', handler1));
    QUnit.equal(false, ipc_.register('foo', handler2));
});

QUnit.asyncTest(
  'send() should invoke a registered handler with the correct arguments',
  function() {
    var handler = sinon.spy();
    var argArray = [1, 2, 3];
    var argDict = {
      key1: 'value1',
      key2: false
    };

    ipc_.register('foo', handler);
    base.Ipc.invoke('foo', 1, false, 'string', argArray, argDict).then(
      function() {
        sinon.assert.calledWith(handler, 1, false, 'string', argArray, argDict);
        pass();
    }, fail);
});

QUnit.asyncTest(
  'send() should not invoke a handler that is unregistered',
  function() {
    var handler = sinon.spy();
    ipc_.register('foo', handler);
    ipc_.unregister('foo');
    base.Ipc.invoke('foo', 'hello', 'world').then(fail, function(error) {
      sinon.assert.notCalled(handler);
      QUnit.equal(error, base.Ipc.Error.UNSUPPORTED_REQUEST_TYPE);
      pass();
    });
});

QUnit.asyncTest(
  'send() should raise exceptions on unknown request types',
  function() {
    var handler = sinon.spy();
    ipc_.register('foo', handler);
    base.Ipc.invoke('bar', 'hello', 'world').then(fail, function(error) {
      QUnit.equal(error, base.Ipc.Error.UNSUPPORTED_REQUEST_TYPE);
      pass();
    });
});

QUnit.asyncTest(
  'send() should raise exceptions on request from another extension',
  function() {
    var handler = sinon.spy();
    var oldId = chrome.runtime.id;
    ipc_.register('foo', handler);
    chrome.runtime.id = 'foreign-extension';
    base.Ipc.invoke('foo', 'hello', 'world').then(fail, function(error) {
      QUnit.equal(error, base.Ipc.Error.INVALID_REQUEST_ORIGIN);
      pass();
    });
    chrome.runtime.id = oldId;
});


QUnit.asyncTest(
  'send() should pass exceptions raised by the handler to the caller',
  function() {
    var handler = function() {
      throw new Error('Whatever can go wrong, will go wrong.');
    };
    ipc_.register('foo', handler);
    base.Ipc.invoke('foo').then(fail, function(error) {
      QUnit.equal(error, 'Whatever can go wrong, will go wrong.');
      pass();
    });
});

QUnit.asyncTest(
  'send() should pass the return value of the handler to the caller',
  function() {
    var handlers = {
      'boolean': function() { return false; },
      'number': function() { return 12; },
      'string': function() { return 'string'; },
      'array': function() { return [1, 2]; },
      'dict': function() { return {key1: 'value1', key2: 'value2'}; }
    };

    var testCases = [];
    for (var ipcName in handlers) {
      ipc_.register(ipcName, handlers[ipcName]);
      testCases.push(base.Ipc.invoke(ipcName));
    }

    Promise.all(testCases).then(function(results){
      QUnit.equal(results[0], false);
      QUnit.equal(results[1], 12);
      QUnit.equal(results[2], 'string');
      QUnit.deepEqual(results[3], [1,2]);
      QUnit.deepEqual(results[4], {key1: 'value1', key2: 'value2'});
      pass();
    }, fail);
});

})();
