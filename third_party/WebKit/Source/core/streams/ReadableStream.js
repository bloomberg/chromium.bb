// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, binding, v8) {
  'use strict';

  const _reader = v8.createPrivateSymbol('[[reader]]');
  const _storedError = v8.createPrivateSymbol('[[storedError]]');
  const _controller = v8.createPrivateSymbol('[[controller]]');

  const _closedPromise = v8.createPrivateSymbol('[[closedPromise]]');
  const _ownerReadableStream =
      v8.createPrivateSymbol('[[ownerReadableStream]]');

  const _readRequests = v8.createPrivateSymbol('[[readRequests]]');

  const createWithExternalControllerSentinel =
      v8.createPrivateSymbol('flag for UA-created ReadableStream to pass');

  const _readableStreamBits = v8.createPrivateSymbol('bit field for [[state]] and [[disturbed]]');
  const DISTURBED = 0b1;
  // The 2nd and 3rd bit are for [[state]].
  const STATE_MASK = 0b110;
  const STATE_BITS_OFFSET = 1;
  const STATE_READABLE = 0;
  const STATE_CLOSED = 1;
  const STATE_ERRORED = 2;

  const _underlyingSource = v8.createPrivateSymbol('[[underlyingSource]]');
  const _controlledReadableStream =
      v8.createPrivateSymbol('[[controlledReadableStream]]');
  const _queue = v8.createPrivateSymbol('[[queue]]');
  const _totalQueuedSize = v8.createPrivateSymbol('[[totalQueuedSize]]');
  const _strategySize = v8.createPrivateSymbol('[[strategySize]]');
  const _strategyHWM = v8.createPrivateSymbol('[[strategyHWM]]');

  const _readableStreamDefaultControllerBits = v8.createPrivateSymbol(
      'bit field for [[started]], [[closeRequested]], [[pulling]], [[pullAgain]]');
  const STARTED = 0b1;
  const CLOSE_REQUESTED = 0b10;
  const PULLING = 0b100;
  const PULL_AGAIN = 0b1000;
  const EXTERNALLY_CONTROLLED = 0b10000;

  const undefined = global.undefined;
  const Infinity = global.Infinity;

  const defineProperty = global.Object.defineProperty;
  const hasOwnProperty = v8.uncurryThis(global.Object.hasOwnProperty);
  const callFunction = v8.uncurryThis(global.Function.prototype.call);
  const applyFunction = v8.uncurryThis(global.Function.prototype.apply);

  const TypeError = global.TypeError;
  const RangeError = global.RangeError;

  const Number = global.Number;
  const Number_isNaN = Number.isNaN;
  const Number_isFinite = Number.isFinite;

  const Promise = global.Promise;
  const thenPromise = v8.uncurryThis(Promise.prototype.then);
  const Promise_resolve = v8.simpleBind(Promise.resolve, Promise);
  const Promise_reject = v8.simpleBind(Promise.reject, Promise);

  const streamErrors = binding.streamErrors;
  const errCancelLockedStream =
      'Cannot cancel a readable stream that is locked to a reader';
  const errEnqueueCloseRequestedStream =
      'Cannot enqueue a chunk into a readable stream that is closed or has been requested to be closed';
  const errCancelReleasedReader =
      'This readable stream reader has been released and cannot be used to cancel its previous owner stream';
  const errReadReleasedReader =
      'This readable stream reader has been released and cannot be used to read from its previous owner stream';
  const errCloseCloseRequestedStream =
      'Cannot close a readable stream that has already been requested to be closed';
  const errEnqueueClosedStream = 'Cannot enqueue a chunk into a closed readable stream';
  const errEnqueueErroredStream = 'Cannot enqueue a chunk into an errored readable stream';
  const errCloseClosedStream = 'Cannot close a closed readable stream';
  const errCloseErroredStream = 'Cannot close an errored readable stream';
  const errErrorClosedStream = 'Cannot error a close readable stream';
  const errErrorErroredStream =
      'Cannot error a readable stream that is already errored';
  const errGetReaderNotByteStream = 'This readable stream does not support BYOB readers';
  const errGetReaderBadMode = 'Invalid reader mode given: expected undefined or "byob"';
  const errReaderConstructorBadArgument =
      'ReadableStreamReader constructor argument is not a readable stream';
  const errReaderConstructorStreamAlreadyLocked =
      'ReadableStreamReader constructor can only accept readable streams that are not yet locked to a reader';
  const errReleaseReaderWithPendingRead =
      'Cannot release a readable stream reader when it still has outstanding read() calls that have not yet settled';
  const errReleasedReaderClosedPromise =
      'This readable stream reader has been released and cannot be used to monitor the stream\'s state';

  const errTmplMustBeFunctionOrUndefined = name =>
      `${name} must be a function or undefined`;
  const errCannotPipeLockedStream = 'Cannot pipe a locked stream';
  const errCannotPipeToALockedStream = 'Cannot pipe to a locked stream';
  const errDestinationStreamClosed = 'Destination stream closed';

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

      this[_readableStreamBits] = 0b0;
      ReadableStreamSetState(this, STATE_READABLE);
      this[_reader] = undefined;
      this[_storedError] = undefined;

      // Avoid allocating the controller if the stream is going to be controlled
      // externally (i.e. from C++) anyway. All calls to underlyingSource
      // methods will disregard their controller argument in such situations
      // (but see below).

      this[_controller] = undefined;

      const type = underlyingSource.type;
      const typeString = String(type);
      if (typeString === 'bytes') {
        throw new RangeError('bytes type is not yet implemented');
      } else if (type !== undefined) {
        throw new RangeError(streamErrors.invalidType);
      }

      this[_controller] =
          new ReadableStreamDefaultController(this, underlyingSource, size, highWaterMark, arguments[2] === createWithExternalControllerSentinel);
    }

    get locked() {
      if (IsReadableStream(this) === false) {
        throw new TypeError(streamErrors.illegalInvocation);
      }

      return IsReadableStreamLocked(this);
    }

    cancel(reason) {
      if (IsReadableStream(this) === false) {
        return Promise_reject(new TypeError(streamErrors.illegalInvocation));
      }

      if (IsReadableStreamLocked(this) === true) {
        return Promise_reject(new TypeError(errCancelLockedStream));
      }

      return ReadableStreamCancel(this, reason);
    }

    getReader({ mode } = {}) {
      if (IsReadableStream(this) === false) {
        throw new TypeError(streamErrors.illegalInvocation);
      }

      if (mode === 'byob') {
        // TODO(ricea): When BYOB readers are supported:
        //
        // a. If
        // ! IsReadableByteStreamController(this.[[_controller]])
        // is false, throw a TypeError exception.
        // b. Return ? AcquireReadableStreamBYOBReader(this).
        throw new TypeError(errGetReaderNotByteStream);
      }

      if (mode === undefined) {
        return AcquireReadableStreamDefaultReader(this);
      }

      throw new RangeError(errGetReaderBadMode);
    }

    pipeThrough({writable, readable}, options) {
      const promise = this.pipeTo(writable, options);
      if (v8.isPromise(promise)) {
        v8.markPromiseAsHandled(promise);
      }
      return readable;
    }

    pipeTo(dest, {preventClose, preventAbort, preventCancel} = {}) {
      if (!IsReadableStream(this)) {
        return Promise_reject(new TypeError(streamErrors.illegalInvocation));
      }

      if (!binding.IsWritableStream(dest)) {
        // TODO(ricea): Think about having a better error message.
        return Promise_reject(new TypeError(streamErrors.illegalInvocation));
      }

      preventClose = Boolean(preventClose);
      preventAbort = Boolean(preventAbort);
      preventCancel = Boolean(preventCancel);

      if (IsReadableStreamLocked(this)) {
        return Promise_reject(new TypeError(errCannotPipeLockedStream));
      }

      if (binding.IsWritableStreamLocked(dest)) {
        return Promise_reject(new TypeError(errCannotPipeToALockedStream));
      }

      return ReadableStreamPipeTo(this, dest, preventClose, preventAbort,
                                  preventCancel);
    }

    tee() {
      if (IsReadableStream(this) === false) {
        throw new TypeError(streamErrors.illegalInvocation);
      }

      return ReadableStreamTee(this);
    }
  }

  function ReadableStreamPipeTo(readable, dest, preventClose, preventAbort,
                                preventCancel) {
    // Callers of this function must ensure that the following invariants
    // are enforced:
    // assert(IsReadableStream(readable));
    // assert(binding.IsWritableStream(dest));
    // assert(!IsReadableStreamLocked(readable));
    // assert(!binding.IsWritableStreamLocked(dest));

    const reader = AcquireReadableStreamDefaultReader(readable);
    const writer = binding.AcquireWritableStreamDefaultWriter(dest);
    let shuttingDown = false;
    const promise = v8.createPromise();
    let reading = false;

    if (checkInitialState()) {
      // Need to detect closing and error when we are not reading.
      thenPromise(reader[_closedPromise], onReaderClosed, readableError);
      // Need to detect error when we are not writing.
      thenPromise(
          binding.getWritableStreamDefaultWriterClosedPromise(writer),
          undefined, writableError);
      pump();
    }

    // Checks the state of the streams and executes the shutdown handlers if
    // necessary. Returns true if piping can continue.
    function checkInitialState() {
      const state = ReadableStreamGetState(readable);

      // Both streams can be errored or closed. To perform the right action the
      // order of the checks must match the standard.
      if (state === STATE_ERRORED) {
        readableError(readable[_storedError]);
        return false;
      }

      if (binding.isWritableStreamErrored(dest)) {
        writableError(binding.getWritableStreamStoredError(dest));
        return false;
      }

      if (state === STATE_CLOSED) {
        readableClosed();
        return false;
      }

      if (binding.isWritableStreamClosingOrClosed(dest)) {
        writableStartedClosed();
        return false;
      }

      return true;
    }

    function pump() {
      if (shuttingDown) {
        return;
      }
      const desiredSize =
          binding.WritableStreamDefaultWriterGetDesiredSize(writer);
      if (desiredSize === null) {
        // This can happen if abort() is queued but not yet started when
        // pipeTo() is called. In that case [[storedError]] is not set yet, and
        // we need to wait until it is before we can cancel the pipe. Once
        // [[storedError]] has been set, the rejection handler set on the writer
        // closed promise above will detect it, so all we need to do here is
        // nothing.
        return;
      }
      if (desiredSize <= 0) {
        thenPromise(
            binding.getWritableStreamDefaultWriterReadyPromise(writer), pump,
            writableError);
        return;
      }
      reading = true;
      // TODO(ricea): Delay reads heuristically when desiredSize is low.
      thenPromise(
          ReadableStreamDefaultReaderRead(reader), readFulfilled, readRejected);
    }

    function readFulfilled({value, done}) {
      reading = false;
      if (shuttingDown) {
        return;
      }
      if (done) {
        readableClosed();
        return;
      }
      const write = binding.WritableStreamDefaultWriterWrite(writer, value);
      thenPromise(write, undefined, writableError);
      pump();
    }

    function readRejected() {
      reading = false;
      readableError(readable[_storedError]);
    }

    // If read() is in progress, then wait for it to tell us that the stream is
    // closed so that we write all the data before shutdown.
    function onReaderClosed() {
      if (!reading) {
        readableClosed();
      }
    }

    // These steps are from "Errors must be propagated forward" in the
    // standard.
    function readableError(error) {
      if (!preventAbort) {
        shutdownWithAction(
            binding.WritableStreamAbort, [dest, error], error, true);
      } else {
        shutdown(error, true);
      }
    }

    // These steps are from "Errors must be propagated backward".
    function writableError(error) {
      if (!preventCancel) {
        shutdownWithAction(
            ReadableStreamCancel, [readable, error], error, true);
      } else {
        shutdown(error, true);
      }
    }

    // These steps are from "Closing must be propagated forward".
    function readableClosed() {
      if (!preventClose) {
        shutdownWithAction(
            binding.WritableStreamDefaultWriterCloseWithErrorPropagation,
            [writer]);
      } else {
        shutdown();
      }
    }

    // These steps are from "Closing must be propagated backward".
    function writableStartedClosed() {
      const destClosed = new TypeError(errDestinationStreamClosed);
      if (!preventCancel) {
        shutdownWithAction(
            ReadableStreamCancel, [readable, destClosed], destClosed, true);
      } else {
        shutdown(destClosed, true);
      }
    }

    function shutdownWithAction(
        action, args, originalError = undefined, errorGiven = false) {
      if (shuttingDown) {
        return;
      }
      shuttingDown = true;
      const p = applyFunction(action, undefined, args);
      thenPromise(
          p, () => finalize(originalError, errorGiven),
          newError => finalize(newError, true));
    }

    function shutdown(error = undefined, errorGiven = false) {
      if (shuttingDown) {
        return;
      }
      shuttingDown = true;
      finalize(error, errorGiven);
    }

    function finalize(error, errorGiven) {
      binding.WritableStreamDefaultWriterRelease(writer);
      ReadableStreamReaderGenericRelease(reader);
      if (errorGiven) {
        v8.rejectPromise(promise, error);
      } else {
        v8.resolvePromise(promise, undefined);
      }
    }

    return promise;
  }

  class ReadableStreamDefaultController {
    constructor(stream, underlyingSource, size, highWaterMark, isExternallyControlled) {
      if (IsReadableStream(stream) === false) {
        throw new TypeError(streamErrors.illegalConstructor);
      }

      if (stream[_controller] !== undefined) {
        throw new TypeError(streamErrors.illegalConstructor);
      }

      this[_controlledReadableStream] = stream;

      this[_underlyingSource] = underlyingSource;

      this[_queue] = new binding.SimpleQueue();
      this[_totalQueuedSize] = 0;

      this[_readableStreamDefaultControllerBits] = 0b0;
      if (isExternallyControlled === true) {
        this[_readableStreamDefaultControllerBits] |= EXTERNALLY_CONTROLLED;
      }

      const normalizedStrategy =
          ValidateAndNormalizeQueuingStrategy(size, highWaterMark);
      this[_strategySize] = normalizedStrategy.size;
      this[_strategyHWM] = normalizedStrategy.highWaterMark;

      const controller = this;

      const startResult = CallOrNoop(
          underlyingSource, 'start', this, 'underlyingSource.start');
      thenPromise(Promise_resolve(startResult),
          () => {
            controller[_readableStreamDefaultControllerBits] |= STARTED;
            ReadableStreamDefaultControllerCallPullIfNeeded(controller);
          },
          r => {
            if (ReadableStreamGetState(stream) === STATE_READABLE) {
              ReadableStreamDefaultControllerError(controller, r);
            }
          });
    }

    get desiredSize() {
      if (IsReadableStreamDefaultController(this) === false) {
        throw new TypeError(streamErrors.illegalInvocation);
      }

      return ReadableStreamDefaultControllerGetDesiredSize(this);
    }

    close() {
      if (IsReadableStreamDefaultController(this) === false) {
        throw new TypeError(streamErrors.illegalInvocation);
      }

      const stream = this[_controlledReadableStream];

      if (this[_readableStreamDefaultControllerBits] & CLOSE_REQUESTED) {
        throw new TypeError(errCloseCloseRequestedStream);
      }

      const state = ReadableStreamGetState(stream);
      if (state === STATE_ERRORED) {
        throw new TypeError(errCloseErroredStream);
      }
      if (state === STATE_CLOSED) {
        throw new TypeError(errCloseClosedStream);
      }

      return ReadableStreamDefaultControllerClose(this);
    }

    enqueue(chunk) {
      if (IsReadableStreamDefaultController(this) === false) {
        throw new TypeError(streamErrors.illegalInvocation);
      }

      const stream = this[_controlledReadableStream];

      if (this[_readableStreamDefaultControllerBits] & CLOSE_REQUESTED) {
        throw new TypeError(errEnqueueCloseRequestedStream);
      }

      const state = ReadableStreamGetState(stream);
      if (state === STATE_ERRORED) {
        throw new TypeError(errEnqueueErroredStream);
      }
      if (state === STATE_CLOSED) {
        throw new TypeError(errEnqueueClosedStream);
      }

      return ReadableStreamDefaultControllerEnqueue(this, chunk);
    }

    error(e) {
      if (IsReadableStreamDefaultController(this) === false) {
        throw new TypeError(streamErrors.illegalInvocation);
      }

      const stream = this[_controlledReadableStream];

      const state = ReadableStreamGetState(stream);
      if (state === STATE_ERRORED) {
        throw new TypeError(errErrorErroredStream);
      }
      if (state === STATE_CLOSED) {
        throw new TypeError(errErrorClosedStream);
      }

      return ReadableStreamDefaultControllerError(this, e);
    }
  }

  function ReadableStreamDefaultControllerCancel(controller, reason) {
    controller[_queue] = new binding.SimpleQueue();

    const underlyingSource = controller[_underlyingSource];
    return PromiseCallOrNoop(underlyingSource, 'cancel', reason, 'underlyingSource.cancel');
  }

  function ReadableStreamDefaultControllerPull(controller) {
    const stream = controller[_controlledReadableStream];

    if (controller[_queue].length > 0) {
      const chunk = DequeueValue(controller);

      if ((controller[_readableStreamDefaultControllerBits] & CLOSE_REQUESTED) &&
          controller[_queue].length === 0) {
        ReadableStreamClose(stream);
      } else {
        ReadableStreamDefaultControllerCallPullIfNeeded(controller);
      }

      return Promise_resolve(CreateIterResultObject(chunk, false));
    }

    const pendingPromise = ReadableStreamAddReadRequest(stream);
    ReadableStreamDefaultControllerCallPullIfNeeded(controller);
    return pendingPromise;
  }

  function ReadableStreamAddReadRequest(stream) {
    const promise = v8.createPromise();
    stream[_reader][_readRequests].push(promise);
    return promise;
  }

  class ReadableStreamDefaultReader {
    constructor(stream) {
      if (IsReadableStream(stream) === false) {
        throw new TypeError(errReaderConstructorBadArgument);
      }
      if (IsReadableStreamLocked(stream) === true) {
        throw new TypeError(errReaderConstructorStreamAlreadyLocked);
      }

      ReadableStreamReaderGenericInitialize(this, stream);

      this[_readRequests] = new binding.SimpleQueue();
    }

    get closed() {
      if (IsReadableStreamDefaultReader(this) === false) {
        return Promise_reject(new TypeError(streamErrors.illegalInvocation));
      }

      return this[_closedPromise];
    }

    cancel(reason) {
      if (IsReadableStreamDefaultReader(this) === false) {
        return Promise_reject(new TypeError(streamErrors.illegalInvocation));
      }

      const stream = this[_ownerReadableStream];
      if (stream === undefined) {
        return Promise_reject(new TypeError(errCancelReleasedReader));
      }

      return ReadableStreamReaderGenericCancel(this, reason);
    }

    read() {
      if (IsReadableStreamDefaultReader(this) === false) {
        return Promise_reject(new TypeError(streamErrors.illegalInvocation));
      }

      if (this[_ownerReadableStream] === undefined) {
        return Promise_reject(new TypeError(errReadReleasedReader));
      }

      return ReadableStreamDefaultReaderRead(this);
    }

    releaseLock() {
      if (IsReadableStreamDefaultReader(this) === false) {
        throw new TypeError(streamErrors.illegalInvocation);
      }

      const stream = this[_ownerReadableStream];
      if (stream === undefined) {
        return undefined;
      }

      if (this[_readRequests].length > 0) {
        throw new TypeError(errReleaseReaderWithPendingRead);
      }

      ReadableStreamReaderGenericRelease(this);
    }
  }

  function ReadableStreamReaderGenericCancel(reader, reason) {
    return ReadableStreamCancel(reader[_ownerReadableStream], reason);
  }

  //
  // Readable stream abstract operations
  //

  function AcquireReadableStreamDefaultReader(stream) {
    return new ReadableStreamDefaultReader(stream);
  }

  function ReadableStreamCancel(stream, reason) {
    stream[_readableStreamBits] |= DISTURBED;

    const state = ReadableStreamGetState(stream);
    if (state === STATE_CLOSED) {
      return Promise_resolve(undefined);
    }
    if (state === STATE_ERRORED) {
      return Promise_reject(stream[_storedError]);
    }

    ReadableStreamClose(stream);

    const sourceCancelPromise = ReadableStreamDefaultControllerCancel(stream[_controller], reason);
    return thenPromise(sourceCancelPromise, () => undefined);
  }

  function ReadableStreamDefaultControllerClose(controller) {
    const stream = controller[_controlledReadableStream];

    controller[_readableStreamDefaultControllerBits] |= CLOSE_REQUESTED;

    if (controller[_queue].length === 0) {
      ReadableStreamClose(stream);
    }
  }

  function ReadableStreamFulfillReadRequest(stream, chunk, done) {
    const reader = stream[_reader];

    const readRequest = stream[_reader][_readRequests].shift();
    v8.resolvePromise(readRequest, CreateIterResultObject(chunk, done));
  }

  function ReadableStreamDefaultControllerEnqueue(controller, chunk) {
    const stream = controller[_controlledReadableStream];

    if (IsReadableStreamLocked(stream) === true && ReadableStreamGetNumReadRequests(stream) > 0) {
      ReadableStreamFulfillReadRequest(stream, chunk, false);
    } else {
      let chunkSize = 1;

      const strategySize = controller[_strategySize];
      if (strategySize !== undefined) {
        try {
          chunkSize = strategySize(chunk);
        } catch (chunkSizeE) {
          if (ReadableStreamGetState(stream) === STATE_READABLE) {
            ReadableStreamDefaultControllerError(controller, chunkSizeE);
          }
          throw chunkSizeE;
        }
      }

      try {
        EnqueueValueWithSize(controller, chunk, chunkSize);
      } catch (enqueueE) {
        if (ReadableStreamGetState(stream) === STATE_READABLE) {
          ReadableStreamDefaultControllerError(controller, enqueueE);
        }
        throw enqueueE;
      }
    }

    ReadableStreamDefaultControllerCallPullIfNeeded(controller);
  }

  function ReadableStreamGetState(stream) {
    return (stream[_readableStreamBits] & STATE_MASK) >> STATE_BITS_OFFSET;
  }

  function ReadableStreamSetState(stream, state) {
    stream[_readableStreamBits] = (stream[_readableStreamBits] & ~STATE_MASK) |
        (state << STATE_BITS_OFFSET);
  }

  function ReadableStreamDefaultControllerError(controller, e) {
    controller[_queue] = new binding.SimpleQueue();
    const stream = controller[_controlledReadableStream];
    ReadableStreamError(stream, e);
  }

  function ReadableStreamError(stream, e) {
    stream[_storedError] = e;
    ReadableStreamSetState(stream, STATE_ERRORED);

    const reader = stream[_reader];
    if (reader === undefined) {
      return undefined;
    }

    if (IsReadableStreamDefaultReader(reader) === true) {
      reader[_readRequests].forEach(request => v8.rejectPromise(request, e));
      reader[_readRequests] = new binding.SimpleQueue();
    }

    v8.rejectPromise(reader[_closedPromise], e);
    v8.markPromiseAsHandled(reader[_closedPromise]);
  }

  function ReadableStreamClose(stream) {
    ReadableStreamSetState(stream, STATE_CLOSED);

    const reader = stream[_reader];
    if (reader === undefined) {
      return undefined;
    }

    if (IsReadableStreamDefaultReader(reader) === true) {
      reader[_readRequests].forEach(request =>
          v8.resolvePromise(request, CreateIterResultObject(undefined, true)));
      reader[_readRequests] = new binding.SimpleQueue();
    }

    v8.resolvePromise(reader[_closedPromise], undefined);
  }

  function ReadableStreamDefaultControllerGetDesiredSize(controller) {
    const queueSize = GetTotalQueueSize(controller);
    return controller[_strategyHWM] - queueSize;
  }

  function IsReadableStream(x) {
    return hasOwnProperty(x, _controller);
  }

  function IsReadableStreamDisturbed(stream) {
    return stream[_readableStreamBits] & DISTURBED;
  }

  function IsReadableStreamLocked(stream) {
    return stream[_reader] !== undefined;
  }

  function IsReadableStreamDefaultController(x) {
    return hasOwnProperty(x, _controlledReadableStream);
  }

  function IsReadableStreamDefaultReader(x) {
    return hasOwnProperty(x, _readRequests);
  }

  function IsReadableStreamReadable(stream) {
    return ReadableStreamGetState(stream) === STATE_READABLE;
  }

  function IsReadableStreamClosed(stream) {
    return ReadableStreamGetState(stream) === STATE_CLOSED;
  }

  function IsReadableStreamErrored(stream) {
    return ReadableStreamGetState(stream) === STATE_ERRORED;
  }

  function ReadableStreamReaderGenericInitialize(reader, stream) {
    // TODO(yhirano): Remove this when we don't need hasPendingActivity in
    // blink::UnderlyingSourceBase.
    const controller = stream[_controller];
    if (controller[_readableStreamDefaultControllerBits] & EXTERNALLY_CONTROLLED) {
      // The stream is created with an external controller (i.e. made in
      // Blink).
      const underlyingSource = controller[_underlyingSource];
      callFunction(underlyingSource.notifyLockAcquired, underlyingSource);
    }

    reader[_ownerReadableStream] = stream;
    stream[_reader] = reader;

    switch (ReadableStreamGetState(stream)) {
      case STATE_READABLE:
        reader[_closedPromise] = v8.createPromise();
        break;
      case STATE_CLOSED:
        reader[_closedPromise] = Promise_resolve(undefined);
        break;
      case STATE_ERRORED:
        reader[_closedPromise] = Promise_reject(stream[_storedError]);
        v8.markPromiseAsHandled(reader[_closedPromise]);
        break;
    }
  }

  function ReadableStreamReaderGenericRelease(reader) {
    // TODO(yhirano): Remove this when we don't need hasPendingActivity in
    // blink::UnderlyingSourceBase.
    const controller = reader[_ownerReadableStream][_controller];
    if (controller[_readableStreamDefaultControllerBits] & EXTERNALLY_CONTROLLED) {
      // The stream is created with an external controller (i.e. made in
      // Blink).
      const underlyingSource = controller[_underlyingSource];
      callFunction(underlyingSource.notifyLockReleased, underlyingSource);
    }

    if (ReadableStreamGetState(reader[_ownerReadableStream]) === STATE_READABLE) {
      v8.rejectPromise(reader[_closedPromise], new TypeError(errReleasedReaderClosedPromise));
    } else {
      reader[_closedPromise] = Promise_reject(new TypeError(errReleasedReaderClosedPromise));
    }
    v8.markPromiseAsHandled(reader[_closedPromise]);

    reader[_ownerReadableStream][_reader] = undefined;
    reader[_ownerReadableStream] = undefined;
  }

  function ReadableStreamDefaultReaderRead(reader) {
    const stream = reader[_ownerReadableStream];
    stream[_readableStreamBits] |= DISTURBED;

    if (ReadableStreamGetState(stream) === STATE_CLOSED) {
      return Promise_resolve(CreateIterResultObject(undefined, true));
    }

    if (ReadableStreamGetState(stream) === STATE_ERRORED) {
      return Promise_reject(stream[_storedError]);
    }

    return ReadableStreamDefaultControllerPull(stream[_controller]);
  }

  function ReadableStreamDefaultControllerCallPullIfNeeded(controller) {
    const shouldPull = ReadableStreamDefaultControllerShouldCallPull(controller);
    if (shouldPull === false) {
      return undefined;
    }

    if (controller[_readableStreamDefaultControllerBits] & PULLING) {
      controller[_readableStreamDefaultControllerBits] |= PULL_AGAIN;
      return undefined;
    }

    controller[_readableStreamDefaultControllerBits] |= PULLING;

    const underlyingSource = controller[_underlyingSource];
    const pullPromise = PromiseCallOrNoop(
        underlyingSource, 'pull', controller, 'underlyingSource.pull');

    thenPromise(pullPromise,
        () => {
          controller[_readableStreamDefaultControllerBits] &= ~PULLING;

          if (controller[_readableStreamDefaultControllerBits] & PULL_AGAIN) {
            controller[_readableStreamDefaultControllerBits] &= ~PULL_AGAIN;
            ReadableStreamDefaultControllerCallPullIfNeeded(controller);
          }
        },
        e => {
          if (ReadableStreamGetState(controller[_controlledReadableStream]) === STATE_READABLE) {
            ReadableStreamDefaultControllerError(controller, e);
          }
        });
  }

  function ReadableStreamDefaultControllerShouldCallPull(controller) {
    const stream = controller[_controlledReadableStream];

    const state = ReadableStreamGetState(stream);
    if (state === STATE_CLOSED || state === STATE_ERRORED) {
      return false;
    }

    if (controller[_readableStreamDefaultControllerBits] & CLOSE_REQUESTED) {
      return false;
    }

    if (!(controller[_readableStreamDefaultControllerBits] & STARTED)) {
      return false;
    }

    if (IsReadableStreamLocked(stream) === true && ReadableStreamGetNumReadRequests(stream) > 0) {
      return true;
    }

    const desiredSize = ReadableStreamDefaultControllerGetDesiredSize(controller);
    if (desiredSize > 0) {
      return true;
    }

    return false;
  }

  function ReadableStreamGetNumReadRequests(stream) {
    const reader = stream[_reader];
    const readRequests = reader[_readRequests];
    return readRequests.length;
  }

  // Potential future optimization: use class instances for the underlying
  // sources, so that we don't re-create
  // closures every time.

  // TODO(domenic): shouldClone argument from spec not supported yet
  function ReadableStreamTee(stream) {
    const reader = AcquireReadableStreamDefaultReader(stream);

    let closedOrErrored = false;
    let canceled1 = false;
    let canceled2 = false;
    let reason1;
    let reason2;
    let promise = v8.createPromise();

    const branch1Stream = new ReadableStream({pull, cancel: cancel1});

    const branch2Stream = new ReadableStream({pull, cancel: cancel2});

    const branch1 = branch1Stream[_controller];
    const branch2 = branch2Stream[_controller];

    thenPromise(
        reader[_closedPromise], undefined, function(r) {
          if (closedOrErrored === true) {
            return;
          }

          ReadableStreamDefaultControllerError(branch1, r);
          ReadableStreamDefaultControllerError(branch2, r);
          closedOrErrored = true;
        });

    return [branch1Stream, branch2Stream];

    function pull() {
      return thenPromise(
          ReadableStreamDefaultReaderRead(reader), function(result) {
            const value = result.value;
            const done = result.done;

            if (done === true && closedOrErrored === false) {
              if (canceled1 === false) {
                ReadableStreamDefaultControllerClose(branch1);
              }
              if (canceled2 === false) {
                ReadableStreamDefaultControllerClose(branch2);
              }
              closedOrErrored = true;
            }

            if (closedOrErrored === true) {
              return;
            }

            if (canceled1 === false) {
              ReadableStreamDefaultControllerEnqueue(branch1, value);
            }

            if (canceled2 === false) {
              ReadableStreamDefaultControllerEnqueue(branch2, value);
            }
          });
    }

    function cancel1(reason) {
      canceled1 = true;
      reason1 = reason;

      if (canceled2 === true) {
        const compositeReason = [reason1, reason2];
        const cancelResult = ReadableStreamCancel(stream, compositeReason);
        v8.resolvePromise(promise, cancelResult);
      }

      return promise;
    }

    function cancel2(reason) {
      canceled2 = true;
      reason2 = reason;

      if (canceled1 === true) {
        const compositeReason = [reason1, reason2];
        const cancelResult = ReadableStreamCancel(stream, compositeReason);
        v8.resolvePromise(promise, cancelResult);
      }

      return promise;
    }
  }

  //
  // Queue-with-sizes
  //

  function DequeueValue(controller) {
    const result = controller[_queue].shift();
    controller[_totalQueuedSize] -= result.size;
    return result.value;
  }

  function EnqueueValueWithSize(controller, value, size) {
    size = Number(size);
    if (Number_isNaN(size) || size === +Infinity || size < 0) {
      throw new RangeError(streamErrors.invalidSize);
    }

    controller[_totalQueuedSize] += size;
    controller[_queue].push({value, size});
  }

  function GetTotalQueueSize(controller) { return controller[_totalQueuedSize]; }

  //
  // Other helpers
  //

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
      return Promise_reject(new TypeError(errTmplMustBeFunctionOrUndefined(nameForError)));
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

  binding.AcquireReadableStreamDefaultReader = AcquireReadableStreamDefaultReader;
  binding.IsReadableStream = IsReadableStream;
  binding.IsReadableStreamDisturbed = IsReadableStreamDisturbed;
  binding.IsReadableStreamLocked = IsReadableStreamLocked;
  binding.IsReadableStreamReadable = IsReadableStreamReadable;
  binding.IsReadableStreamClosed = IsReadableStreamClosed;
  binding.IsReadableStreamErrored = IsReadableStreamErrored;
  binding.IsReadableStreamDefaultReader = IsReadableStreamDefaultReader;
  binding.ReadableStreamDefaultReaderRead = ReadableStreamDefaultReaderRead;
  binding.ReadableStreamTee = ReadableStreamTee;

  binding.ReadableStreamDefaultControllerClose = ReadableStreamDefaultControllerClose;
  binding.ReadableStreamDefaultControllerGetDesiredSize = ReadableStreamDefaultControllerGetDesiredSize;
  binding.ReadableStreamDefaultControllerEnqueue = ReadableStreamDefaultControllerEnqueue;
  binding.ReadableStreamDefaultControllerError = ReadableStreamDefaultControllerError;

  binding.createReadableStreamWithExternalController =
      (underlyingSource, strategy) => {
        return new ReadableStream(
            underlyingSource, strategy, createWithExternalControllerSentinel);
      };
});
