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

  // Numeric encodings of states
  const WRITABLE = 0;
  const CLOSED = 1;
  const ERRORED = 2;

  // Mask to extract or assign states to _stateAndFlags
  const STATE_MASK = 0xF;

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
    TEMP_ASSERT(stateNames[state] !== undefined,
                `name for state ${state} exists in stateNames`);
    return new TypeError(
        templateErrorCannotActionOnStateStream(action, stateNames[state]));
  }

  function rejectPromises(queue, e) {
    queue.forEach(promise => v8.rejectPromise(promise, e));
  }

  // https://tc39.github.io/ecma262/#sec-ispropertykey
  // TODO(ricea): Remove this when the asserts using it are removed.
  function IsPropertyKey(argument) {
    return typeof argument === 'string' || typeof argument === 'symbol';
  }

  // TODO(ricea): Remove all asserts once the implementation has stabilised.
  function TEMP_ASSERT(predicate, message) {
    if (predicate) {
      return;
    }
    v8.log(`Assertion failed: ${message}\n`);
    v8.logStackTrace();
    class WritableStreamInternalError extends Error {
      constructor(message) {
        super(message);
      }
    }
    throw new WritableStreamInternalError(message);
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
    return hasOwnProperty(x, _writableStreamController);
  }

  function IsWritableStreamLocked(stream) {
    TEMP_ASSERT(IsWritableStream(stream),
                '! IsWritableStream(stream) is true.');
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
    TEMP_ASSERT(state === WRITABLE,
               'state is "writable".');
    const error = new TypeError(errStreamAborting);
    if (stream[_pendingAbortRequest] !== undefined) {
      return Promise_reject(error);
    }

    const controller = stream[_writableStreamController];
    TEMP_ASSERT(controller !== undefined,
                'controller is not undefined');
    if (!WritableStreamHasOperationMarkedInFlight(stream) &&
        controller[_started]) {
      WritableStreamFinishAbort(stream);
      return WritableStreamDefaultControllerAbortSteps(controller, reason);
    }
    const writer = stream[_writer];
    if (writer !== undefined) {
      WritableStreamDefaultWriterEnsureReadyPromiseRejected(writer, error);
    }
    const promise = v8.createPromise();
    stream[_pendingAbortRequest] = {promise, reason};
    return promise;
  }

  function WritableStreamError(stream, error) {
    stream[_stateAndFlags] = (stream[_stateAndFlags] & ~STATE_MASK) | ERRORED;
    stream[_storedError] = error;
    WritableStreamDefaultControllerErrorSteps(stream[_writableStreamController]);
    if (stream[_pendingAbortRequest] === undefined) {
      const writer = stream[_writer];
      if (writer !== undefined) {
        WritableStreamDefaultWriterEnsureReadyPromiseRejected(writer, error);
      }
    }
    if (!WritableStreamHasOperationMarkedInFlight(stream)) {
      WritableStreamRejectPromisesInReactionToError(stream);
    }
  }

  function WritableStreamFinishAbort(stream) {
    const error = new TypeError(errStreamAborted);
    WritableStreamError(stream, error);
  }

  // Writable Stream Abstract Operations Used by Controllers

  function WritableStreamAddWriteRequest(stream) {
    TEMP_ASSERT(IsWritableStreamLocked(stream),
                '! IsWritableStreamLocked(writer) is true.');
    TEMP_ASSERT((stream[_stateAndFlags] & STATE_MASK) === WRITABLE,
                'stream.[[state]] is "writable".');
    const promise = v8.createPromise();
    stream[_writeRequests].push(promise);
    return promise;
  }

  function WritableStreamFinishInFlightWrite(stream) {
    TEMP_ASSERT(stream[_inFlightWriteRequest] !== undefined,
                '_stream_.[[inFlightWriteRequest]] is not *undefined*.');
    v8.resolvePromise(stream[_inFlightWriteRequest], undefined);
    stream[_inFlightWriteRequest] = undefined;
    const state = stream[_stateAndFlags] & STATE_MASK;
    if (state === ERRORED) {
      WritableStreamFinishInFlightWriteInErroredState(stream);
      return;
    }
    TEMP_ASSERT(state === WRITABLE, '_state_ is `"writable"`.');
    WritableStreamHandleAbortRequestIfPending(stream);
  }

  function WritableStreamFinishInFlightWriteInErroredState(stream) {
    WritableStreamRejectAbortRequestIfPending(stream);
    WritableStreamRejectPromisesInReactionToError(stream);
  }

  function WritableStreamFinishInFlightWriteWithError(stream, error) {
    TEMP_ASSERT(stream[_inFlightWriteRequest] !== undefined,
                '_stream_.[[inFlightWriteRequest]] is not *undefined*.');
    v8.rejectPromise(stream[_inFlightWriteRequest], error);
    stream[_inFlightWriteRequest] = undefined;
    const state = stream[_stateAndFlags] & STATE_MASK;
    if (state === ERRORED) {
      WritableStreamFinishInFlightWriteInErroredState(stream);
      return;
    }
    TEMP_ASSERT(state === WRITABLE, '_state_ is `"writable"`.');
    WritableStreamError(stream, error);
    WritableStreamRejectAbortRequestIfPending(stream);
  }

  function WritableStreamFinishInFlightClose(stream) {
    TEMP_ASSERT(stream[_inFlightCloseRequest] !== undefined,
                '_stream_.[[inFlightCloseRequest]] is not *undefined*.');
    v8.resolvePromise(stream[_inFlightCloseRequest], undefined);
    stream[_inFlightCloseRequest] = undefined;
    const state = stream[_stateAndFlags] & STATE_MASK;
    if (state === ERRORED) {
      WritableStreamFinishInFlightCloseInErroredState(stream);
      return;
    }
    TEMP_ASSERT(state === WRITABLE, '_state_ is `"writable"`.');
    stream[_stateAndFlags] = (stream[_stateAndFlags] & ~STATE_MASK) | CLOSED;
    const writer = stream[_writer];
    if (writer !== undefined) {
      v8.resolvePromise(writer[_closedPromise], undefined);
    }
    if (stream[_pendingAbortRequest] !== undefined) {
      v8.resolvePromise(stream[_pendingAbortRequest].promise, undefined);
      stream[_pendingAbortRequest] = undefined;
    }
  }

  function WritableStreamFinishInFlightCloseInErroredState(stream) {
    WritableStreamRejectAbortRequestIfPending(stream);
    WritableStreamRejectClosedPromiseInReactionToError(stream);
  }

  function WritableStreamFinishInFlightCloseWithError(stream, error) {
    TEMP_ASSERT(stream[_inFlightCloseRequest] !== undefined,
                '_stream_.[[inFlightCloseRequest]] is not *undefined*.');
    v8.rejectPromise(stream[_inFlightCloseRequest], error);
    stream[_inFlightCloseRequest] = undefined;
    const state = stream[_stateAndFlags] & STATE_MASK;
    if (state === ERRORED) {
      WritableStreamFinishInFlightCloseInErroredState(stream);
      return;
    }
    TEMP_ASSERT(state === WRITABLE, '_state_ is `"writable"`.');
    WritableStreamError(stream, error);
    WritableStreamRejectAbortRequestIfPending(stream);
  }

  function WritableStreamCloseQueuedOrInFlight(stream) {
    return stream[_closeRequest] !== undefined ||
        stream[_inFlightCloseRequest] !== undefined;
  }

  function WritableStreamHandleAbortRequestIfPending(stream) {
    if (stream[_pendingAbortRequest] === undefined) {
      return;
    }
    WritableStreamFinishAbort(stream);
    const abortRequest = stream[_pendingAbortRequest];
    stream[_pendingAbortRequest] = undefined;
    const promise =
        WritableStreamDefaultControllerAbortSteps(stream[_writableStreamController],
                                                  abortRequest.reason);
    thenPromise(promise,
                result => v8.resolvePromise(abortRequest.promise, result),
                reason => v8.rejectPromise(abortRequest.promise, reason));
  }

  function WritableStreamHasOperationMarkedInFlight(stream) {
    return stream[_inFlightWriteRequest] !== undefined ||
        stream[_inFlightCloseRequest] !== undefined;
  }

  function WritableStreamMarkCloseRequestInFlight(stream) {
    TEMP_ASSERT(stream[_inFlightCloseRequest] === undefined,
                '_stream_.[[inFlightCloseRequest]] is *undefined*.');
    TEMP_ASSERT(stream[_closeRequest] !== undefined,
                '_stream_.[[closeRequest]] is not *undefined*.');
    stream[_inFlightCloseRequest] = stream[_closeRequest];
    stream[_closeRequest] = undefined;
  }

  function WritableStreamMarkFirstWriteRequestInFlight(stream) {
    TEMP_ASSERT(stream[_inFlightWriteRequest] === undefined,
                '_stream_.[[inFlightWriteRequest]] is *undefined*.');
    TEMP_ASSERT(stream[_writeRequests].length !== 0,
                '_stream_.[[writeRequests]] is not empty.');
    const writeRequest = stream[_writeRequests].shift();
    stream[_inFlightWriteRequest] = writeRequest;
  }

  function WritableStreamRejectClosedPromiseInReactionToError(stream) {
    const writer = stream[_writer];
    if (writer !== undefined) {
      v8.rejectPromise(writer[_closedPromise], stream[_storedError]);
      v8.markPromiseAsHandled(writer[_closedPromise]);
    }
  }

  function WritableStreamRejectAbortRequestIfPending(stream) {
    if (stream[_pendingAbortRequest] !== undefined) {
      v8.rejectPromise(stream[_pendingAbortRequest].promise,
                       stream[_storedError]);
      stream[_pendingAbortRequest] = undefined;
    }
  }

  function WritableStreamRejectPromisesInReactionToError(stream) {
    const storedError = stream[_storedError];
    rejectPromises(stream[_writeRequests], storedError);
    stream[_writeRequests] = new binding.SimpleQueue();

    if (stream[_closeRequest] !== undefined) {
      TEMP_ASSERT(stream[_inFlightCloseRequest] === undefined,
                  '_stream_.[[inFlightCloseRequest]] is *undefined*.');
      v8.rejectPromise(stream[_closeRequest], storedError);
      stream[_closeRequest] = undefined;
    }

    WritableStreamRejectClosedPromiseInReactionToError(stream);
  }

  function WritableStreamUpdateBackpressure(stream, backpressure) {
    TEMP_ASSERT((stream[_stateAndFlags] & STATE_MASK) === WRITABLE,
                'stream.[[state]] is "writable".');
    TEMP_ASSERT(!WritableStreamCloseQueuedOrInFlight(stream),
                'WritableStreamCloseQueuedOrInFlight(_stream_) is *false*.');
    const writer = stream[_writer];
    if (writer !== undefined &&
        backpressure !== Boolean(stream[_stateAndFlags] & BACKPRESSURE_FLAG)) {
      if (backpressure) {
        writer[_readyPromise] = v8.createPromise();
      } else {
        TEMP_ASSERT(!backpressure, '_backpressure_ is *false*.');
        v8.resolvePromise(writer[_readyPromise], undefined);
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
    TEMP_ASSERT(
        IsWritableStream(stream), '! IsWritableStream(stream) is true.');
    return (stream[_stateAndFlags] & STATE_MASK) === ERRORED;
  }

  function isWritableStreamClosingOrClosed(stream) {
    TEMP_ASSERT(
        IsWritableStream(stream), '! IsWritableStream(stream) is true.');
    return WritableStreamCloseQueuedOrInFlight(stream) ||
        (stream[_stateAndFlags] & STATE_MASK) === CLOSED;
  }

  function getWritableStreamStoredError(stream) {
    TEMP_ASSERT(
        IsWritableStream(stream), '! IsWritableStream(stream) is true.');
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
      if (state === WRITABLE) {
        if (stream[_pendingAbortRequest] !== undefined) {
          const error = new TypeError(errStreamAborting);
          this[_readyPromise] = Promise_reject(error);
          v8.markPromiseAsHandled(this[_readyPromise]);
        } else if (!WritableStreamCloseQueuedOrInFlight(stream) &&
            stream[_stateAndFlags] & BACKPRESSURE_FLAG) {
          this[_readyPromise] = v8.createPromise();
        } else {
          this[_readyPromise] = Promise_resolve(undefined);
        }
        this[_closedPromise] = v8.createPromise();
      } else if (state === CLOSED) {
        this[_readyPromise] = Promise_resolve(undefined);
        this[_closedPromise] = Promise_resolve(undefined);
      } else {
        TEMP_ASSERT(state === ERRORED, '_state_ is `"errored"`.');
        const storedError = stream[_storedError];
        this[_readyPromise] = Promise_reject(storedError);
        v8.markPromiseAsHandled(this[_readyPromise]);
        this[_closedPromise] = Promise_reject(storedError);
        v8.markPromiseAsHandled(this[_closedPromise]);
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
      TEMP_ASSERT(stream[_writer] !== undefined,
                  'stream.[[writer]] is not undefined.');
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
    return hasOwnProperty(x, _ownerWritableStream);
  }

  function WritableStreamDefaultWriterAbort(writer, reason) {
    const stream = writer[_ownerWritableStream];
    TEMP_ASSERT(stream !== undefined,
                'stream is not undefined.');
    return WritableStreamAbort(stream, reason);
  }

  function WritableStreamDefaultWriterClose(writer) {
    const stream = writer[_ownerWritableStream];
    TEMP_ASSERT(stream !== undefined, 'stream is not undefined.');
    const state = stream[_stateAndFlags] & STATE_MASK;
    if (state === CLOSED || state === ERRORED) {
      return Promise_reject(
          createCannotActionOnStateStreamError('close', state));
    }
    if (stream[_pendingAbortRequest] !== undefined) {
      return Promise_reject(new TypeError(errStreamAborting));
    }
    TEMP_ASSERT(state === WRITABLE, '_state_ is `"writable"`.');
    TEMP_ASSERT(!WritableStreamCloseQueuedOrInFlight(stream),
                '! WritableStreamCloseQueuedOrInFlight(_stream_) is *false*.');
    const promise = v8.createPromise();
    stream[_closeRequest] = promise;
    if (stream[_stateAndFlags] & BACKPRESSURE_FLAG) {
      v8.resolvePromise(writer[_readyPromise], undefined);
    }
    WritableStreamDefaultControllerClose(stream[_writableStreamController]);
    return promise;
  }

  function WritableStreamDefaultWriterCloseWithErrorPropagation(writer) {
    const stream = writer[_ownerWritableStream];
    TEMP_ASSERT(stream !== undefined, 'stream is not undefined.');
    const state = stream[_stateAndFlags] & STATE_MASK;
    if (WritableStreamCloseQueuedOrInFlight(stream) || state === CLOSED) {
      return Promise_resolve(undefined);
    }
    if (state === ERRORED) {
      return Promise_reject(stream[_storedError]);
    }
    TEMP_ASSERT(state === WRITABLE, 'state is "writable".');
    return WritableStreamDefaultWriterClose(writer);
  }

  function WritableStreamDefaultWriterEnsureReadyPromiseRejected(writer,
                                                                 error) {
    if (v8.promiseState(writer[_readyPromise]) === v8.kPROMISE_PENDING) {
      v8.rejectPromise(writer[_readyPromise], error);
    } else {
      writer[_readyPromise] = Promise_reject(error);
    }
    v8.markPromiseAsHandled(writer[_readyPromise]);
  }

  function WritableStreamDefaultWriterGetDesiredSize(writer) {
    const stream = writer[_ownerWritableStream];
    const state = stream[_stateAndFlags] & STATE_MASK;
    if (state === ERRORED || stream[_pendingAbortRequest] !== undefined) {
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
    TEMP_ASSERT(stream !== undefined,
                'stream is not undefined.');
    TEMP_ASSERT(stream[_writer] === writer,
                'stream.[[writer]] is writer.');
    const releasedError = new TypeError(errReleasedWriterClosedPromise);
    const state = stream[_stateAndFlags] & STATE_MASK;
    WritableStreamDefaultWriterEnsureReadyPromiseRejected(writer,
                                                          releasedError);
    if (state === WRITABLE ||
        WritableStreamHasOperationMarkedInFlight(stream)) {
      v8.rejectPromise(writer[_closedPromise], releasedError);
    } else {
      writer[_closedPromise] = Promise_reject(releasedError);
    }
    v8.markPromiseAsHandled(writer[_closedPromise]);
    stream[_writer] = undefined;
    writer[_ownerWritableStream] = undefined;
  }

  function WritableStreamDefaultWriterWrite(writer, chunk) {
    const stream = writer[_ownerWritableStream];
    TEMP_ASSERT(stream !== undefined, 'stream is not undefined.');
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
    if (stream[_pendingAbortRequest] !== undefined) {
      return Promise_reject(new TypeError(errStreamAborting));
    }
    const promise = WritableStreamAddWriteRequest(stream);
    WritableStreamDefaultControllerWrite(controller, chunk, chunkSize);
    return promise;
  }

  // Functions to expose internals for ReadableStream.pipeTo. These do not
  // appear in the standard.
  function getWritableStreamDefaultWriterClosedPromise(writer) {
    TEMP_ASSERT(
        IsWritableStreamDefaultWriter(writer),
        'writer is a WritableStreamDefaultWriter.');
    return writer[_closedPromise];
  }

  function getWritableStreamDefaultWriterReadyPromise(writer) {
    TEMP_ASSERT(
        IsWritableStreamDefaultWriter(writer),
        'writer is a WritableStreamDefaultWriter.');
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
      const state = this[_controlledWritableStream][_stateAndFlags] & STATE_MASK;
      if (state === CLOSED || state === ERRORED) {
        throw createCannotActionOnStateStreamError('error', state);
      }
      WritableStreamDefaultControllerError(this, e);
    }
  }

  // Writable Stream Default Controller Internal Methods

  // TODO(ricea): Virtual dispatch via V8 Private Symbols seems to be difficult
  // or impossible, so use static dispatch for now. This will have to be fixed
  // when adding a byte controller.
  function WritableStreamDefaultControllerAbortSteps(controller, reason) {
    const sinkAbortPromise =
        PromiseInvokeOrNoop(controller[_underlyingSink], 'abort', [reason]);
    return thenPromise(sinkAbortPromise, () => undefined);
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
          controller[_started] = true;
          if ((stream[_stateAndFlags] & STATE_MASK) === ERRORED) {
            WritableStreamRejectAbortRequestIfPending(stream);
          } else {
            WritableStreamHandleAbortRequestIfPending(stream);
          }
          WritableStreamDefaultControllerAdvanceQueueIfNeeded(controller);
        },
        r => {
          TEMP_ASSERT(
              (stream[_stateAndFlags] & STATE_MASK) === WRITABLE ||
                  (stream[_stateAndFlags] & STATE_MASK) === ERRORED,
              '_stream_.[[state]] is `"writable"` or `"errored"`.');
          WritableStreamDefaultControllerErrorIfNeeded(controller, r);
          WritableStreamRejectAbortRequestIfPending(stream);
        });
  }

  // Writable Stream Default Controller Abstract Operations

  function IsWritableStreamDefaultController(x) {
    return hasOwnProperty(x, _underlyingSink);
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
    if (!WritableStreamCloseQueuedOrInFlight(stream)) {
      const backpressure =
          WritableStreamDefaultControllerGetBackpressure(controller);
      WritableStreamUpdateBackpressure(stream, backpressure);
    }
    WritableStreamDefaultControllerAdvanceQueueIfNeeded(controller);
  }

  function WritableStreamDefaultControllerAdvanceQueueIfNeeded(controller) {
    const stream = controller[_controlledWritableStream];
    const state = stream[_stateAndFlags] & STATE_MASK;
    if (state === CLOSED || state === ERRORED) {
      return;
    }
    if (!controller[_started]) {
      return;
    }
    if (stream[_inFlightWriteRequest] !== undefined) {
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
    const state = controller[_controlledWritableStream][_stateAndFlags] & STATE_MASK;
    if (state === WRITABLE) {
      WritableStreamDefaultControllerError(controller, error);
    }
  }

  function WritableStreamDefaultControllerProcessClose(controller) {
    const stream = controller[_controlledWritableStream];
    WritableStreamMarkCloseRequestInFlight(stream);
    DequeueValue(controller);
    TEMP_ASSERT(controller[_queue].length === 0,
                'controller.[[queue]] is empty.');
    const sinkClosePromise = PromiseInvokeOrNoop(controller[_underlyingSink],
                                                 'close', [controller]);
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
          if (state === ERRORED) {
            return;
          }
          TEMP_ASSERT(state === WRITABLE, '_state_ is `"writable"`.');
          DequeueValue(controller);
          if (!WritableStreamCloseQueuedOrInFlight(stream)) {
            const backpressure =
                WritableStreamDefaultControllerGetBackpressure(controller);
            WritableStreamUpdateBackpressure(stream, backpressure);
          }
          WritableStreamDefaultControllerAdvanceQueueIfNeeded(controller);
        },
        reason => {
          const wasErrored = (stream[_stateAndFlags] & STATE_MASK) === ERRORED;
          WritableStreamFinishInFlightWriteWithError(stream, reason);
          TEMP_ASSERT((stream[_stateAndFlags] & STATE_MASK) === ERRORED,
                      '_stream_.[[state]] is `"errored"`.');
          if (!wasErrored) {
            ResetQueue(controller);
          }
        });
  }

  function WritableStreamDefaultControllerGetBackpressure(controller) {
    const desiredSize =
        WritableStreamDefaultControllerGetDesiredSize(controller);
    return desiredSize <= 0;
  }

  function WritableStreamDefaultControllerError(controller, error) {
    const stream = controller[_controlledWritableStream];
    TEMP_ASSERT((stream[_stateAndFlags] & STATE_MASK) === WRITABLE,
                '_stream_.[[state]] is `"writable"`.');
    WritableStreamError(stream, error);
  }

  // Queue-with-Sizes Operations
  //
  // TODO(ricea): Share these operations with ReadableStream.js.
  function DequeueValue(container) {
    TEMP_ASSERT(
        hasOwnProperty(container, _queue) &&
            hasOwnProperty(container, _queueTotalSize),
        'Assert: _container_ has [[queue]] and [[queueTotalSize]] internal ' +
            'slots.');
    TEMP_ASSERT(container[_queue].length !== 0,
                '_container_.[[queue]] is not empty.');
    const pair = container[_queue].shift();
    container[_queueTotalSize] -= pair.size;
    if (container[_queueTotalSize] < 0) {
      container[_queueTotalSize] = 0;
    }
    return pair.value;
  }

  function EnqueueValueWithSize(container, value, size) {
    TEMP_ASSERT(
        hasOwnProperty(container, _queue) &&
            hasOwnProperty(container, _queueTotalSize),
        'Assert: _container_ has [[queue]] and [[queueTotalSize]] internal ' +
            'slots.');
    size = Number(size);
    if (!IsFiniteNonNegativeNumber(size)) {
      throw new RangeError(streamErrors.invalidSize);
    }

    container[_queue].push({value, size});
    container[_queueTotalSize] += size;
  }

  function PeekQueueValue(container) {
    TEMP_ASSERT(
        hasOwnProperty(container, _queue) &&
            hasOwnProperty(container, _queueTotalSize),
        'Assert: _container_ has [[queue]] and [[queueTotalSize]] internal ' +
            'slots.');
    TEMP_ASSERT(container[_queue].length !== 0,
                '_container_.[[queue]] is not empty.');
    const pair = container[_queue].peek();
    return pair.value;
  }

  function ResetQueue(container) {
    TEMP_ASSERT(
        hasOwnProperty(container, _queue) &&
            hasOwnProperty(container, _queueTotalSize),
        'Assert: _container_ has [[queue]] and [[queueTotalSize]] internal ' +
            'slots.');
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
    TEMP_ASSERT(IsPropertyKey(P),
                'P is a valid property key.');
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
      throw new RangeError(streamErrors.errInvalidHWM);
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
