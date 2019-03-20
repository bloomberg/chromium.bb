// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @type {?string} Result
 */
let result;

/**
 * @type {!PromiseSlot} Test target.
 */
let slot;

function setUp() {
  slot = new PromiseSlot(
      value => {
        result = 'fulfilled:' + value;
      },
      value => {
        result = 'rejected:' + value;
      });
  result = null;
}

function testPromiseSlot(callback) {
  const fulfilledPromise = Promise.resolve('fulfilled');
  const rejectedPromise = Promise.reject('rejected');
  slot.setPromise(fulfilledPromise);
  reportPromise(
      fulfilledPromise
          .then(() => {
            assertEquals('fulfilled:fulfilled', result);
            slot.setPromise(rejectedPromise);
            return rejectedPromise;
          })
          .then(
              () => {
                // Should not reach here.
                assertTrue(false);
              },
              () => {
                assertEquals('rejected:rejected', result);
              }),
      callback);
}

function testPromiseSlotReassignBeforeCompletion(callback) {
  let fulfillComputation;
  const computingPromise = new Promise((fulfill, reject) => {
    fulfillComputation = fulfill;
  });
  const fulfilledPromise = Promise.resolve('fulfilled');

  slot.setPromise(computingPromise);
  // Reassign promise.
  slot.setPromise(fulfilledPromise);
  reportPromise(
      fulfilledPromise
          .then(() => {
            assertEquals('fulfilled:fulfilled', result);
            fulfillComputation('fulfilled after detached');
            return computingPromise;
          })
          .then(value => {
            assertEquals('fulfilled after detached', value);
            // The detached promise does not affect the slot.
            assertEquals('fulfilled:fulfilled', result);
          }),
      callback);
}

function testPromiseSlotReassignBeforeCompletionWithCancel(callback) {
  let rejectComputation;
  const computingPromise = new Promise((fulfill, reject) => {
    rejectComputation = reject;
  });
  computingPromise.cancel = () => {
    rejectComputation('cancelled');
  };
  const fulfilledPromise = Promise.resolve('fulfilled');

  slot.setPromise(computingPromise);
  slot.setPromise(fulfilledPromise);
  reportPromise(
      fulfilledPromise
          .then(() => {
            assertEquals('fulfilled:fulfilled', result);
            return computingPromise;
          })
          .then(
              () => {
                // Should not reach here.
                assertTrue(false);
              },
              value => {
                assertEquals('cancelled', value);
                // The detached promise does not affect the slot.
                assertEquals('fulfilled:fulfilled', result);
              }),
      callback);
}

function testPromiseSlotReassignNullBeforeCompletion(callback) {
  let fulfillComputation;
  const computingPromise = new Promise((fulfill, reject) => {
    fulfillComputation = fulfill;
  });

  slot.setPromise(computingPromise);
  slot.setPromise(null);
  assertEquals(null, result);

  fulfillComputation('fulfilled');
  reportPromise(
      computingPromise.then(value => {
        assertEquals('fulfilled', value);
        assertEquals(null, result);
      }),
      callback);
}
