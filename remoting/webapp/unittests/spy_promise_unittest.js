// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

var originalGlobalPromise = Promise;

QUnit.module('spy_promise', {
  beforeEach: function() {
    assertInitialState();
    SpyPromise.reset(); // Defend against broken tests.
  },
  afterEach: function() {
    assertInitialState();
  }
});

function assertInitialState() {
  QUnit.equal(Promise, originalGlobalPromise);
  QUnit.equal(
      SpyPromise['settleAllPromise_'], null,
      'settleAll should not be running');
  QUnit.equal(
      SpyPromise.unsettledCount, 0,
      'SpyPromise.unsettledCount should be zero ' +
        'before/after any test finishes');
}

/**
 * @return {!Promise}
 */
function finish() {
  return SpyPromise.settleAll().then(function() {
    QUnit.equal(
        SpyPromise.unsettledCount, 0,
        'SpyPromise.unsettledCount should be zero after settleAll finishes.');
  });
};

QUnit.test('run', function(assert) {
  var done = assert.async();
  QUnit.notEqual(SpyPromise, originalGlobalPromise);
  return SpyPromise.run(function() {
    QUnit.equal(Promise, SpyPromise);
    QUnit.equal(SpyPromise.unsettledCount, 0);
    var dummy1 = new Promise(function(resolve) { resolve(null); });
    QUnit.equal(SpyPromise.unsettledCount, 1);
  }).then(function() {
    QUnit.equal(Promise, originalGlobalPromise);
    QUnit.equal(SpyPromise.unsettledCount, 0);
    done();
  });
});

QUnit.test('activate/restore', function() {
  QUnit.notEqual(SpyPromise, originalGlobalPromise);
  SpyPromise.activate();
  QUnit.notEqual(SpyPromise, originalGlobalPromise);
  QUnit.equal(SpyPromise.unsettledCount, 0);
  var dummy1 = new Promise(function(resolve) { resolve(null); });
  QUnit.equal(SpyPromise.unsettledCount, 1);
  SpyPromise.restore();
  QUnit.equal(Promise, originalGlobalPromise);
  return finish();
});

QUnit.test('new/then', function(assert) {
  var done = assert.async();
  new SpyPromise(function(resolve, reject) {
    resolve('hello');
  }).then(function(/**string*/ value) {
    QUnit.equal(SpyPromise.unsettledCount, 0);
    QUnit.equal(value, 'hello');
    done();
  });
  QUnit.equal(SpyPromise.unsettledCount, 1);
  return finish();
});

QUnit.test('new/catch', function(assert) {
  var done = assert.async();
  new SpyPromise(function(resolve, reject) {
    reject('hello');
  }).catch(function(/**string*/ value) {
    QUnit.equal(SpyPromise.unsettledCount, 0);
    QUnit.equal(value, 'hello');
    done();
  });
  QUnit.equal(SpyPromise.unsettledCount, 1);
  return finish();
});

QUnit.test('new+throw/catch', function(assert) {
  var done = assert.async();
  new SpyPromise(function(resolve, reject) {
    throw 'hello';
  }).catch(function(/**string*/ value) {
    QUnit.equal(SpyPromise.unsettledCount, 0);
    QUnit.equal(value, 'hello');
    done();
  });
  QUnit.equal(SpyPromise.unsettledCount, 1);
  return finish();
});

QUnit.test('resolve/then', function(assert) {
  var done = assert.async();
  SpyPromise.resolve('hello').then(function(/**string*/ value) {
    QUnit.equal(SpyPromise.unsettledCount, 0);
    QUnit.equal(value, 'hello');
    done();
  });
  QUnit.equal(SpyPromise.unsettledCount, 1);
  return finish();
});

QUnit.test('reject/then', function(assert) {
  var done = assert.async();
  SpyPromise.reject('hello').then(null, function(/**string*/ value) {
    QUnit.equal(SpyPromise.unsettledCount, 0);
    QUnit.equal(value, 'hello');
    done();
  });
  QUnit.equal(SpyPromise.unsettledCount, 1);
  return finish();
});

QUnit.test('reject/catch', function(assert) {
  var done = assert.async();
  SpyPromise.reject('hello').catch(function(/**string*/ value) {
    QUnit.equal(SpyPromise.unsettledCount, 0);
    QUnit.equal(value, 'hello');
    done();
  });
  QUnit.equal(SpyPromise.unsettledCount, 1);
  return finish();
});

QUnit.test('all', function(assert) {
  var done = assert.async();
  SpyPromise.all([Promise.resolve(1), Promise.resolve(2)]).
      then(function(/**string*/ value) {
        QUnit.equal(SpyPromise.unsettledCount, 0);
        QUnit.deepEqual(value, [1, 2]);
        done();
      });
  QUnit.equal(SpyPromise.unsettledCount, 1);
  return finish();
});

QUnit.test('race', function(assert) {
  var done = assert.async();
  var fast = Promise.resolve('fast');
  var slow = new Promise(function() {}); // never settled
  SpyPromise.race([fast, slow]).
      then(function(/**string*/ value) {
        QUnit.equal(SpyPromise.unsettledCount, 0);
        QUnit.equal(value, 'fast');
        done();
      });
  QUnit.equal(SpyPromise.unsettledCount, 1);
  return finish();
});

QUnit.test('resolve/then/then', function(assert) {
  var done = assert.async();
  SpyPromise.resolve('hello').then(function(/**string*/ value) {
    QUnit.equal(value, 'hello');
    return 'goodbye';
  }).then(function(/**string*/ value) {
    QUnit.equal(value, 'goodbye');
    done();
  });
  return finish();
});


QUnit.test('resolve/then+throw/catch', function(assert) {
  var done = assert.async();
  SpyPromise.resolve('hello').then(function(/**string*/ value) {
    QUnit.equal(value, 'hello');
    throw 'goodbye';
  }).catch(function(/**string*/ value) {
    QUnit.equal(value, 'goodbye');
    done();
  });
  return finish();
});

QUnit.test('reject/catch/then', function(assert) {
  var done = assert.async();
  SpyPromise.reject('hello').catch(function(/**string*/ value) {
    QUnit.equal(value, 'hello');
    return 'goodbye';
  }).then(function(/**string*/ value) {
    QUnit.equal(value, 'goodbye');
    done();
  });
  return finish();
});


QUnit.test('reject/catch+throw/catch', function(assert) {
  var done = assert.async();
  SpyPromise.reject('hello').catch(function(/**string*/ value) {
    QUnit.equal(value, 'hello');
    throw 'goodbye';
  }).catch(function(/**string*/ value) {
    QUnit.equal(value, 'goodbye');
    done();
  });
  return finish();
});

QUnit.test('settleAll timeout = 100', function(assert) {
  var done = assert.async();
  var startTime = Date.now();
  var neverResolved = new SpyPromise(function() {});
  return SpyPromise.settleAll(100).catch(function(error) {
    QUnit.ok(error instanceof Error);
    QUnit.ok(startTime + 200 < Date.now());
    done();
  });
});

QUnit.test('settleAll timeout = 500', function(assert) {
  var done = assert.async();
  var startTime = Date.now();
  var neverResolved = new SpyPromise(function() {});
  return SpyPromise.settleAll(500).catch(function(error) {
    QUnit.ok(startTime + 750 < Date.now());
    done();
  });
});

QUnit.test('settleAll timeout = 1000', function(assert) {
  var done = assert.async();
  var startTime = Date.now();
  var neverResolved = new SpyPromise(function() {});
  return SpyPromise.settleAll(1000).catch(function(error) {
    QUnit.ok(startTime + 1500 < Date.now());
    done();
  });
});

})();