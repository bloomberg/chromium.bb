// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** @suppress {duplicate} */
var base = base || {};

(function() {
/**
 * A wrapper around a Promise object that keeps track of all
 * outstanding promises.  This function is written to serve as a
 * drop-in replacement for the native Promise constructor.  To create
 * a SpyPromise from an existing native Promise, use
 * SpyPromise.resolve.
 *
 * Note that this is a pseudo-constructor that actually returns a
 * regular promise with appropriate handlers attached.  This detail
 * should be transparent when SpyPromise.activate has been called.
 *
 * The normal way to use this class is within a call to
 * SpyPromise.run, for example:
 *
 *   base.SpyPromise.run(function() {
 *     myCodeThatUsesPromises();
 *   });
 *   base.SpyPromise.settleAll().then(function() {
 *     console.log('All promises have been settled!');
 *   });
 *
 * @constructor
 * @extends {Promise}
 * @param {function(function(?):?, function(*):?):?} func A function
 *     of the same type used as an argument to the native Promise
 *     constructor, in other words, a function which is called
 *     immediately, and whose arguments are a resolve function and a
 *     reject function.
 */
base.SpyPromise = function(func) {
  var unsettled = new RealPromise(func);
  var unsettledId = remember(unsettled);
  return unsettled.then(function(/** * */value) {
    forget(unsettledId);
    return value;
  }, function(error) {
    forget(unsettledId);
    throw error;
  });
};

/**
 * The real promise constructor.  Needed because it is normally hidden
 * by SpyPromise.activate or SpyPromise.run.
 * @const
 */
var RealPromise = Promise;

/**
 * The real window.setTimeout method.  Needed because some test
 * frameworks like to replace this method with a fake implementation.
 * @const
 */
var realSetTimeout = window.setTimeout.bind(window);

/**
 * The number of unsettled promises.
 * @type {number}
 */
base.SpyPromise.unsettledCount;  // initialized by reset()

/**
 * A collection of all unsettled promises.
 * @type {!Object<number,!Promise>}
 */
var unsettled;  // initialized by reset()

/**
 * A counter used to assign ID numbers to new SpyPromise objects.
 * @type {number}
 */
var nextPromiseId;  // initialized by reset()

/**
 * A promise returned by SpyPromise.settleAll.
 * @type {Promise<null>}
 */
var settleAllPromise;  // initialized by reset()

/**
 * Records an unsettled promise.
 *
 * @param {!Promise} unsettledPromise
 * @return {number} The ID number to be passed to forget_.
 */
function remember(unsettledPromise) {
  var id = nextPromiseId++;
  if (unsettled[id] != null) {
    throw Error('Duplicate ID: ' + id);
  }
  base.SpyPromise.unsettledCount++;
  unsettled[id] = unsettledPromise;
  return id;
};

/**
 * Forgets a promise.  Called after the promise has been settled.
 *
 * @param {number} id
 * @private
 */
function forget(id) {
  console.assert(unsettled[id] != null, 'No such Promise: ' + id + '.');
  base.SpyPromise.unsettledCount--;
  delete unsettled[id];
};

/**
 * Forgets about all unsettled promises.
 */
base.SpyPromise.reset = function() {
  base.SpyPromise.unsettledCount = 0;
  unsettled = {};
  nextPromiseId = 0;
  settleAllPromise = null;
};

// Initialize static variables.
base.SpyPromise.reset();

/**
 * Tries to wait until all promises has been settled.
 *
 * @param {number=} opt_maxTimeMs The maximum number of milliseconds
 *     (approximately) to wait (default: 1000).
 * @return {!Promise<null>} A real promise that is resolved when all
 *     SpyPromises have been settled, or rejected after opt_maxTimeMs
 *     milliseconds have elapsed.
 */
base.SpyPromise.settleAll = function(opt_maxTimeMs) {
  if (settleAllPromise) {
    return settleAllPromise;
  }

  var maxDelay = opt_maxTimeMs == null ? 1000 : opt_maxTimeMs;

  /**
   * @param {number} count
   * @param {number} totalDelay
   * @return {!Promise<null>}
   */
  function loop(count, totalDelay) {
    return new RealPromise(function(resolve, reject) {
      if (base.SpyPromise.unsettledCount == 0) {
        settleAllPromise = null;
        resolve(null);
      } else if (totalDelay > maxDelay) {
        settleAllPromise = null;
        base.SpyPromise.reset();
        reject(new Error('base.SpyPromise.settleAll timed out'));
      } else {
        // This implements quadratic backoff according to Euler's
        // triangular number formula.
        var delay = count;

        // Must jump through crazy hoops to get a real timer in a unit test.
        realSetTimeout(function() {
          resolve(loop(
              count + 1,
              delay + totalDelay));
        }, delay);
      }
    });
  };

  // An extra promise needed here to prevent the loop function from
  // finishing before settleAllPromise is set.  If that happens,
  // settleAllPromise will never be reset to null.
  settleAllPromise = RealPromise.resolve().then(function() {
    return loop(0, 0);
  });
  return settleAllPromise;
};

/**
 * Only for testing this class.  Do not use.
 * @returns {boolean} True if settleAll is executing.
 */
base.SpyPromise.isSettleAllRunning = function() {
  return settleAllPromise != null;
};

/**
 * Wrapper for Promise.resolve.
 *
 * @param {*} value
 * @return {!base.SpyPromise}
 */
base.SpyPromise.resolve = function(value) {
  return new base.SpyPromise(function(resolve, reject) {
    resolve(value);
  });
};

/**
 * Wrapper for Promise.reject.
 *
 * @param {*} value
 * @return {!base.SpyPromise}
 */
base.SpyPromise.reject = function(value) {
  return new base.SpyPromise(function(resolve, reject) {
    reject(value);
  });
};

/**
 * Wrapper for Promise.all.
 *
 * @param  {!Array<Promise>} promises
 * @return {!base.SpyPromise}
 */
base.SpyPromise.all = function(promises) {
  return base.SpyPromise.resolve(RealPromise.all(promises));
};

/**
 * Wrapper for Promise.race.
 *
 * @param {!Array<Promise>} promises
 * @return {!base.SpyPromise}
 */
base.SpyPromise.race = function(promises) {
  return base.SpyPromise.resolve(RealPromise.race(promises));
};

/**
 * Sets Promise = base.SpyPromise.  Must not be called more than once
 * without an intervening call to restore().
 */
base.SpyPromise.activate = function() {
  if (settleAllPromise) {
    throw Error('called base.SpyPromise.activate while settleAll is running');
  }
  if (Promise === base.SpyPromise) {
    throw Error('base.SpyPromise is already active');
  }
  Promise = /** @type {function(new:Promise)} */(base.SpyPromise);
};

/**
 * Restores the original value of Promise.
 */
base.SpyPromise.restore = function() {
  if (settleAllPromise) {
    throw Error('called base.SpyPromise.restore while settleAll is running');
  }
  if (Promise === base.SpyPromise) {
    Promise = RealPromise;
  } else if (Promise === RealPromise) {
    throw new Error('base.SpyPromise is not active.');
  } else {
    throw new Error('Something fishy is going on.');
  }
};

/**
 * Calls func with Promise equal to base.SpyPromise.
 *
 * @param {function():void} func A function which is expected to
 *     create one or more promises.
 * @param {number=} opt_timeoutMs An optional timeout specifying how
 *     long to wait for promise chains started in func to be settled.
 *     (default: 1000 ms)
 * @return {!Promise<null>} A promise that is resolved after every
 *     promise chain started in func is fully settled, or rejected
 *     after a opt_timeoutMs.  In any case, the original value of the
 *     Promise constructor is restored before this promise is settled.
 */
base.SpyPromise.run = function(func, opt_timeoutMs) {
  base.SpyPromise.activate();
  try {
    func();
  } finally {
    return base.SpyPromise.settleAll(opt_timeoutMs).then(function() {
      base.SpyPromise.restore();
      return null;
    }, function(error) {
      base.SpyPromise.restore();
      throw error;
    });
  }
};
})();
