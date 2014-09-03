// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

module('base');

test('mix(dest, src) should copy properties from |src| to |dest|',
  function() {
    var src = { a: 'a', b: 'b'};
    var dest = { c: 'c'};

    base.mix(dest, src);
    deepEqual(dest, {a: 'a', b: 'b', c: 'c'});
});

test('mix(dest, src) should assert if properties are overwritten',
  function() {
    var src = { a: 'a', b: 'b'};
    var dest = { a: 'a'};

    sinon.spy(base.debug, 'assert');

    try {
      base.mix(dest, src);
    } catch (e) {
    } finally {
      sinon.assert.called(base.debug.assert);
      base.debug.assert.restore();
    }
});

test('values(obj) should return an array containing the values of |obj|',
  function() {
    var output = base.values({ a: 'a', b: 'b'});

    notEqual(output.indexOf('a'), -1, '"a" should be in the output');
    notEqual(output.indexOf('b'), -1, '"b" should be in the output');
});

test('dispose(obj) should invoke the dispose method on |obj|',
  function() {
    var obj = {
      dispose: sinon.spy()
    };
    base.dispose(obj);
    sinon.assert.called(obj.dispose);
});

test('dispose(obj) should not crash if |obj| is null',
  function() {
    expect(0);
    base.dispose(null);
});

test('urljoin(url, opt_param) should return url if |opt_param| is missing',
  function() {
    QUnit.equal(
        base.urlJoin('http://www.chromium.org'), 'http://www.chromium.org');
});

test('urljoin(url, opt_param) should urlencode |opt_param|',
  function() {
    var result = base.urlJoin('http://www.chromium.org', {
      a: 'a',
      foo: 'foo',
      escapist: ':/?#[]@$&+,;='
    });
    QUnit.equal(
        result,
        'http://www.chromium.org?a=a&foo=foo' +
        '&escapist=%3A%2F%3F%23%5B%5D%40%24%26%2B%2C%3B%3D');
});

test('escapeHTML(str) should escape special characters', function() {
  QUnit.equal(
    base.escapeHTML('<script>alert("hello")</script>'),
    '&lt;script&gt;alert("hello")&lt;/script&gt;');
});

QUnit.asyncTest('Promise.sleep(delay) should fulfill the promise after |delay|',
  function() {
    var isCalled = false;
    var clock = this.clock;

    base.Promise.sleep(100).then(function(){
      isCalled = true;
      ok(true, 'Promise.sleep() is fulfilled after delay.');
      QUnit.start();
    });

    // Tick the clock for 2 seconds and check if the promise is fulfilled.
    clock.tick(2);

    // Promise fulfillment always occur on a new stack.  Therefore, we will run
    // the verification in a requestAnimationFrame.
    window.requestAnimationFrame(function(){
      ok(!isCalled, 'Promise.sleep() should not be fulfilled prematurely.');
      clock.tick(101);
    }.bind(this));
});

QUnit.asyncTest('Promise.negate should fulfill iff the promise does not.',
  function() {

    base.Promise.negate(Promise.reject()).then(
        ok.bind(null, true),
        ok.bind(null, false));
    base.Promise.negate(Promise.resolve()).then(
        ok.bind(null, false),
        ok.bind(null, true));
    window.requestAnimationFrame(function(){
      QUnit.start();
    });
});

module('base.Deferred');

QUnit.asyncTest('resolve() should fulfill the underlying promise.', function() {
  function async() {
    var deferred = new base.Deferred();
    deferred.resolve('bar');
    return deferred.promise();
  }

  async().then(function(value){
    QUnit.equal(value, 'bar');
    QUnit.start();
  }, function() {
    QUnit.ok(false, 'The reject handler should not be invoked.');
  });
});

QUnit.asyncTest('reject() should fail the underlying promise.', function() {
  function async() {
    var deferred = new base.Deferred();
    deferred.reject('bar');
    return deferred.promise();
  }

  async().then(function(){
    QUnit.ok(false, 'The then handler should not be invoked.');
  }, function(value) {
    QUnit.equal(value, 'bar');
    QUnit.start();
  });
});


var source = null;
var listener = null;

module('base.EventSource', {
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

test('raiseEvent() should invoke the listener', function() {
  source.raiseEvent('foo');
  sinon.assert.called(listener);
});

test('raiseEvent() should invoke the listener with the correct event data',
  function() {
    var data = {
      field: 'foo'
    };
    source.raiseEvent('foo', data);
    sinon.assert.calledWith(listener, data);
});

test(
  'raiseEvent() should not invoke listeners that are added during raiseEvent',
  function() {
    source.addEventListener('foo', function() {
      source.addEventListener('foo', function() {
        ok(false);
      });
      ok(true);
    });
    source.raiseEvent('foo');
});

test('raiseEvent() should not invoke listeners of a different event',
  function() {
    source.raiseEvent('bar');
    sinon.assert.notCalled(listener);
});

test('raiseEvent() should assert when undeclared events are raised',
  function() {
    sinon.spy(base.debug, 'assert');
    try {
      source.raiseEvent('undefined');
    } catch (e) {
    } finally {
      sinon.assert.called(base.debug.assert);
      base.debug.assert.restore();
    }
});

test(
  'removeEventListener() should not invoke the listener in subsequent ' +
  'calls to |raiseEvent|',
  function() {
    source.raiseEvent('foo');
    sinon.assert.calledOnce(listener);

    source.removeEventListener('foo', listener);
    source.raiseEvent('foo');
    sinon.assert.calledOnce(listener);
});

test('removeEventListener() should work even if the listener ' +
  'is removed during |raiseEvent|',
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

test('encodeUtf8() can encode UTF8 strings', function() {
  function toJsArray(arrayBuffer) {
    var result = [];
    var array = new Uint8Array(arrayBuffer);
    for (var i = 0; i < array.length; ++i) {
      result.push(array[i]);
    }
    return result;
  }

  // ASCII.
  QUnit.deepEqual(toJsArray(base.encodeUtf8("ABC")), [0x41, 0x42, 0x43]);

  // Some arbitrary characters from the basic Unicode plane.
  QUnit.deepEqual(
      toJsArray(base.encodeUtf8("挂Ѓф")),
      [/* 挂 */ 0xE6, 0x8C, 0x82, /* Ѓ */ 0xD0, 0x83, /* ф */ 0xD1, 0x84]);

  // Unicode surrogate pair for U+1F603.
  QUnit.deepEqual(toJsArray(base.encodeUtf8("😃")),
                  [0xF0, 0x9F, 0x98, 0x83]);
});

test('decodeUtf8() can decode UTF8 strings', function() {
  // ASCII.
  QUnit.equal(base.decodeUtf8(new Uint8Array([0x41, 0x42, 0x43]).buffer),
              "ABC");

  // Some arbitrary characters from the basic Unicode plane.
  QUnit.equal(
      base.decodeUtf8(
          new Uint8Array([/* 挂 */ 0xE6, 0x8C, 0x82,
                          /* Ѓ */ 0xD0, 0x83,
                          /* ф */ 0xD1, 0x84]).buffer),
      "挂Ѓф");

  // Unicode surrogate pair for U+1F603.
  QUnit.equal(base.decodeUtf8(new Uint8Array([0xF0, 0x9F, 0x98, 0x83]).buffer),
              "😃");
});

})();
