// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

var originalGlobalPromise = Promise;

QUnit.module('spy_promise', {
  beforeEach: function(/** QUnit.Assert*/ assert) {
    assertInitialState(assert);
    base.SpyPromise.reset(); // Defend against broken tests.
  },
  afterEach: function(/** QUnit.Assert*/ assert) {
    assertInitialState(assert);
  }
});

function assertInitialState(/** QUnit.Assert */ assert) {
  assert.equal(Promise, originalGlobalPromise);
  assert.ok(
      !base.SpyPromise.isSettleAllRunning(),
      'settleAll should not be running');
  assert.equal(
      base.SpyPromise.unsettledCount, 0,
      'base.SpyPromise.unsettledCount should be zero ' +
        'before/after any test finishes');
}

/**
 * @param {!QUnit.Assert} assert
 * @return {!Promise}
 */
function finish(assert) {
  return base.SpyPromise.settleAll().then(function() {
    assert.equal(
        base.SpyPromise.unsettledCount, 0,
        'base.SpyPromise.unsettledCount should be zero ' +
          'after settleAll finishes.');
  });
}

QUnit.test('run', function(assert) {
  var done = assert.async();
  assert.notEqual(base.SpyPromise, originalGlobalPromise);
  return base.SpyPromise.run(function() {
    assert.equal(Promise, base.SpyPromise);
    assert.equal(base.SpyPromise.unsettledCount, 0);
    var dummy1 = new Promise(function(resolve) { resolve(null); });
    assert.equal(base.SpyPromise.unsettledCount, 1);
  }).then(function() {
    assert.equal(Promise, originalGlobalPromise);
    assert.equal(base.SpyPromise.unsettledCount, 0);
    done();
  });
});

QUnit.test('activate/restore', function(assert) {
  assert.notEqual(base.SpyPromise, originalGlobalPromise);
  base.SpyPromise.activate();
  assert.notEqual(base.SpyPromise, originalGlobalPromise);
  assert.equal(base.SpyPromise.unsettledCount, 0);
  var dummy1 = new Promise(function(resolve) { resolve(null); });
  assert.equal(base.SpyPromise.unsettledCount, 1);
  base.SpyPromise.restore();
  assert.equal(Promise, originalGlobalPromise);
  return finish(assert);
});

QUnit.test('new/then', function(assert) {
  var done = assert.async();
  new base.SpyPromise(function(resolve, reject) {
    resolve('hello');
  }).then(function(/**string*/ value) {
    assert.equal(base.SpyPromise.unsettledCount, 0);
    assert.equal(value, 'hello');
    done();
  });
  assert.equal(base.SpyPromise.unsettledCount, 1);
  return finish(assert);
});

QUnit.test('new/catch', function(assert) {
  var done = assert.async();
  new base.SpyPromise(function(resolve, reject) {
    reject('hello');
  }).catch(function(/**string*/ value) {
    assert.equal(base.SpyPromise.unsettledCount, 0);
    assert.equal(value, 'hello');
    done();
  });
  assert.equal(base.SpyPromise.unsettledCount, 1);
  return finish(assert);
});

QUnit.test('new+throw/catch', function(assert) {
  var done = assert.async();
  new base.SpyPromise(function(resolve, reject) {
    throw 'hello';
  }).catch(function(/**string*/ value) {
    assert.equal(base.SpyPromise.unsettledCount, 0);
    assert.equal(value, 'hello');
    done();
  });
  assert.equal(base.SpyPromise.unsettledCount, 1);
  return finish(assert);
});

QUnit.test('resolve/then', function(assert) {
  var done = assert.async();
  base.SpyPromise.resolve('hello').then(function(/**string*/ value) {
    assert.equal(base.SpyPromise.unsettledCount, 0);
    assert.equal(value, 'hello');
    done();
  });
  assert.equal(base.SpyPromise.unsettledCount, 1);
  return finish(assert);
});

QUnit.test('reject/then', function(assert) {
  var done = assert.async();
  base.SpyPromise.reject('hello').then(null, function(/**string*/ value) {
    assert.equal(base.SpyPromise.unsettledCount, 0);
    assert.equal(value, 'hello');
    done();
  });
  assert.equal(base.SpyPromise.unsettledCount, 1);
  return finish(assert);
});

QUnit.test('reject/catch', function(assert) {
  var done = assert.async();
  base.SpyPromise.reject('hello').catch(function(/**string*/ value) {
    assert.equal(base.SpyPromise.unsettledCount, 0);
    assert.equal(value, 'hello');
    done();
  });
  assert.equal(base.SpyPromise.unsettledCount, 1);
  return finish(assert);
});

QUnit.test('all', function(assert) {
  var done = assert.async();
  base.SpyPromise.all([Promise.resolve(1), Promise.resolve(2)]).
      then(
          /** @param {string} value */
          function(value) {
            assert.equal(base.SpyPromise.unsettledCount, 0);
            assert.deepEqual(value, [1, 2]);
            done();
          });
  assert.equal(base.SpyPromise.unsettledCount, 1);
  return finish(assert);
});

QUnit.test('race', function(assert) {
  var done = assert.async();
  var fast = Promise.resolve('fast');
  var slow = new Promise(function() {}); // never settled
  base.SpyPromise.race([fast, slow]).
      then(function(/**string*/ value) {
        assert.equal(base.SpyPromise.unsettledCount, 0);
        assert.equal(value, 'fast');
        done();
      });
  assert.equal(base.SpyPromise.unsettledCount, 1);
  return finish(assert);
});

QUnit.test('resolve/then/then', function(assert) {
  var done = assert.async();
  base.SpyPromise.resolve('hello').then(function(/**string*/ value) {
    assert.equal(value, 'hello');
    return 'goodbye';
  }).then(function(/**string*/ value) {
    assert.equal(value, 'goodbye');
    done();
  });
  return finish(assert);
});


QUnit.test('resolve/then+throw/catch', function(assert) {
  var done = assert.async();
  base.SpyPromise.resolve('hello').then(function(/**string*/ value) {
    assert.equal(value, 'hello');
    throw 'goodbye';
  }).catch(function(/**string*/ value) {
    assert.equal(value, 'goodbye');
    done();
  });
  return finish(assert);
});

QUnit.test('reject/catch/then', function(assert) {
  var done = assert.async();
  base.SpyPromise.reject('hello').catch(function(/**string*/ value) {
    assert.equal(value, 'hello');
    return 'goodbye';
  }).then(function(/**string*/ value) {
    assert.equal(value, 'goodbye');
    done();
  });
  return finish(assert);
});


QUnit.test('reject/catch+throw/catch', function(assert) {
  var done = assert.async();
  base.SpyPromise.reject('hello').catch(function(/**string*/ value) {
    assert.equal(value, 'hello');
    throw 'goodbye';
  }).catch(function(/**string*/ value) {
    assert.equal(value, 'goodbye');
    done();
  });
  return finish(assert);
});

QUnit.test('settleAll timeout = 100', function(assert) {
  var done = assert.async();
  var startTime = Date.now();
  var neverResolved = new base.SpyPromise(function() {});
  return base.SpyPromise.settleAll(100).catch(function(error) {
    assert.ok(error instanceof Error);
    assert.ok(startTime + 200 < Date.now());
    done();
  });
});

QUnit.test('settleAll timeout = 500', function(assert) {
  var done = assert.async();
  var startTime = Date.now();
  var neverResolved = new base.SpyPromise(function() {});
  return base.SpyPromise.settleAll(500).catch(function(error) {
    assert.ok(startTime + 750 < Date.now());
    done();
  });
});

QUnit.test('settleAll timeout = 1000', function(assert) {
  var done = assert.async();
  var startTime = Date.now();
  var neverResolved = new base.SpyPromise(function() {});
  return base.SpyPromise.settleAll(1000).catch(function(error) {
    assert.ok(startTime + 1500 < Date.now());
    done();
  });
});

})();