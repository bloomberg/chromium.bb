// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

var source = null;
var listener = null;

module('base.EventSource.raiseEvent', {
  setup: function() {
    source = new base.EventSource();
    source.defineEvents(['foo', 'bar']);
    listener = sinon.spy();
    source.addEventListener('foo', listener);
  },
  teardown: function() {
    source = null;
    listener = null;
  }
});

test('should invoke the listener', function() {
  source.raiseEvent('foo');
  sinon.assert.called(listener);
});

test('should invoke the listener with the correct event data', function() {
  var data = {
    field: 'foo'
  };
  source.raiseEvent('foo', data);
  sinon.assert.calledWith(listener, data);
});

test('should not invoke listeners that are added during raiseEvent',
  function() {
    source.addEventListener('foo', function() {
      source.addEventListener('foo', function() {
        ok(false);
      });
      ok(true);
    });
    source.raiseEvent('foo');
});

test('should not invoke listeners of a different event',
  function() {
    source.raiseEvent('bar');
    sinon.assert.notCalled(listener);
});

test('should assert when undeclared events are raised', function() {
  sinon.spy(base.debug, 'assert');
  try {
    source.raiseEvent('undefined');
  } catch (e){
  } finally{
    sinon.assert.called(base.debug.assert);
    base.debug.assert.restore();
  }
});

module('base.EventSource.removeEventListener', {
  setup: function() {
    source = new base.EventSource();
    source.defineEvents(['foo', 'bar']);
  },
  teardown: function() {
    source = null;
    listener = null;
  }
});

test('should not invoke the listener in subsequent calls to |raiseEvent|',
  function() {
    listener = sinon.spy();
    source.addEventListener('foo', listener);

    source.raiseEvent('foo');
    sinon.assert.calledOnce(listener);

    source.removeEventListener('foo', listener);
    source.raiseEvent('foo');
    sinon.assert.calledOnce(listener);
});

test('should work even if the listener is removed during |raiseEvent|',
  function() {
    var sink = {};
    sink.listener = sinon.spy(function() {
      source.removeEventListener('foo', sink.listener);
    });

    source.addEventListener('foo', sink.listener);
    source.raiseEvent('foo');
    sinon.assert.calledOnce(sink.listener);

    source.raiseEvent('foo');
    sinon.assert.calledOnce(sink.listener);
});

})();
