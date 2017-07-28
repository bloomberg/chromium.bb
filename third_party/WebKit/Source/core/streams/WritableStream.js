// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of WritableStream for Blink.  See
// https://streams.spec.whatwg.org/#ws. The implementation closely follows the
// standard, except where required for performance or integration with Blink. In
// particular, classes, methods and abstract operations are implemented in the
// same order as in the standard, to simplify side-by-side reading.

(function(global, binding, v8) {
  'use strict';

  // Private symbols. These correspond to the internal slots in the standard.
  // "[[X]]" in the standard is spelt _X in this implementation.

  // TODO(ricea): Optimise [[closeRequest]] and [[inFlightCloseRequest]] into a
  // single slot + a flag to say which one is set at the moment.
  const _closeRequest = v8.createPrivateSymbol('[[closeRequest]]');
  const _inFlightWriteRequest = v8.createPrivateSymbol('[[inFlightWriteRequest]]');
  const _inFlightCloseRequest = v8.createPrivateSymbol('[[inFlightCloseRequest]]');
  const _pendingAbortRequest =
      v8.createPrivateSymbol('[[pendingAbortRequest]]');
  // Flags and state are combined into a single integer field for efficiency.
  const _stateAndFlags = v8.createPrivateSymbol('[[state]] and flags');
  const _storedError = v8.createPrivateSymbol('[[storedError]]');
  const _writableStreamController =
      v8.createPrivateSymbol('[[writableStreamController]]');
  const _writer = v8.createPrivateSymbol('[[writer]]');
  const _writeRequests = v8.createPrivateSymbol('[[writeRequests]]');
  const _closedPromise = v8.createPrivateSymbol('[[closedPromise]]');
  const _ownerWritableStream =
      v8.createPrivateSymbol('[[ownerWritableStream]]');
  const _readyPromise = v8.createPrivateSymbol('[[readyPromise]]');
  const _controlledWritableStream =
      v8.createPrivateSymbol('[[controlledWritableStream]]');
  const _queue = v8.createPrivateSymbol('[[queue]]');
  const _queueTotalSize = v8.createPrivateSymbol('[[queueTotalSize]]');
  const _started = v8.createPrivateSymbol('[[started]]');
  const _strategyHWM = v8.createPrivateSymbol('[[strategyHWM]]');
  const _strategySize = v8.createPrivateSymbol('[[strategySize]]');
  const _underlyingSink = v8.createPrivateSymbol('[[underlyingSink]]');

  // Numeric encodings of stream states. Stored in the _stateAndFlags slot.
  const WRITABLE = 0;
  const CLOSED = 1;
  const ERRORING = 2;
  const ERRORED = 3;

  // Mask to extract or assign states to _stateAndFlags.
  const STATE_MASK = 0xF;

  // Also stored in _stateAndFlags.
  const BACKPRESSURE_FLAG = 0x10;

  // Javascript functions. It is important to use these copies, as the ones on
  // the global object may have been overwritten. See "V8 Extras Design Doc",
  // section "Security Considerations".
  // https://docs.google.com/document/d/1AT5-T0aHGp7Lt29vPWFr2-qG8r3l9CByyvKwEuA8Ec0/edit#heading=h.9yixony1a18r
  const undefined = global.undefined;

  const defineProperty = global.Object.defineProperty;
  const hasOwnProperty = v8.uncurryThis(global.Object.hasOwnProperty);

  const Function_apply = v8.uncurryThis(global.Function.prototype.apply);
  const Function_call = v8.uncurryThis(global.Function.prototype.call);

  const TypeError = global.TypeError;
  const RangeError = global.RangeError;

  const Boolean = global.Boolean;
  const Number = global.Number;
  const Number_isNaN = Number.isNaN;
  const Number_isFinite = Number.isFinite;

  const Promise = global.Promise;
  const thenPromise = v8.uncurryThis(Promise.prototype.then);
  const Promise_resolve = v8.simpleBind(Promise.resolve, Promise);
  const Promise_reject = v8.simpleBind(Promise.reject, Promise);

  // From CommonOperations.js
  const { hasOwnPropertyNoThrow } = binding.streamOperations;

  // User-visible strings.
  const streamErrors = binding.streamErrors;
  const errAbortLockedStream = 'Cannot abort a writable stream that is locked to a writer';
  const errStreamAborted = 'The stream has been aborted';
  const errStreamAborting = 'The stream is in the process of being aborted';
  const errWriterLockReleasedPrefix = 'This writable stream writer has been released and cannot be ';
  const errCloseCloseRequestedStream =
      'Cannot close a writable stream that has already been requested to be closed';
  const errWriteCloseRequestedStream =
      'Cannot write to a writable stream that is due to be closed';
  const templateErrorCannotActionOnStateStream =
      (action, state) => `Cannot ${action} a ${state} writable stream`;
  const errReleasedWriterClosedPromise =
      'This writable stream writer has been released and cannot be used to monitor the stream\'s state';
  const templateErrorIsNotAFunction = f => `${f} is not a function`;

  // These verbs are used after errWriterLockReleasedPrefix
  const verbUsedToGetTheDesiredSize = 'used to get the desiredSize';
  const verbAborted = 'aborted';
  const verbClosed = 'closed';
  const verbWrittenTo = 'written to';

  // Utility functions (not from the standard).
  function createWriterLockReleasedError(verb) {
    return new TypeError(errWriterLockReleasedPrefix + verb);
  }

  const stateNames = {[CLOSED]: 'closed', [ERRORED]: 'errored'};
  function createCannotActionOnStateStreamError(action, state) {
    // assert(stateNames[state] !== undefined,
    //        `name for state ${state} exists in stateNames`);
    return new TypeError(
        templateErrorCannotActionOnStateStream(action, stateNames[state]));
  }

  // TODO(ricea): Share these with ReadableStream.
  function internalError() {
    throw new RangeError('WritableStream Internal Error');
  }

  function rejectPromise(p, reason) {
    if (!v8.isPromise(p)) {
      internalError();
    }
    v8.rejectPromise(p, reason);
  }

  function resolvePromise(p, value) {
    if (!v8.isPromise(p)) {
      internalError();
    }
    v8.resolvePromise(p, value);
  }

  function markPromiseAsHandled(p) {
    if (!v8.isPromise(p)) {
      internalError();
    }
    v8.markPromiseAsHandled(p);
  }

  function promiseState(p) {
    if (!v8.isPromise(p)) {
      internalError();
    }
    return v8.promiseState(p);
  }

  function rejectPromises(queue, e) {
    queue.forEach(promise => rejectPromise(promise, e));
  }

  class WritableStream {
    constructor(underlyingSink = {}, { size, highWaterMark = 1 } = {}) {
      this[_stateAndFlags] = WRITABLE;
      this[_storedError] = undefined;
      this[_writer] = undefined;
      this[_writableStreamController] = undefined;
      this[_inFlightWriteRequest] = undefined;
      this[_closeRequest] = undefined;
      this[_inFlightCloseRequest] = undefined;
      this[_pendingAbortRequest] = undefined;
      this[_writeRequests] = new binding.SimpleQueue();
      const type = underlyingSink.type;
      if (type !== undefined) {
        throw new RangeError(streamErrors.invalidType);
      }
      this[_writableStreamController] =
          new WritableStreamDefaultController(this, underlyingSink, size,
                                              highWaterMark);
      WritableStreamDefaultControllerStartSteps(this[_writableStreamController]);
    }

    get locked() {
      if (!IsWritableStream(this)) {
        throw new TypeError(streamErrors.illegalInvocation);
      }
      return IsWritableStreamLocked(this);
    }

    abort(reason) {
      if (!IsWritableStream(this)) {
        return Promise_reject(new TypeError(streamErrors.illegalInvocation));
      }
      if (IsWritableStreamLocked(this)) {
        return Promise_reject(new TypeError(errAbortLockedStream));
      }
      return WritableStreamAbort(this, reason);
    }

    getWriter() {
      if (!IsWritableStream(this)) {
         throw new TypeError(streamErrors.illegalInvocation);
      }
      return AcquireWritableStreamDefaultWriter(this);
    }
  }

  // General Writable Stream Abstract Operations

  function AcquireWritableStreamDefaultWriter(stream) {
    return new WritableStreamDefaultWriter(stream);
  }

  function IsWritableStream(x) {
    return hasOwnPropertyNoThrow(x, _writableStreamController);
  }

  function IsWritableStreamLocked(stream) {
    // assert(IsWritableStream(stream),
    //        '! IsWritableStream(stream) is true.');
    return stream[_writer] !== undefined;
  }

  function WritableStreamAbort(stream, reason) {
    const state = stream[_stateAndFlags] & STATE_MASK;
    if (state === CLOSED) {
      return Promise_resolve(undefined);
    }
    if (state === ERRORED) {
      return Promise_reject(stream[_storedError]);
    }
    const error = new TypeError(errStreamAborting);
    if (stream[_pendingAbortRequest] !== undefined) {
      return Promise_reject(error);
    }

    // assert(state === WRITABLE || state === ERRORING,
    //        '_state_ is `"writable"` or `"erroring"`');

    const wasAlreadyErroring = state === ERRORING;
    if (wasAlreadyErroring) {
      reason = undefined;
    }

    const promise = v8.createPromise();
    stream[_pendingAbortRequest] = {promise, reason, wasAlreadyErroring};

    if (!wasAlreadyErroring) {
      WritableStreamStartErroring(stream, error);
    }
    return promise;
  }

  // Writable Stream Abstract Operations Used by Controllers

  function WritableStreamAddWriteRequest(stream) {
    // assert(IsWritableStreamLocked(stream),
    //        '! IsWritableStreamLocked(writer) is true.');
    // assert((stream[_stateAndFlags] & STATE_MASK) === WRITABLE,
    //        'stream.[[state]] is "writable".');
    const promise = v8.createPromise();
    stream[_writeRequests].push(promise);
    return promise;
  }

  function WritableStreamDealWithRejection(stream, error) {
    const state = stream[_stateAndFlags] & STATE_MASK;
    if (state === WRITABLE) {
      WritableStreamStartErroring(stream, error);
      return;
    }

    // assert(state === ERRORING, '_state_ is `"erroring"`');
    WritableStreamFinishErroring(stream);
  }

  function WritableStreamStartErroring(stream, reason) {
    // assert(stream[_storedError] === undefined,
    //        '_stream_.[[storedError]] is *undefined*');
    // assert((stream[_stateAndFlags] & STATE_MASK) === WRITABLE,
    //        '_stream_.[[state]] is `"writable"`');

    const controller = stream[_writableStreamController];
    // assert(controller !== undefined, '_controller_ is not *undefined*');

    stream[_stateAndFlags] = (stream[_stateAndFlags] & ~STATE_MASK) | ERRORING;
    stream[_storedError] = reason;

    const writer = stream[_writer];
    if (writer !== undefined) {
      WritableStreamDefaultWriterEnsureReadyPromiseRejected(writer, reason);
    }

    if (!WritableStreamHasOperationMarkedInFlight(stream) &&
        controller[_started]) {
      WritableStreamFinishErroring(stream);
    }
  }

  function WritableStreamFinishErroring(stream) {
    // assert((stream[_stateAndFlags] & STATE_MASK) === ERRORING,
    //        '_stream_.[[state]] is `"erroring"`');
    // assert(!WritableStreamHasOperationMarkedInFlight(stream),
    //        '! WritableStreamHasOperationMarkedInFlight(_stream_) is *false*');

    stream[_stateAndFlags] = (stream[_stateAndFlags] & ~STATE_MASK) | ERRORED;

    WritableStreamDefaultControllerErrorSteps(
        stream[_writableStreamController]);

    const storedError = stream[_storedError];
    rejectPromises(stream[_writeRequests], storedError);
    stream[_writeRequests] = new binding.SimpleQueue();

    if (stream[_pendingAbortRequest] === undefined) {
      WritableStreamRejectCloseAndClosedPromiseIfNeeded(stream);
      return;
    }

    const abortRequest = stream[_pendingAbortRequest];
    stream[_pendingAbortRequest] = undefined;

    if (abortRequest.wasAlreadyErroring === true) {
      rejectPromise(abortRequest.promise, storedError);
      WritableStreamRejectCloseAndClosedPromiseIfNeeded(stream);
      return;
    }

    const promise = WritableStreamDefaultControllerAbortSteps(
        stream[_writableStreamController], abortRequest.reason);

    thenPromise(
        promise,
        () => {
          resolvePromise(abortRequest.promise, undefined);
          WritableStreamRejectCloseAndClosedPromiseIfNeeded(stream);
        },
        reason => {
          rejectPromise(abortRequest.promise, reason);
          WritableStreamRejectCloseAndClosedPromiseIfNeeded(stream);
        });
  }

  function WritableStreamFinishInFlightWrite(stream) {
    // assert(stream[_inFlightWriteRequest] !== undefined,
    //        '_stream_.[[inFlightWriteRequest]] is not *undefined*.');
    resolvePromise(stream[_inFlightWriteRequest], undefined);
    stream[_inFlightWriteRequest] = undefined;
  }

  function WritableStreamFinishInFlightWriteWithError(stream, error) {
    // assert(stream[_inFlightWriteRequest] !== undefined,
    //        '_stream_.[[inFlightWriteRequest]] is not *undefined*.');
    rejectPromise(stream[_inFlightWriteRequest], error);
    stream[_inFlightWriteRequest] = undefined;

    let state = stream[_stateAndFlags] & STATE_MASK;
    // assert(state === WRITABLE || state === ERRORING,
    //        '_stream_.[[state]] is `"writable"` or `"erroring"`');

    WritableStreamDealWithRejection(stream, error);
  }

  function WritableStreamFinishInFlightClose(stream) {
    // assert(stream[_inFlightCloseRequest] !== undefined,
    //        '_stream_.[[inFlightCloseRequest]] is not *undefined*.');
    resolvePromise(stream[_inFlightCloseRequest], undefined);
    stream[_inFlightCloseRequest] = undefined;

    const state = stream[_stateAndFlags] & STATE_MASK;
    // assert(state === WRITABLE || state === ERRORING,
    //        '_stream_.[[state]] is `"writable"` or `"erroring"`');

    if (state === ERRORING) {
      stream[_storedError] = undefined;
      if (stream[_pendingAbortRequest] !== undefined) {
        resolvePromise(stream[_pendingAbortRequest].promise, undefined);
        stream[_pendingAbortRequest] = undefined;
      }
    }

    stream[_stateAndFlags] = (stream[_stateAndFlags] & ~STATE_MASK) | CLOSED;
    const writer = stream[_writer];
    if (writer !== undefined) {
      resolvePromise(writer[_closedPromise], undefined);
    }

    // assert(stream[_pendingAbortRequest] === undefined,
    //        '_stream_.[[pendingAbortRequest]] is *undefined*');
    // assert(stream[_storedError] === undefined,
    //        '_stream_.[[storedError]] is *undefined*');
  }

  function WritableStreamFinishInFlightCloseWithError(stream, error) {
    // assert(stream[_inFlightCloseRequest] !== undefined,
    //        '_stream_.[[inFlightCloseRequest]] is not *undefined*.');
    rejectPromise(stream[_inFlightCloseRequest], error);
    stream[_inFlightCloseRequest] = undefined;

    const state = stream[_stateAndFlags] & STATE_MASK;
    // assert(state === WRITABLE || state === ERRORING,
    //        '_stream_.[[state]] is `"writable"` or `"erroring"`');

    if (stream[_pendingAbortRequest] !== undefined) {
      rejectPromise(stream[_pendingAbortRequest].promise, error);
      stream[_pendingAbortRequest] = undefined;
    }

    WritableStreamDealWithRejection(stream, error);
  }

  function WritableStreamCloseQueuedOrInFlight(stream) {
    return stream[_closeRequest] !== undefined ||
        stream[_inFlightCloseRequest] !== undefined;
  }

  function WritableStreamHasOperationMarkedInFlight(stream) {
    return stream[_inFlightWriteRequest] !== undefined ||
        stream[_inFlightCloseRequest] !== undefined;
  }

  function WritableStreamMarkCloseRequestInFlight(stream) {
    // assert(stream[_inFlightCloseRequest] === undefined,
    //        '_stream_.[[inFlightCloseRequest]] is *undefined*.');
    // assert(stream[_closeRequest] !== undefined,
    //        '_stream_.[[closeRequest]] is not *undefined*.');
    stream[_inFlightCloseRequest] = stream[_closeRequest];
    stream[_closeRequest] = undefined;
  }

  function WritableStreamMarkFirstWriteRequestInFlight(stream) {
    // assert(stream[_inFlightWriteRequest] === undefined,
    //        '_stream_.[[inFlightWriteRequest]] is *undefined*.');
    // assert(stream[_writeRequests].length !== 0,
    //        '_stream_.[[writeRequests]] is not empty.');
    const writeRequest = stream[_writeRequests].shift();
    stream[_inFlightWriteRequest] = writeRequest;
  }

  function WritableStreamRejectCloseAndClosedPromiseIfNeeded(stream) {
    // assert((stream[_stateAndFlags] & STATE_MASK) === ERRORED,
    //        '_stream_.[[state]] is `"errored"`');

    if (stream[_closeRequest] !== undefined) {
      // assert(stream[_inFlightCloseRequest] === undefined,
      //        '_stream_.[[inFlightCloseRequest]] is *undefined*');
      rejectPromise(stream[_closeRequest], stream[_storedError]);
      stream[_closeRequest] = undefined;
    }

    const writer = stream[_writer];
    if (writer !== undefined) {
      rejectPromise(writer[_closedPromise], stream[_storedError]);
      markPromiseAsHandled(writer[_closedPromise]);
    }
  }

  function WritableStreamUpdateBackpressure(stream, backpressure) {
    // assert((stream[_stateAndFlags] & STATE_MASK) === WRITABLE,
    //        'stream.[[state]] is "writable".');
    // assert(!WritableStreamCloseQueuedOrInFlight(stream),
    //        'WritableStreamCloseQueuedOrInFlight(_stream_) is *false*.');
    const writer = stream[_writer];
    if (writer !== undefined &&
        backpressure !== Boolean(stream[_stateAndFlags] & BACKPRESSURE_FLAG)) {
      if (backpressure) {
        writer[_readyPromise] = v8.createPromise();
      } else {
        // assert(!backpressure, '_backpressure_ is *false*.');
        resolvePromise(writer[_readyPromise], undefined);
      }
    }
    if (backpressure) {
      stream[_stateAndFlags] |= BACKPRESSURE_FLAG;
    } else {
      stream[_stateAndFlags] &= ~BACKPRESSURE_FLAG;
    }
  }

  // Functions to expose internals for ReadableStream.pipeTo. These are not
  // part of the standard.
  function isWritableStreamErrored(stream) {
    // assert(
    //     IsWritableStream(stream), '! IsWritableStream(stream) is true.');
    return (stream[_stateAndFlags] & STATE_MASK) === ERRORED;
  }

  function isWritableStreamClosingOrClosed(stream) {
    // assert(
    //     IsWritableStream(stream), '! IsWritableStream(stream) is true.');
    return WritableStreamCloseQueuedOrInFlight(stream) ||
        (stream[_stateAndFlags] & STATE_MASK) === CLOSED;
  }

  function getWritableStreamStoredError(stream) {
    // assert(
    //     IsWritableStream(stream), '! IsWritableStream(stream) is true.');
    return stream[_storedError];
  }

  class WritableStreamDefaultWriter {
    constructor(stream) {
      if (!IsWritableStream(stream)) {
        throw new TypeError(streamErrors.illegalConstructor);
      }
      if (IsWritableStreamLocked(stream)) {
        throw new TypeError(streamErrors.illegalConstructor);
      }
      this[_ownerWritableStream] = stream;
      stream[_writer] = this;
      const state = stream[_stateAndFlags] & STATE_MASK;
      switch (state) {
        case WRITABLE:
        {
          if (!WritableStreamCloseQueuedOrInFlight(stream) &&
              stream[_stateAndFlags] & BACKPRESSURE_FLAG) {
            this[_readyPromise] = v8.createPromise();
          } else {
            this[_readyPromise] = Promise_resolve(undefined);
          }
          this[_closedPromise] = v8.createPromise();
          break;
        }

        case ERRORING:
        {
          this[_readyPromise] = Promise_reject(stream[_storedError]);
          markPromiseAsHandled(this[_readyPromise]);
          this[_closedPromise] = v8.createPromise();
          break;
        }

        case CLOSED:
        {
          this[_readyPromise] = Promise_resolve(undefined);
          this[_closedPromise] = Promise_resolve(undefined);
          break;
        }

        default:
        {
          // assert(state === ERRORED, '_state_ is `"errored"`.');
          const storedError = stream[_storedError];
          this[_readyPromise] = Promise_reject(storedError);
          markPromiseAsHandled(this[_readyPromise]);
          this[_closedPromise] = Promise_reject(storedError);
          markPromiseAsHandled(this[_closedPromise]);
          break;
        }
      }
    }

    get closed() {
      if (!IsWritableStreamDefaultWriter(this)) {
        return Promise_reject(new TypeError(streamErrors.illegalInvocation));
      }
      return this[_closedPromise];
    }

    get desiredSize() {
      if (!IsWritableStreamDefaultWriter(this)) {
        throw new TypeError(streamErrors.illegalInvocation);
      }
      if (this[_ownerWritableStream] === undefined) {
        throw createWriterLockReleasedError(verbUsedToGetTheDesiredSize);
      }
      return WritableStreamDefaultWriterGetDesiredSize(this);
    }

    get ready() {
      if (!IsWritableStreamDefaultWriter(this)) {
        return Promise_reject(new TypeError(streamErrors.illegalInvocation));
      }
      return this[_readyPromise];
    }

    abort(reason) {
     if (!IsWritableStreamDefaultWriter(this)) {
        return Promise_reject(new TypeError(streamErrors.illegalInvocation));
      }
      if (this[_ownerWritableStream] === undefined) {
        return Promise_reject(createWriterLockReleasedError(verbAborted));
      }
      return WritableStreamDefaultWriterAbort(this, reason);
    }

    close() {
      if (!IsWritableStreamDefaultWriter(this)) {
        return Promise_reject(new TypeError(streamErrors.illegalInvocation));
      }
      const stream = this[_ownerWritableStream];
      if (stream === undefined) {
        return Promise_reject(createWriterLockReleasedError(verbClosed));
      }
      if (WritableStreamCloseQueuedOrInFlight(stream)) {
        return Promise_reject(new TypeError(errCloseCloseRequestedStream));
      }
      return WritableStreamDefaultWriterClose(this);
    }

    releaseLock() {
      if (!IsWritableStreamDefaultWriter(this)) {
        throw new TypeError(streamErrors.illegalInvocation);
      }
      const stream = this[_ownerWritableStream];
      if (stream === undefined) {
        return;
      }
      // assert(stream[_writer] !== undefined,
      //        'stream.[[writer]] is not undefined.');
      WritableStreamDefaultWriterRelease(this);
    }

    write(chunk) {
      if (!IsWritableStreamDefaultWriter(this)) {
        return Promise_reject(new TypeError(streamErrors.illegalInvocation));
      }
      if (this[_ownerWritableStream] === undefined) {
        return Promise_reject(createWriterLockReleasedError(verbWrittenTo));
      }
      return WritableStreamDefaultWriterWrite(this, chunk);
    }
  }

  // Writable Stream Writer Abstract Operations

  function IsWritableStreamDefaultWriter(x) {
    return hasOwnPropertyNoThrow(x, _ownerWritableStream);
  }

  function WritableStreamDefaultWriterAbort(writer, reason) {
    const stream = writer[_ownerWritableStream];
    // assert(stream !== undefined,
    //        'stream is not undefined.');
    return WritableStreamAbort(stream, reason);
  }

  function WritableStreamDefaultWriterClose(writer) {
    const stream = writer[_ownerWritableStream];
    // assert(stream !== undefined, 'stream is not undefined.');
    const state = stream[_stateAndFlags] & STATE_MASK;
    if (state === CLOSED || state === ERRORED) {
      return Promise_reject(
          createCannotActionOnStateStreamError('close', state));
    }

    // assert(state === WRITABLE || state === ERRORING,
    //        '_state_ is `"writable"` or `"erroring"`.');
    // assert(!WritableStreamCloseQueuedOrInFlight(stream),
    //        '! WritableStreamCloseQueuedOrInFlight(_stream_) is *false*.');
    const promise = v8.createPromise();
    stream[_closeRequest] = promise;

    if ((stream[_stateAndFlags] & BACKPRESSURE_FLAG) &&
        state === WRITABLE) {
      resolvePromise(writer[_readyPromise], undefined);
    }
    WritableStreamDefaultControllerClose(stream[_writableStreamController]);
    return promise;
  }

  function WritableStreamDefaultWriterCloseWithErrorPropagation(writer) {
    const stream = writer[_ownerWritableStream];
    // assert(stream !== undefined, 'stream is not undefined.');
    const state = stream[_stateAndFlags] & STATE_MASK;
    if (WritableStreamCloseQueuedOrInFlight(stream) || state === CLOSED) {
      return Promise_resolve(undefined);
    }
    if (state === ERRORED) {
      return Promise_reject(stream[_storedError]);
    }

    // assert(state === WRITABLE || state === ERRORING,
    //        '_state_ is `"writable"` or `"erroring"`.');

    return WritableStreamDefaultWriterClose(writer);
  }

  function WritableStreamDefaultWriterEnsureClosedPromiseRejected(
      writer, error) {
    if (promiseState(writer[_closedPromise]) === v8.kPROMISE_PENDING) {
      rejectPromise(writer[_closedPromise], error);
    } else {
      writer[_closedPromise] = Promise_reject(error);
    }
    markPromiseAsHandled(writer[_closedPromise]);
  }


  function WritableStreamDefaultWriterEnsureReadyPromiseRejected(
      writer, error) {
    if (promiseState(writer[_readyPromise]) === v8.kPROMISE_PENDING) {
      rejectPromise(writer[_readyPromise], error);
    } else {
      writer[_readyPromise] = Promise_reject(error);
    }
    markPromiseAsHandled(writer[_readyPromise]);
  }

  function WritableStreamDefaultWriterGetDesiredSize(writer) {
    const stream = writer[_ownerWritableStream];
    const state = stream[_stateAndFlags] & STATE_MASK;
    if (state === ERRORED || state === ERRORING) {
      return null;
    }
    if (state === CLOSED) {
      return 0;
    }
    return WritableStreamDefaultControllerGetDesiredSize(
        stream[_writableStreamController]);
  }

  function WritableStreamDefaultWriterRelease(writer) {
    const stream = writer[_ownerWritableStream];
    // assert(stream !== undefined,
    //        'stream is not undefined.');
    // assert(stream[_writer] === writer,
    //        'stream.[[writer]] is writer.');
    const releasedError = new TypeError(errReleasedWriterClosedPromise);
    WritableStreamDefaultWriterEnsureReadyPromiseRejected(
        writer, releasedError);
    WritableStreamDefaultWriterEnsureClosedPromiseRejected(
        writer, releasedError);
    stream[_writer] = undefined;
    writer[_ownerWritableStream] = undefined;
  }

  function WritableStreamDefaultWriterWrite(writer, chunk) {
    const stream = writer[_ownerWritableStream];
    // assert(stream !== undefined, 'stream is not undefined.');
    const controller = stream[_writableStreamController];
    const chunkSize =
        WritableStreamDefaultControllerGetChunkSize(controller, chunk);
    if (stream !== writer[_ownerWritableStream]) {
      return Promise_reject(createWriterLockReleasedError(verbWrittenTo));
    }
    const state = stream[_stateAndFlags] & STATE_MASK;
    if (state === ERRORED) {
      return Promise_reject(stream[_storedError]);
    }
    if (WritableStreamCloseQueuedOrInFlight(stream)) {
      return Promise_reject(new TypeError(
          templateErrorCannotActionOnStateStream('write to', 'closing')));
    }
    if (state === CLOSED) {
      return Promise_reject(
          createCannotActionOnStateStreamError('write to', CLOSED));
    }
    if (state === ERRORING) {
      return Promise_reject(stream[_storedError]);
    }
    // assert(state === WRITABLE, '_state_ is `"writable"`');
    const promise = WritableStreamAddWriteRequest(stream);
    WritableStreamDefaultControllerWrite(controller, chunk, chunkSize);
    return promise;
  }

  // Functions to expose internals for ReadableStream.pipeTo. These do not
  // appear in the standard.
  function getWritableStreamDefaultWriterClosedPromise(writer) {
    // assert(
    //     IsWritableStreamDefaultWriter(writer),
    //     'writer is a WritableStreamDefaultWriter.');
    return writer[_closedPromise];
  }

  function getWritableStreamDefaultWriterReadyPromise(writer) {
    // assert(
    //     IsWritableStreamDefaultWriter(writer),
    //     'writer is a WritableStreamDefaultWriter.');
    return writer[_readyPromise];
  }

  class WritableStreamDefaultController {
    constructor(stream, underlyingSink, size, highWaterMark) {
      if (!IsWritableStream(stream)) {
        throw new TypeError(streamErrors.illegalConstructor);
      }
      if (stream[_writableStreamController] !== undefined) {
        throw new TypeError(streamErrors.illegalConstructor);
      }
      this[_controlledWritableStream] = stream;
      this[_underlyingSink] = underlyingSink;
      // These are just initialised to avoid triggering the assert() in
      // ResetQueue. They are overwritten by ResetQueue().
      this[_queue] = undefined;
      this[_queueTotalSize] = undefined;
      ResetQueue(this);
      this[_started] = false;
      const normalizedStrategy =
          ValidateAndNormalizeQueuingStrategy(size, highWaterMark);
      this[_strategySize] = normalizedStrategy.size;
      this[_strategyHWM] = normalizedStrategy.highWaterMark;
      const backpressure = WritableStreamDefaultControllerGetBackpressure(this);
      WritableStreamUpdateBackpressure(stream, backpressure);
    }

    error(e) {
      if (!IsWritableStreamDefaultController(this)) {
        throw new TypeError(streamErrors.illegalInvocation);
      }
      const state =
          this[_controlledWritableStream][_stateAndFlags] & STATE_MASK;
      if (state !== WRITABLE) {
        return;
      }
      WritableStreamDefaultControllerError(this, e);
    }
  }

  // Writable Stream Default Controller Internal Methods

  // TODO(ricea): Virtual dispatch via V8 Private Symbols seems to be difficult
  // or impossible, so use static dispatch for now. This will have to be fixed
  // when adding a byte controller.
  function WritableStreamDefaultControllerAbortSteps(controller, reason) {
    return PromiseInvokeOrNoop(controller[_underlyingSink], 'abort', [reason]);
  }

  function WritableStreamDefaultControllerErrorSteps(controller) {
    ResetQueue(controller);
  }

  function WritableStreamDefaultControllerStartSteps(controller) {
    const startResult =
        InvokeOrNoop(controller[_underlyingSink], 'start', [controller]);
    const stream = controller[_controlledWritableStream];
    const startPromise = Promise_resolve(startResult);
    thenPromise(
        startPromise,
        () => {
          const state = stream[_stateAndFlags] & STATE_MASK;
          // assert(state === WRITABLE || state === ERRORING,
          //        '_stream_.[[state]] is `"writable"` or `"erroring"`');
          controller[_started] = true;
          WritableStreamDefaultControllerAdvanceQueueIfNeeded(controller);
        },
        r => {
          const state = stream[_stateAndFlags] & STATE_MASK;
          // assert(state === WRITABLE || state === ERRORING,
          //        '_stream_.[[state]] is `"writable"` or `"erroring"`');
          controller[_started] = true;
          WritableStreamDealWithRejection(stream, r);
        });
  }

  // Writable Stream Default Controller Abstract Operations

  function IsWritableStreamDefaultController(x) {
    return hasOwnPropertyNoThrow(x, _underlyingSink);
  }

  function WritableStreamDefaultControllerClose(controller) {
    EnqueueValueWithSize(controller, 'close', 0);
    WritableStreamDefaultControllerAdvanceQueueIfNeeded(controller);
  }

  function WritableStreamDefaultControllerGetChunkSize(controller, chunk) {
    const strategySize = controller[_strategySize];
    if (strategySize === undefined) {
      return 1;
    }
    let value;
    try {
      value = Function_call(strategySize, undefined, chunk);
    } catch (e) {
      WritableStreamDefaultControllerErrorIfNeeded(controller, e);
      return 1;
    }
    return value;
  }

  function WritableStreamDefaultControllerGetDesiredSize(controller) {
    return controller[_strategyHWM] - controller[_queueTotalSize];
  }

  function WritableStreamDefaultControllerWrite(controller, chunk, chunkSize) {
    const writeRecord = {chunk};
    try {
      EnqueueValueWithSize(controller, writeRecord, chunkSize);
    } catch (e) {
      WritableStreamDefaultControllerErrorIfNeeded(controller, e);
      return;
    }
    const stream = controller[_controlledWritableStream];
    if (!WritableStreamCloseQueuedOrInFlight(stream) &&
        (stream[_stateAndFlags] & STATE_MASK) === WRITABLE) {
      const backpressure =
          WritableStreamDefaultControllerGetBackpressure(controller);
      WritableStreamUpdateBackpressure(stream, backpressure);
    }
    WritableStreamDefaultControllerAdvanceQueueIfNeeded(controller);
  }

  function WritableStreamDefaultControllerAdvanceQueueIfNeeded(controller) {
    const stream = controller[_controlledWritableStream];
    if (!controller[_started]) {
      return;
    }
    if (stream[_inFlightWriteRequest] !== undefined) {
      return;
    }
    const state = stream[_stateAndFlags] & STATE_MASK;
    if (state === CLOSED || state === ERRORED) {
      return;
    }
    if (state === ERRORING) {
      WritableStreamFinishErroring(stream);
      return;
    }
    if (controller[_queue].length === 0) {
      return;
    }
    const writeRecord = PeekQueueValue(controller);
    if (writeRecord === 'close') {
      WritableStreamDefaultControllerProcessClose(controller);
    } else {
      WritableStreamDefaultControllerProcessWrite(controller,
                                                  writeRecord.chunk);
    }
  }

  function WritableStreamDefaultControllerErrorIfNeeded(controller, error) {
    const state =
        controller[_controlledWritableStream][_stateAndFlags] & STATE_MASK;
    if (state === WRITABLE) {
      WritableStreamDefaultControllerError(controller, error);
    }
  }

  function WritableStreamDefaultControllerProcessClose(controller) {
    const stream = controller[_controlledWritableStream];
    WritableStreamMarkCloseRequestInFlight(stream);
    DequeueValue(controller);
    // assert(controller[_queue].length === 0,
    //        'controller.[[queue]] is empty.');
    const sinkClosePromise = PromiseInvokeOrNoop(
        controller[_underlyingSink], 'close', []);
    thenPromise(
        sinkClosePromise,
        () => WritableStreamFinishInFlightClose(stream),
        reason => WritableStreamFinishInFlightCloseWithError(stream, reason)
        );
  }

  function WritableStreamDefaultControllerProcessWrite(controller, chunk) {
    const stream = controller[_controlledWritableStream];
    WritableStreamMarkFirstWriteRequestInFlight(stream);
    const sinkWritePromise = PromiseInvokeOrNoop(controller[_underlyingSink],
                                                 'write', [chunk, controller]);
    thenPromise(
        sinkWritePromise,
        () => {
          WritableStreamFinishInFlightWrite(stream);
          const state = stream[_stateAndFlags] & STATE_MASK;
          // assert(state === WRITABLE || state === ERRORING,
          //        '_state_ is `"writable"` or `"erroring"`');
          DequeueValue(controller);
          if (!WritableStreamCloseQueuedOrInFlight(stream) &&
              state === WRITABLE) {
            const backpressure =
                WritableStreamDefaultControllerGetBackpressure(controller);
            WritableStreamUpdateBackpressure(stream, backpressure);
          }
          WritableStreamDefaultControllerAdvanceQueueIfNeeded(controller);
        },
        reason => {
          WritableStreamFinishInFlightWriteWithError(stream, reason);
        });
  }

  function WritableStreamDefaultControllerGetBackpressure(controller) {
    const desiredSize =
        WritableStreamDefaultControllerGetDesiredSize(controller);
    return desiredSize <= 0;
  }

  function WritableStreamDefaultControllerError(controller, error) {
    const stream = controller[_controlledWritableStream];
    // assert((stream[_stateAndFlags] & STATE_MASK) === WRITABLE,
    //        '_stream_.[[state]] is `"writable"`.');
    WritableStreamStartErroring(stream, error);
  }

  // Queue-with-Sizes Operations
  //
  // TODO(ricea): Share these operations with ReadableStream.js.
  function DequeueValue(container) {
    // assert(
    //     hasOwnProperty(container, _queue) &&
    //         hasOwnProperty(container, _queueTotalSize),
    //     'Assert: _container_ has [[queue]] and [[queueTotalSize]] internal ' +
    //         'slots.');
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
    //     'Assert: _container_ has [[queue]] and [[queueTotalSize]] internal ' +
    //         'slots.');
    size = Number(size);
    if (!IsFiniteNonNegativeNumber(size)) {
      throw new RangeError(streamErrors.invalidSize);
    }

    container[_queue].push({value, size});
    container[_queueTotalSize] += size;
  }

  function PeekQueueValue(container) {
    // assert(
    //     hasOwnProperty(container, _queue) &&
    //         hasOwnProperty(container, _queueTotalSize),
    //     'Assert: _container_ has [[queue]] and [[queueTotalSize]] internal ' +
    //         'slots.');
    // assert(container[_queue].length !== 0,
    //        '_container_.[[queue]] is not empty.');
    const pair = container[_queue].peek();
    return pair.value;
  }

  function ResetQueue(container) {
    // assert(
    //     hasOwnProperty(container, _queue) &&
    //         hasOwnProperty(container, _queueTotalSize),
    //     'Assert: _container_ has [[queue]] and [[queueTotalSize]] internal ' +
    //         'slots.');
    container[_queue] = new binding.SimpleQueue();
    container[_queueTotalSize] = 0;
  }

  // Miscellaneous Operations

  // This differs from "CallOrNoop" in the ReadableStream implementation in
  // that it takes the arguments as an array, so that multiple arguments can be
  // passed.
  //
  // TODO(ricea): Consolidate with ReadableStream implementation.
  function InvokeOrNoop(O, P, args) {
    // assert(IsPropertyKey(P),
    //        'P is a valid property key.');
    if (args === undefined) {
      args = [];
    }
    const method = O[P];
    if (method === undefined) {
      return undefined;
    }
    if (typeof method !== 'function') {
      throw new TypeError(templateErrorIsNotAFunction(P));
    }
    return Function_apply(method, O, args);
  }

  function IsFiniteNonNegativeNumber(v) {
    return Number_isFinite(v) && v >= 0;
  }

  function PromiseInvokeOrNoop(O, P, args) {
    try {
      return Promise_resolve(InvokeOrNoop(O, P, args));
    } catch (e) {
      return Promise_reject(e);
    }
  }

  // TODO(ricea): Share this operation with ReadableStream.js.
  function ValidateAndNormalizeQueuingStrategy(size, highWaterMark) {
    if (size !== undefined && typeof size !== 'function') {
      throw new TypeError(streamErrors.sizeNotAFunction);
    }

    highWaterMark = Number(highWaterMark);
    if (Number_isNaN(highWaterMark)) {
      throw new RangeError(streamErrors.invalidHWM);
    }
    if (highWaterMark < 0) {
      throw new RangeError(streamErrors.invalidHWM);
    }

    return {size, highWaterMark};
  }

  //
  // Additions to the global object
  //

  defineProperty(global, 'WritableStream', {
    value: WritableStream,
    enumerable: false,
    configurable: true,
    writable: true
  });

  // TODO(ricea): Exports to Blink

  // Exports for ReadableStream
  binding.AcquireWritableStreamDefaultWriter =
      AcquireWritableStreamDefaultWriter;
  binding.IsWritableStream = IsWritableStream;
  binding.isWritableStreamClosingOrClosed = isWritableStreamClosingOrClosed;
  binding.isWritableStreamErrored = isWritableStreamErrored;
  binding.IsWritableStreamLocked = IsWritableStreamLocked;
  binding.WritableStreamAbort = WritableStreamAbort;
  binding.WritableStreamDefaultWriterCloseWithErrorPropagation =
      WritableStreamDefaultWriterCloseWithErrorPropagation;
  binding.getWritableStreamDefaultWriterClosedPromise =
      getWritableStreamDefaultWriterClosedPromise;
  binding.WritableStreamDefaultWriterGetDesiredSize =
      WritableStreamDefaultWriterGetDesiredSize;
  binding.getWritableStreamDefaultWriterReadyPromise =
      getWritableStreamDefaultWriterReadyPromise;
  binding.WritableStreamDefaultWriterRelease =
      WritableStreamDefaultWriterRelease;
  binding.WritableStreamDefaultWriterWrite = WritableStreamDefaultWriterWrite;
  binding.getWritableStreamStoredError = getWritableStreamStoredError;
});
