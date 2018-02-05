// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of functions that are shared between ReadableStream and
// WritableStream.

(function(global, binding, v8) {
  'use strict';

  // Common private symbols. These correspond directly to internal slots in the
  // standard. "[[X]]" in the standard is spelt _X here.
  const _queue = v8.createPrivateSymbol('[[queue]]');
  const _queueTotalSize = v8.createPrivateSymbol('[[queueTotalSize]]');

  // Javascript functions. It is important to use these copies for security and
  // robustness. See "V8 Extras Design Doc", section "Security Considerations".
  // https://docs.google.com/document/d/1AT5-T0aHGp7Lt29vPWFr2-qG8r3l9CByyvKwEuA8Ec0/edit#heading=h.9yixony1a18r
  const Boolean = global.Boolean;
  const Number = global.Number;
  const Number_isFinite = Number.isFinite;
  const Number_isNaN = Number.isNaN;

  const RangeError = global.RangeError;
  const TypeError = global.TypeError;

  const hasOwnProperty = v8.uncurryThis(global.Object.hasOwnProperty);

  function hasOwnPropertyNoThrow(x, property) {
    // The cast of |x| to Boolean will eliminate undefined and null, which would
    // cause hasOwnProperty to throw a TypeError, as well as some other values
    // that can't be objects and so will fail the check anyway.
    return Boolean(x) && hasOwnProperty(x, property);
  }

  //
  // Assert is not normally enabled, to avoid the space and time overhead. To
  // enable, uncomment this definition and then in the file you wish to enable
  // asserts for, uncomment the assert statements and add this definition:
  // const assert = pred => binding.SimpleAssert(pred);
  //
  // binding.SimpleAssert = pred => {
  //   if (pred) {
  //     return;
  //   }
  //   v8.log('\n\n\n  *** ASSERTION FAILURE ***\n\n');
  //   v8.logStackTrace();
  //   v8.log('**************************************************\n\n');
  //   class StreamsAssertionError extends Error {}
  //   throw new StreamsAssertionError('Streams Assertion Failure');
  // };

  //
  // Promise-manipulation functions
  //

  // Not exported.
  function streamInternalError() {
    throw new RangeError('Stream API Internal Error');
  }

  function rejectPromise(p, reason) {
    if (!v8.isPromise(p)) {
      streamInternalError();
    }
    v8.rejectPromise(p, reason);
  }

  function resolvePromise(p, value) {
    if (!v8.isPromise(p)) {
      streamInternalError();
    }
    v8.resolvePromise(p, value);
  }

  function markPromiseAsHandled(p) {
    if (!v8.isPromise(p)) {
      streamInternalError();
    }
    v8.markPromiseAsHandled(p);
  }

  function promiseState(p) {
    if (!v8.isPromise(p)) {
      streamInternalError();
    }
    return v8.promiseState(p);
  }

  //
  // Queue-with-Sizes Operations
  //
  function DequeueValue(container) {
    // assert(
    //     hasOwnProperty(container, _queue) &&
    //         hasOwnProperty(container, _queueTotalSize),
    //     '_container_ has [[queue]] and [[queueTotalSize]] internal slots.');
    // assert(container[_queue].length !== 0,
    //        '_container_.[[queue]] is not empty.');
    const pair = container[_queue].shift();
    container[_queueTotalSize] -= pair.size;
    if (container[_queueTotalSize] < 0) {
      container[_queueTotalSize] = 0;
    }
    return pair.value;
  }

  function EnqueueValueWithSize(container, value, size) {
    // assert(
    //     hasOwnProperty(container, _queue) &&
    //         hasOwnProperty(container, _queueTotalSize),
    //     '_container_ has [[queue]] and [[queueTotalSize]] internal 'slots.');
    size = Number(size);
    if (!IsFiniteNonNegativeNumber(size)) {
      throw new RangeError(binding.streamErrors.invalidSize);
    }

    container[_queue].push({value, size});
    container[_queueTotalSize] += size;
  }

  function PeekQueueValue(container) {
    // assert(
    //     hasOwnProperty(container, _queue) &&
    //         hasOwnProperty(container, _queueTotalSize),
    //     '_container_ has [[queue]] and [[queueTotalSize]] internal slots.');
    // assert(container[_queue].length !== 0,
    //        '_container_.[[queue]] is not empty.');
    const pair = container[_queue].peek();
    return pair.value;
  }

  function ResetQueue(container) {
    // assert(
    //     hasOwnProperty(container, _queue) &&
    //         hasOwnProperty(container, _queueTotalSize),
    //     '_container_ has [[queue]] and [[queueTotalSize]] internal slots.');
    container[_queue] = new binding.SimpleQueue();
    container[_queueTotalSize] = 0;
  }

  // Not exported.
  function IsFiniteNonNegativeNumber(v) {
    return Number_isFinite(v) && v >= 0;
  }

  function ValidateAndNormalizeQueuingStrategy(size, highWaterMark) {
    if (size !== undefined && typeof size !== 'function') {
      throw new TypeError(binding.streamErrors.sizeNotAFunction);
    }

    highWaterMark = Number(highWaterMark);
    if (Number_isNaN(highWaterMark)) {
      throw new RangeError(binding.streamErrors.invalidHWM);
    }
    if (highWaterMark < 0) {
      throw new RangeError(binding.streamErrors.invalidHWM);
    }

    return {size, highWaterMark};
  }

  //
  // Invoking functions.
  // These differ from the Invoke versions in the spec in that they take a fixed
  // number of arguments rather than a list, and also take a name to be used for
  // the function on error.
  //

  // Internal utility functions. Not exported.
  const callFunction = v8.uncurryThis(global.Function.prototype.call);
  const errTmplMustBeFunctionOrUndefined = name =>
        `${name} must be a function or undefined`;
  const Promise_resolve = Promise.resolve.bind(Promise);
  const Promise_reject = Promise.reject.bind(Promise);

  function resolveMethod(O, P, nameForError) {
    const method = O[P];

    if (typeof method !== 'function' && typeof method !== 'undefined') {
      throw new TypeError(errTmplMustBeFunctionOrUndefined(nameForError));
    }

    return method;
  }

  // Modified from InvokeOrNoop in spec. Takes 1 argument.
  function CallOrNoop1(O, P, arg0, nameForError) {
    const method = resolveMethod(O, P, nameForError);
    if (method === undefined) {
      return undefined;
    }

    return callFunction(method, O, arg0);
  }

  // Modified from PromiseInvokeOrNoop in spec. Version with no arguments.
  function PromiseCallOrNoop0(O, P, nameForError) {
    try {
      const method = resolveMethod(O, P, nameForError);
      if (method === undefined) {
        return Promise_resolve();
      }

      return Promise_resolve(callFunction(method, O));
    } catch (e) {
      return Promise_reject(e);
    }
  }

  // Modified from PromiseInvokeOrNoop in spec. Version with 1 argument.
  function PromiseCallOrNoop1(O, P, arg0, nameForError) {
    try {
      return Promise_resolve(CallOrNoop1(O, P, arg0, nameForError));
    } catch (e) {
      return Promise_reject(e);
    }
  }

  // Modified from PromiseInvokeOrNoop in spec. Version with 2 arguments.
  function PromiseCallOrNoop2(O, P, arg0, arg1, nameForError) {
    try {
      const method = resolveMethod(O, P, nameForError);
      if (method === undefined) {
        return Promise_resolve();
      }

      return Promise_resolve(callFunction(method, O, arg0, arg1));
    } catch (e) {
      return Promise_reject(e);
    }
  }

  binding.streamOperations = {
    _queue,
    _queueTotalSize,
    hasOwnPropertyNoThrow,
    rejectPromise,
    resolvePromise,
    markPromiseAsHandled,
    promiseState,
    DequeueValue,
    EnqueueValueWithSize,
    PeekQueueValue,
    ResetQueue,
    ValidateAndNormalizeQueuingStrategy,
    CallOrNoop1,
    PromiseCallOrNoop0,
    PromiseCallOrNoop1,
    PromiseCallOrNoop2
  };
});
