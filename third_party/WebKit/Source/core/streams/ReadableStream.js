// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, binding, v8) {
  'use strict';

  const readableStreamController = v8.createPrivateSymbol('[[controller]]');
  const readableStreamQueue = v8.createPrivateSymbol('[[queue]]');
  const readableStreamQueueSize =
      v8.createPrivateSymbol('[[queue]] total size');
  const readableStreamReader = v8.createPrivateSymbol('[[reader]]');
  const readableStreamState = v8.createPrivateSymbol('[[state]]');
  const readableStreamStoredError = v8.createPrivateSymbol('[[storedError]]');
  const readableStreamStrategySize = v8.createPrivateSymbol('[[strategySize]]');
  const readableStreamStrategyHWM = v8.createPrivateSymbol('[[strategyHWM]]');
  const readableStreamUnderlyingSource =
      v8.createPrivateSymbol('[[underlyingSource]]');

  const readableStreamControllerControlledReadableStream =
      v8.createPrivateSymbol('[[controlledReadableStream]]');

  const readableStreamReaderClosedPromise =
      v8.createPrivateSymbol('[[closedPromise]]');
  const readableStreamReaderOwnerReadableStream =
      v8.createPrivateSymbol('[[ownerReadableStream]]');
  const readableStreamReaderReadRequests =
      v8.createPrivateSymbol('[[readRequests]]');

  const createWithExternalControllerSentinel =
      v8.createPrivateSymbol('flag for UA-created ReadableStream to pass');

  const STATE_READABLE = 0;
  const STATE_CLOSED = 1;
  const STATE_ERRORED = 2;

  const readableStreamBits = v8.createPrivateSymbol(
      'bit field for [[started]], [[closeRequested]], [[pulling]], [[pullAgain]], [[disturbed]]');
  const STARTED = 0b1;
  const CLOSE_REQUESTED = 0b10;
  const PULLING = 0b100;
  const PULL_AGAIN = 0b1000;
  const DISTURBED = 0b10000;

  const undefined = global.undefined;
  const Infinity = global.Infinity;

  const defineProperty = global.Object.defineProperty;
  const hasOwnProperty = v8.uncurryThis(global.Object.hasOwnProperty);
  const callFunction = v8.uncurryThis(global.Function.prototype.call);

  const TypeError = global.TypeError;
  const RangeError = global.RangeError;

  const Number = global.Number;
  const Number_isNaN = Number.isNaN;
  const Number_isFinite = Number.isFinite;

  const Promise = global.Promise;
  const thenPromise = v8.uncurryThis(Promise.prototype.then);
  const Promise_resolve = v8.simpleBind(Promise.resolve, Promise);
  const Promise_reject = v8.simpleBind(Promise.reject, Promise);

  const errIllegalInvocation = 'Illegal invocation';
  const errIllegalConstructor = 'Illegal constructor';
  const errCancelLockedStream =
      'Cannot cancel a readable stream that is locked to a reader';
  const errEnqueueInCloseRequestedStream =
      'Cannot enqueue a chunk into a readable stream that is closed or has been requested to be closed';
  const errCancelReleasedReader =
      'This readable stream reader has been released and cannot be used to cancel its previous owner stream';
  const errReadReleasedReader =
      'This readable stream reader has been released and cannot be used to read from its previous owner stream';
  const errCloseCloseRequestedStream =
      'Cannot close a readable stream that has already been requested to be closed';
  const errCloseErroredStream = 'Cannot close an errored readable stream';
  const errErrorClosedStream = 'Cannot error a close readable stream';
  const errErrorErroredStream =
      'Cannot error a readable stream that is already errored';
  const errReaderConstructorBadArgument =
      'ReadableStreamReader constructor argument is not a readable stream';
  const errReaderConstructorStreamAlreadyLocked =
      'ReadableStreamReader constructor can only accept readable streams that are not yet locked to a reader';
  const errReleaseReaderWithPendingRead =
      'Cannot release a readable stream reader when it still has outstanding read() calls that have not yet settled';
  const errReleasedReaderClosedPromise =
      'This readable stream reader has been released and cannot be used to monitor the stream\'s state';
  const errInvalidSize =
      'The return value of a queuing strategy\'s size function must be a finite, non-NaN, non-negative number';
  const errSizeNotAFunction =
      'A queuing strategy\'s size property must be a function';
  const errInvalidHWM =
      'A queueing strategy\'s highWaterMark property must be a nonnegative, non-NaN number';
  const errTmplMustBeFunctionOrUndefined = name =>
      `${name} must be a function or undefined`;

  class ReadableStream {
    constructor() {
      // TODO(domenic): when V8 gets default parameters and destructuring, all
      // this can be cleaned up.
      const underlyingSource = arguments[0] === undefined ? {} : arguments[0];
      const strategy = arguments[1] === undefined ? {} : arguments[1];
      const size = strategy.size;
      let highWaterMark = strategy.highWaterMark;
      if (highWaterMark === undefined) {
        highWaterMark = 1;
      }

      const normalizedStrategy =
          ValidateAndNormalizeQueuingStrategy(size, highWaterMark);

      this[readableStreamUnderlyingSource] = underlyingSource;

      this[readableStreamQueue] = new v8.InternalPackedArray();
      this[readableStreamQueueSize] = 0;

      this[readableStreamState] = STATE_READABLE;
      this[readableStreamBits] = 0b0;
      this[readableStreamReader] = undefined;
      this[readableStreamStoredError] = undefined;

      this[readableStreamStrategySize] = normalizedStrategy.size;
      this[readableStreamStrategyHWM] = normalizedStrategy.highWaterMark;

      // Avoid allocating the controller if the stream is going to be controlled
      // externally (i.e. from C++) anyway. All calls to underlyingSource
      // methods will disregard their controller argument in such situations
      // (but see below).

      const isControlledExternally =
          arguments[2] === createWithExternalControllerSentinel;
      const controller =
          isControlledExternally ? null : new ReadableStreamController(this);
      this[readableStreamController] = controller;

      // We need to pass ourself to the underlyingSource start method for
      // externally-controlled streams. We use the now-useless controller
      // argument to do so.
      const argToStart = isControlledExternally ? this : controller;

      const startResult = CallOrNoop(
          underlyingSource, 'start', argToStart, 'underlyingSource.start');
      thenPromise(Promise_resolve(startResult),
          () => {
            this[readableStreamBits] |= STARTED;
            RequestReadableStreamPull(this);
          },
          r => {
            if (this[readableStreamState] === STATE_READABLE) {
              return ErrorReadableStream(this, r);
            }
          });
    }

    get locked() {
      if (IsReadableStream(this) === false) {
        throw new TypeError(errIllegalInvocation);
      }

      return IsReadableStreamLocked(this);
    }

    cancel(reason) {
      if (IsReadableStream(this) === false) {
        return Promise_reject(new TypeError(errIllegalInvocation));
      }

      if (IsReadableStreamLocked(this) === true) {
        return Promise_reject(new TypeError(errCancelLockedStream));
      }

      return CancelReadableStream(this, reason);
    }

    getReader() {
      if (IsReadableStream(this) === false) {
        throw new TypeError(errIllegalInvocation);
      }

      return AcquireReadableStreamReader(this);
    }

    tee() {
      if (IsReadableStream(this) === false) {
        throw new TypeError(errIllegalInvocation);
      }

      return TeeReadableStream(this);
    }
  }

  class ReadableStreamController {
    constructor(stream) {
      if (IsReadableStream(stream) === false) {
        throw new TypeError(errIllegalConstructor);
      }

      if (stream[readableStreamController] !== undefined) {
        throw new TypeError(errIllegalConstructor);
      }

      this[readableStreamControllerControlledReadableStream] = stream;
    }

    get desiredSize() {
      if (IsReadableStreamController(this) === false) {
        throw new TypeError(errIllegalInvocation);
      }

      return GetReadableStreamDesiredSize(
          this[readableStreamControllerControlledReadableStream]);
    }

    close() {
      if (IsReadableStreamController(this) === false) {
        throw new TypeError(errIllegalInvocation);
      }

      const stream = this[readableStreamControllerControlledReadableStream];

      if (stream[readableStreamBits] & CLOSE_REQUESTED) {
        throw new TypeError(errCloseCloseRequestedStream);
      }
      if (stream[readableStreamState] === STATE_ERRORED) {
        throw new TypeError(errCloseErroredStream);
      }

      return CloseReadableStream(stream);
    }

    enqueue(chunk) {
      if (IsReadableStreamController(this) === false) {
        throw new TypeError(errIllegalInvocation);
      }

      const stream = this[readableStreamControllerControlledReadableStream];

      if (stream[readableStreamState] === STATE_ERRORED) {
        throw stream[readableStreamStoredError];
      }

      if (stream[readableStreamBits] & CLOSE_REQUESTED) {
        throw new TypeError(errEnqueueInCloseRequestedStream);
      }

      return EnqueueInReadableStream(stream, chunk);
    }

    error(e) {
      if (IsReadableStreamController(this) === false) {
        throw new TypeError(errIllegalInvocation);
      }

      const stream = this[readableStreamControllerControlledReadableStream];

      const state = stream[readableStreamState];
      if (state !== STATE_READABLE) {
        if (state === STATE_ERRORED) {
          throw new TypeError(errErrorErroredStream);
        }
        if (state === STATE_CLOSED) {
          throw new TypeError(errErrorClosedStream);
        }
      }

      return ErrorReadableStream(stream, e);
    }
  }

  class ReadableStreamReader {
    constructor(stream) {
      if (IsReadableStream(stream) === false) {
        throw new TypeError(errReaderConstructorBadArgument);
      }
      if (IsReadableStreamLocked(stream) === true) {
        throw new TypeError(errReaderConstructorStreamAlreadyLocked);
      }

      // TODO(yhirano): Remove this when we don't need hasPendingActivity in
      // blink::UnderlyingSourceBase.
      if (stream[readableStreamController] === null) {
        // The stream is created with an external controller (i.e. made in
        // Blink).
        const underlyingSource = stream[readableStreamUnderlyingSource];
        callFunction(underlyingSource.notifyLockAcquired, underlyingSource);
      }

      this[readableStreamReaderOwnerReadableStream] = stream;
      stream[readableStreamReader] = this;

      this[readableStreamReaderReadRequests] = new v8.InternalPackedArray();

      switch (stream[readableStreamState]) {
        case STATE_READABLE:
          this[readableStreamReaderClosedPromise] = v8.createPromise();
          break;
        case STATE_CLOSED:
          this[readableStreamReaderClosedPromise] = Promise_resolve(undefined);
          break;
        case STATE_ERRORED:
          this[readableStreamReaderClosedPromise] =
              Promise_reject(stream[readableStreamStoredError]);
          break;
      }
    }

    get closed() {
      if (IsReadableStreamReader(this) === false) {
        return Promise_reject(new TypeError(errIllegalInvocation));
      }

      return this[readableStreamReaderClosedPromise];
    }

    cancel(reason) {
      if (IsReadableStreamReader(this) === false) {
        return Promise_reject(new TypeError(errIllegalInvocation));
      }

      const stream = this[readableStreamReaderOwnerReadableStream];
      if (stream === undefined) {
        return Promise_reject(new TypeError(errCancelReleasedReader));
      }

      return CancelReadableStream(stream, reason);
    }

    read() {
      if (IsReadableStreamReader(this) === false) {
        return Promise_reject(new TypeError(errIllegalInvocation));
      }

      if (this[readableStreamReaderOwnerReadableStream] === undefined) {
        return Promise_reject(new TypeError(errReadReleasedReader));
      }

      return ReadFromReadableStreamReader(this);
    }

    releaseLock() {
      if (IsReadableStreamReader(this) === false) {
        throw new TypeError(errIllegalInvocation);
      }

      const stream = this[readableStreamReaderOwnerReadableStream];
      if (stream === undefined) {
        return undefined;
      }

      if (this[readableStreamReaderReadRequests].length > 0) {
        throw new TypeError(errReleaseReaderWithPendingRead);
      }

      // TODO(yhirano): Remove this when we don't need hasPendingActivity in
      // blink::UnderlyingSourceBase.
      if (stream[readableStreamController] === null) {
        // The stream is created with an external controller (i.e. made in
        // Blink).
        const underlyingSource = stream[readableStreamUnderlyingSource];
        callFunction(underlyingSource.notifyLockReleased, underlyingSource);
      }

      if (stream[readableStreamState] === STATE_READABLE) {
        v8.rejectPromise(this[readableStreamReaderClosedPromise],
            new TypeError(errReleasedReaderClosedPromise));
      } else {
        this[readableStreamReaderClosedPromise] =
            Promise_reject(new TypeError(errReleasedReaderClosedPromise));
      }

      this[readableStreamReaderOwnerReadableStream][readableStreamReader] =
          undefined;
      this[readableStreamReaderOwnerReadableStream] = undefined;
    }
  }

  //
  // Readable stream abstract operations
  //

  function AcquireReadableStreamReader(stream) {
    return new ReadableStreamReader(stream);
  }

  function CancelReadableStream(stream, reason) {
    stream[readableStreamBits] |= DISTURBED;

    const state = stream[readableStreamState];
    if (state === STATE_CLOSED) {
      return Promise_resolve(undefined);
    }
    if (state === STATE_ERRORED) {
      return Promise_reject(stream[readableStreamStoredError]);
    }

    stream[readableStreamQueue] = new v8.InternalPackedArray();
    FinishClosingReadableStream(stream);

    const underlyingSource = stream[readableStreamUnderlyingSource];
    const sourceCancelPromise = PromiseCallOrNoop(
        underlyingSource, 'cancel', reason, 'underlyingSource.cancel');
    return thenPromise(sourceCancelPromise, () => undefined);
  }

  function CloseReadableStream(stream) {
    if (stream[readableStreamState] === STATE_CLOSED) {
      return undefined;
    }

    stream[readableStreamBits] |= CLOSE_REQUESTED;

    if (stream[readableStreamQueue].length === 0) {
      return FinishClosingReadableStream(stream);
    }
  }

  function EnqueueInReadableStream(stream, chunk) {
    if (stream[readableStreamState] === STATE_CLOSED) {
      return undefined;
    }

    if (IsReadableStreamLocked(stream) === true &&
        stream[readableStreamReader][readableStreamReaderReadRequests].length >
            0) {
      const readRequest =
          stream[readableStreamReader][readableStreamReaderReadRequests]
              .shift();
      v8.resolvePromise(readRequest, CreateIterResultObject(chunk, false));
    } else {
      let chunkSize = 1;

      const strategySize = stream[readableStreamStrategySize];
      if (strategySize !== undefined) {
        try {
          chunkSize = strategySize(chunk);
        } catch (chunkSizeE) {
          if (stream[readableStreamState] === STATE_READABLE) {
            ErrorReadableStream(stream, chunkSizeE);
          }
          throw chunkSizeE;
        }
      }

      try {
        EnqueueValueWithSize(stream, chunk, chunkSize);
      } catch (enqueueE) {
        if (stream[readableStreamState] === STATE_READABLE) {
          ErrorReadableStream(stream, enqueueE);
        }
        throw enqueueE;
      }
    }

    RequestReadableStreamPull(stream);
  }

  function ErrorReadableStream(stream, e) {
    stream[readableStreamQueue] = new v8.InternalPackedArray();
    stream[readableStreamStoredError] = e;
    stream[readableStreamState] = STATE_ERRORED;

    const reader = stream[readableStreamReader];
    if (reader === undefined) {
      return undefined;
    }

    const readRequests = reader[readableStreamReaderReadRequests];
    for (let i = 0; i < readRequests.length; ++i) {
      v8.rejectPromise(readRequests[i], e);
    }
    reader[readableStreamReaderReadRequests] = new v8.InternalPackedArray();

    v8.rejectPromise(reader[readableStreamReaderClosedPromise], e);
  }

  function FinishClosingReadableStream(stream) {
    stream[readableStreamState] = STATE_CLOSED;

    const reader = stream[readableStreamReader];
    if (reader === undefined) {
      return undefined;
    }


    const readRequests = reader[readableStreamReaderReadRequests];
    for (let i = 0; i < readRequests.length; ++i) {
      v8.resolvePromise(
          readRequests[i], CreateIterResultObject(undefined, true));
    }
    reader[readableStreamReaderReadRequests] = new v8.InternalPackedArray();

    v8.resolvePromise(reader[readableStreamReaderClosedPromise], undefined);
  }

  function GetReadableStreamDesiredSize(stream) {
    const queueSize = GetTotalQueueSize(stream);
    return stream[readableStreamStrategyHWM] - queueSize;
  }

  function IsReadableStream(x) {
    return hasOwnProperty(x, readableStreamUnderlyingSource);
  }

  function IsReadableStreamDisturbed(stream) {
    return stream[readableStreamBits] & DISTURBED;
  }

  function IsReadableStreamLocked(stream) {
    return stream[readableStreamReader] !== undefined;
  }

  function IsReadableStreamController(x) {
    return hasOwnProperty(x, readableStreamControllerControlledReadableStream);
  }

  function IsReadableStreamReadable(stream) {
    return stream[readableStreamState] === STATE_READABLE;
  }

  function IsReadableStreamClosed(stream) {
    return stream[readableStreamState] === STATE_CLOSED;
  }

  function IsReadableStreamErrored(stream) {
    return stream[readableStreamState] === STATE_ERRORED;
  }

  function IsReadableStreamReader(x) {
    return hasOwnProperty(x, readableStreamReaderOwnerReadableStream);
  }

  function ReadFromReadableStreamReader(reader) {
    const stream = reader[readableStreamReaderOwnerReadableStream];
    stream[readableStreamBits] |= DISTURBED;

    if (stream[readableStreamState] === STATE_CLOSED) {
      return Promise_resolve(CreateIterResultObject(undefined, true));
    }

    if (stream[readableStreamState] === STATE_ERRORED) {
      return Promise_reject(stream[readableStreamStoredError]);
    }

    const queue = stream[readableStreamQueue];
    if (queue.length > 0) {
      const chunk = DequeueValue(stream);

      if (stream[readableStreamBits] & CLOSE_REQUESTED && queue.length === 0) {
        FinishClosingReadableStream(stream);
      } else {
        RequestReadableStreamPull(stream);
      }

      return Promise_resolve(CreateIterResultObject(chunk, false));
    } else {
      const readRequest = v8.createPromise();

      reader[readableStreamReaderReadRequests].push(readRequest);
      RequestReadableStreamPull(stream);
      return readRequest;
    }
  }

  function RequestReadableStreamPull(stream) {
    const shouldPull = ShouldReadableStreamPull(stream);
    if (shouldPull === false) {
      return undefined;
    }

    if (stream[readableStreamBits] & PULLING) {
      stream[readableStreamBits] |= PULL_AGAIN;
      return undefined;
    }

    stream[readableStreamBits] |= PULLING;

    const underlyingSource = stream[readableStreamUnderlyingSource];
    const controller = stream[readableStreamController];
    const pullPromise = PromiseCallOrNoop(
        underlyingSource, 'pull', controller, 'underlyingSource.pull');

    thenPromise(pullPromise,
        () => {
          stream[readableStreamBits] &= ~PULLING;

          if (stream[readableStreamBits] & PULL_AGAIN) {
            stream[readableStreamBits] &= ~PULL_AGAIN;
            return RequestReadableStreamPull(stream);
          }
        },
        e => {
          if (stream[readableStreamState] === STATE_READABLE) {
            return ErrorReadableStream(stream, e);
          }
        });
  }

  function ShouldReadableStreamPull(stream) {
    const state = stream[readableStreamState];
    if (state === STATE_CLOSED || state === STATE_ERRORED) {
      return false;
    }

    if (stream[readableStreamBits] & CLOSE_REQUESTED) {
      return false;
    }

    if (!(stream[readableStreamBits] & STARTED)) {
      return false;
    }

    if (IsReadableStreamLocked(stream) === true) {
      const reader = stream[readableStreamReader];
      const readRequests = reader[readableStreamReaderReadRequests];
      if (readRequests.length > 0) {
        return true;
      }
    }

    const desiredSize = GetReadableStreamDesiredSize(stream);
    if (desiredSize > 0) {
      return true;
    }

    return false;
  }

  // Potential future optimization: use class instances for the underlying
  // sources, so that we don't re-create
  // closures every time.

  // TODO(domenic): shouldClone argument from spec not supported yet
  function TeeReadableStream(stream) {
    const reader = AcquireReadableStreamReader(stream);

    let closedOrErrored = false;
    let canceled1 = false;
    let canceled2 = false;
    let reason1;
    let reason2;
    let promise = v8.createPromise();

    const branch1 = new ReadableStream({pull, cancel: cancel1});

    const branch2 = new ReadableStream({pull, cancel: cancel2});

    thenPromise(
        reader[readableStreamReaderClosedPromise], undefined, function(r) {
          if (closedOrErrored === true) {
            return;
          }

          ErrorReadableStream(branch1, r);
          ErrorReadableStream(branch2, r);
          closedOrErrored = true;
        });

    return [branch1, branch2];


    function pull() {
      return thenPromise(
          ReadFromReadableStreamReader(reader), function(result) {
            const value = result.value;
            const done = result.done;

            if (done === true && closedOrErrored === false) {
              CloseReadableStream(branch1);
              CloseReadableStream(branch2);
              closedOrErrored = true;
            }

            if (closedOrErrored === true) {
              return;
            }

            if (canceled1 === false) {
              EnqueueInReadableStream(branch1, value);
            }

            if (canceled2 === false) {
              EnqueueInReadableStream(branch2, value);
            }
          });
    }

    function cancel1(reason) {
      canceled1 = true;
      reason1 = reason;

      if (canceled2 === true) {
        const compositeReason = [reason1, reason2];
        const cancelResult = CancelReadableStream(stream, compositeReason);
        v8.resolvePromise(promise, cancelResult);
      }

      return promise;
    }

    function cancel2(reason) {
      canceled2 = true;
      reason2 = reason;

      if (canceled1 === true) {
        const compositeReason = [reason1, reason2];
        const cancelResult = CancelReadableStream(stream, compositeReason);
        v8.resolvePromise(promise, cancelResult);
      }

      return promise;
    }
  }

  //
  // Queue-with-sizes
  // Modified from taking the queue (as in the spec) to taking the stream, so we
  // can modify the queue size alongside.
  //

  function DequeueValue(stream) {
    const result = stream[readableStreamQueue].shift();
    stream[readableStreamQueueSize] -= result.size;
    return result.value;
  }

  function EnqueueValueWithSize(stream, value, size) {
    size = Number(size);
    if (Number_isNaN(size) || size === +Infinity || size < 0) {
      throw new RangeError(errInvalidSize);
    }

    stream[readableStreamQueueSize] += size;
    stream[readableStreamQueue].push({value, size});
  }

  function GetTotalQueueSize(stream) { return stream[readableStreamQueueSize]; }

  //
  // Other helpers
  //

  function ValidateAndNormalizeQueuingStrategy(size, highWaterMark) {
    if (size !== undefined && typeof size !== 'function') {
      throw new TypeError(errSizeNotAFunction);
    }

    highWaterMark = Number(highWaterMark);
    if (Number_isNaN(highWaterMark)) {
      throw new TypeError(errInvalidHWM);
    }
    if (highWaterMark < 0) {
      throw new RangeError(errInvalidHWM);
    }

    return {size, highWaterMark};
  }

  // Modified from InvokeOrNoop in spec
  function CallOrNoop(O, P, arg, nameForError) {
    const method = O[P];
    if (method === undefined) {
      return undefined;
    }
    if (typeof method !== 'function') {
      throw new TypeError(errTmplMustBeFunctionOrUndefined(nameForError));
    }

    return callFunction(method, O, arg);
  }


  // Modified from PromiseInvokeOrNoop in spec
  function PromiseCallOrNoop(O, P, arg, nameForError) {
    let method;
    try {
      method = O[P];
    } catch (methodE) {
      return Promise_reject(methodE);
    }

    if (method === undefined) {
      return Promise_resolve(undefined);
    }

    if (typeof method !== 'function') {
      return Promise_reject(errTmplMustBeFunctionOrUndefined(nameForError));
    }

    try {
      return Promise_resolve(callFunction(method, O, arg));
    } catch (e) {
      return Promise_reject(e);
    }
  }

  function CreateIterResultObject(value, done) { return {value, done}; }


  //
  // Additions to the global
  //

  defineProperty(global, 'ReadableStream', {
    value: ReadableStream,
    enumerable: false,
    configurable: true,
    writable: true
  });

  //
  // Exports to Blink
  //

  binding.AcquireReadableStreamReader = AcquireReadableStreamReader;
  binding.IsReadableStream = IsReadableStream;
  binding.IsReadableStreamDisturbed = IsReadableStreamDisturbed;
  binding.IsReadableStreamLocked = IsReadableStreamLocked;
  binding.IsReadableStreamReadable = IsReadableStreamReadable;
  binding.IsReadableStreamClosed = IsReadableStreamClosed;
  binding.IsReadableStreamErrored = IsReadableStreamErrored;
  binding.IsReadableStreamReader = IsReadableStreamReader;
  binding.ReadFromReadableStreamReader = ReadFromReadableStreamReader;

  binding.CloseReadableStream = CloseReadableStream;
  binding.GetReadableStreamDesiredSize = GetReadableStreamDesiredSize;
  binding.EnqueueInReadableStream = EnqueueInReadableStream;
  binding.ErrorReadableStream = ErrorReadableStream;

  binding.createReadableStreamWithExternalController =
      (underlyingSource, strategy) => {
        return new ReadableStream(
            underlyingSource, strategy, createWithExternalControllerSentinel);
      };
});
