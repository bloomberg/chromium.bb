// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

QUnit.module('base');

QUnit.test('mix(dest, src) should copy properties from |src| to |dest|',
  function(assert) {
    var src = { a: 'a', b: 'b'};
    var dest = { c: 'c'};

    base.mix(dest, src);
    assert.deepEqual(dest, {a: 'a', b: 'b', c: 'c'});
});

QUnit.test('mix(dest, src) should assert if properties are overwritten',
  function(assert) {
    var src = { a: 'a', b: 'b'};
    var dest = { a: 'a'};

    sinon.stub(base.debug, 'assert');

    try {
      base.mix(dest, src);
    } catch (e) {
    } finally {
      sinon.assert.called(base.debug.assert);
      $testStub(base.debug.assert).restore();
    }
});

QUnit.test('values(obj) should return an array containing the values of |obj|',
  function(assert) {
    var output = base.values({ a: 'a', b: 'b'});

    assert.notEqual(output.indexOf('a'), -1, '"a" should be in the output');
    assert.notEqual(output.indexOf('b'), -1, '"b" should be in the output');
});

QUnit.test('deepCopy(obj) should return null on NaN and undefined',
  function(assert) {
    assert.equal(base.deepCopy(NaN), null);
    assert.equal(base.deepCopy(undefined), null);
});

QUnit.test('deepCopy(obj) should copy primitive types recursively',
  function(assert) {
    assert.equal(base.deepCopy(1), 1);
    assert.equal(base.deepCopy('hello'), 'hello');
    assert.equal(base.deepCopy(false), false);
    assert.equal(base.deepCopy(null), null);
    assert.deepEqual(base.deepCopy([1, 2]), [1, 2]);
    assert.deepEqual(base.deepCopy({'key': 'value'}), {'key': 'value'});
    assert.deepEqual(base.deepCopy(
      {'key': {'key_nested': 'value_nested'}}),
      {'key': {'key_nested': 'value_nested'}}
    );
    assert.deepEqual(base.deepCopy([1, [2, [3]]]), [1, [2, [3]]]);
});

QUnit.test('modify the original after deepCopy(obj) should not affect the copy',
  function(assert) {
    var original = [1, 2, 3, 4];
    var copy = base.deepCopy(original);
    original[2] = 1000;
    assert.deepEqual(copy, [1, 2, 3, 4]);
});

QUnit.test('dispose(obj) should invoke the dispose method on |obj|',
  function(assert) {
    /**
     * @constructor
     * @implements {base.Disposable}
     */
    base.MockDisposable = function() {};
    base.MockDisposable.prototype.dispose = sinon.spy();

    var obj = new base.MockDisposable();
    base.dispose(obj);
    sinon.assert.called(obj.dispose);
});

QUnit.test('dispose(obj) should not crash if |obj| is null',
  function(assert) {
    assert.expect(0);
    base.dispose(null);
});

QUnit.test(
  'urljoin(url, opt_param) should return url if |opt_param| is missing',
  function(assert) {
    assert.equal(
        base.urlJoin('http://www.chromium.org'), 'http://www.chromium.org');
});

QUnit.test('urljoin(url, opt_param) should urlencode |opt_param|',
  function(assert) {
    var result = base.urlJoin('http://www.chromium.org', {
      a: 'a',
      foo: 'foo',
      escapist: ':/?#[]@$&+,;='
    });
    assert.equal(
        result,
        'http://www.chromium.org?a=a&foo=foo' +
        '&escapist=%3A%2F%3F%23%5B%5D%40%24%26%2B%2C%3B%3D');
});

QUnit.test('escapeHTML(str) should escape special characters', function(assert){
  assert.equal(
    base.escapeHTML('<script>alert("hello")</script>'),
    '&lt;script&gt;alert("hello")&lt;/script&gt;');
});

QUnit.test('Promise.sleep(delay) should fulfill the promise after |delay|',
  /**
   * 'this' is not defined for jscompile, so it can't figure out the type of
   * this.clock.
   * @suppress {reportUnknownTypes|checkVars|checkTypes}
   */
  function(assert) {
    var isCalled = false;
    var clock = /** @type {QUnit.Clock} */ (this.clock);

    var promise = base.Promise.sleep(100).then(function(){
      isCalled = true;
      assert.ok(true, 'Promise.sleep() is fulfilled after delay.');
    });

    // Tick the clock for 2 seconds and check if the promise is fulfilled.
    clock.tick(2);

    // Promise fulfillment always occur on a new stack.  Therefore, we will run
    // the verification in a requestAnimationFrame.
    window.requestAnimationFrame(function(){
      assert.ok(
          !isCalled, 'Promise.sleep() should not be fulfilled prematurely.');
      clock.tick(101);
    });

    return promise;
});

QUnit.test('Promise.negate should fulfill iff the promise does not.',
  function(assert) {
    return base.Promise.negate(Promise.reject())
    .then(function() {
      assert.ok(true);
    }).catch(function() {
      assert.ok(false);
    }).then(function() {
      return base.Promise.negate(Promise.resolve());
    }).then(function() {
      assert.ok(false);
    }).catch(function() {
      assert.ok(true);
    });
});

QUnit.module('base.Deferred');

QUnit.test('resolve() should fulfill the underlying promise.', function(assert){
  /** @returns {Promise} */
  function async() {
    var deferred = new base.Deferred();
    deferred.resolve('bar');
    return deferred.promise();
  }

  return async().then(function(/** string */ value){
    assert.equal(value, 'bar');
  }, function() {
    assert.ok(false, 'The reject handler should not be invoked.');
  });
});

QUnit.test('reject() should fail the underlying promise.', function(assert) {
  /** @returns {Promise} */
  function async() {
    var deferred = new base.Deferred();
    deferred.reject('bar');
    return deferred.promise();
  }

  return async().then(function(){
    assert.ok(false, 'The then handler should not be invoked.');
  }, function(value) {
    assert.equal(value, 'bar');
  });
});


/** @type {base.EventSourceImpl} */
var source = null;
var listener = null;

QUnit.module('base.EventSource', {
  beforeEach: function() {
    source = new base.EventSourceImpl();
    source.defineEvents(['foo', 'bar']);
    listener = sinon.spy();
    source.addEventListener('foo', listener);
  },
  afterEach: function() {
    source = null;
    listener = null;
  }
});

QUnit.test('raiseEvent() should invoke the listener', function() {
  source.raiseEvent('foo');
  sinon.assert.called(listener);
});

QUnit.test(
  'raiseEvent() should invoke the listener with the correct event data',
  function(assert) {
    var data = {
      field: 'foo'
    };
    source.raiseEvent('foo', data);
    sinon.assert.calledWith(listener, data);
});

QUnit.test(
  'raiseEvent() should not invoke listeners that are added during raiseEvent',
  function(assert) {
    source.addEventListener('foo', function() {
      source.addEventListener('foo', function() {
        assert.ok(false);
      });
     assert.ok(true);
    });
    source.raiseEvent('foo');
});

QUnit.test('raiseEvent() should not invoke listeners of a different event',
  function(assert) {
    source.raiseEvent('bar');
    sinon.assert.notCalled(listener);
});

QUnit.test('raiseEvent() should assert when undeclared events are raised',
  function(assert) {
    sinon.stub(base.debug, 'assert');
    try {
      source.raiseEvent('undefined');
    } catch (e) {
    } finally {
      sinon.assert.called(base.debug.assert);
      $testStub(base.debug.assert).restore();
    }
});

QUnit.test(
  'removeEventListener() should not invoke the listener in subsequent ' +
  'calls to |raiseEvent|',
  function(assert) {
    source.raiseEvent('foo');
    sinon.assert.calledOnce(listener);

    source.removeEventListener('foo', listener);
    source.raiseEvent('foo');
    sinon.assert.calledOnce(listener);
});

QUnit.test('removeEventListener() should work even if the listener ' +
  'is removed during |raiseEvent|',
  function(assert) {
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

QUnit.test('encodeUtf8() can encode UTF8 strings', function(assert) {
  /** @type {function(ArrayBuffer):Array} */
  function toJsArray(arrayBuffer) {
    var result = [];
    var array = new Uint8Array(arrayBuffer);
    for (var i = 0; i < array.length; ++i) {
      result.push(array[i]);
    }
    return result;
  }

  // ASCII.
  assert.deepEqual(toJsArray(base.encodeUtf8("ABC")), [0x41, 0x42, 0x43]);

  // Some arbitrary characters from the basic Unicode plane.
  assert.deepEqual(
      toJsArray(base.encodeUtf8("挂Ѓф")),
      [/* 挂 */ 0xE6, 0x8C, 0x82, /* Ѓ */ 0xD0, 0x83, /* ф */ 0xD1, 0x84]);

  // Unicode surrogate pair for U+1F603.
  assert.deepEqual(toJsArray(base.encodeUtf8("😃")),
                  [0xF0, 0x9F, 0x98, 0x83]);
});

QUnit.test('decodeUtf8() can decode UTF8 strings', function(assert) {
  // ASCII.
  assert.equal(base.decodeUtf8(new Uint8Array([0x41, 0x42, 0x43]).buffer),
              "ABC");

  // Some arbitrary characters from the basic Unicode plane.
  assert.equal(
      base.decodeUtf8(
          new Uint8Array([/* 挂 */ 0xE6, 0x8C, 0x82,
                          /* Ѓ */ 0xD0, 0x83,
                          /* ф */ 0xD1, 0x84]).buffer),
      "挂Ѓф");

  // Unicode surrogate pair for U+1F603.
  assert.equal(base.decodeUtf8(new Uint8Array([0xF0, 0x9F, 0x98, 0x83]).buffer),
              "😃");
});

})();
