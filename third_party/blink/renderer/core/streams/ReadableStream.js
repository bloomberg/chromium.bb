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

  const _readableStreamBits =
        v8.createPrivateSymbol('bit field for [[state]] and [[disturbed]]');
  const DISTURBED = 0b1;
  // The 2nd and 3rd bit are for [[state]].
  const STATE_MASK = 0b110;
  const STATE_BITS_OFFSET = 1;
  const STATE_READABLE = 0;
  const STATE_CLOSED = 1;
  const STATE_ERRORED = 2;

  const _controlledReadableStream =
        v8.createPrivateSymbol('[[controlledReadableStream]]');
  const _strategyHWM = v8.createPrivateSymbol('[[strategyHWM]]');

  const _readableStreamDefaultControllerBits = v8.createPrivateSymbol(
      'bit field for [[started]], [[closeRequested]], [[pulling]], ' +
        '[[pullAgain]]');
  const internalReadableStreamSymbol = v8.createPrivateSymbol(
      'internal ReadableStream in exposed ReadableStream interface');
  // Remove this once C++ code has been updated to use CreateReadableStream.
  const _strategySizeAlgorithm = v8.createPrivateSymbol(
      '[[strategySizeAlgorithm]]');
  const _pullAlgorithm = v8.createPrivateSymbol('[[pullAlgorithm]]');
  const _cancelAlgorithm = v8.createPrivateSymbol('[[cancelAlgorithm]]');
  const STARTED = 0b1;
  const CLOSE_REQUESTED = 0b10;
  const PULLING = 0b100;
  const PULL_AGAIN = 0b1000;

  const ObjectCreate = global.Object.create;

  const callFunction = v8.uncurryThis(global.Function.prototype.call);
  const applyFunction = v8.uncurryThis(global.Function.prototype.apply);

  const TypeError = global.TypeError;
  const RangeError = global.RangeError;

  const String = global.String;

  const Promise = global.Promise;
  const thenPromise = v8.uncurryThis(Promise.prototype.then);

  // From CommonOperations.js
  const {
    _queue,
    _queueTotalSize,
    createPromise,
    createRejectedPromise,
    createResolvedPromise,
    hasOwnPropertyNoThrow,
    rejectPromise,
    resolvePromise,
    markPromiseAsHandled,
    CallOrNoop1,
    CreateAlgorithmFromUnderlyingMethod,
    CreateAlgorithmFromUnderlyingMethodPassingController,
    CreateCrossRealmTransformReadable,
    CreateCrossRealmTransformWritable,
    DequeueValue,
    EnqueueValueWithSize,
    MakeSizeAlgorithmFromSizeFunction,
    ValidateAndNormalizeHighWaterMark,
  } = binding.streamOperations;

  const streamErrors = binding.streamErrors;
  const errEnqueueCloseRequestedStream =
        'Cannot enqueue a chunk into a readable stream that is closed or ' +
        'has been requested to be closed';
  const errCancelReleasedReader =
        'This readable stream reader has been released and cannot be used ' +
        'to cancel its previous owner stream';
  const errReadReleasedReader =
        'This readable stream reader has been released and cannot be used ' +
        'to read from its previous owner stream';
  const errCloseCloseRequestedStream =
        'Cannot close a readable stream that has already been requested to ' +
        'be closed';
  const errEnqueueClosedStream =
        'Cannot enqueue a chunk into a closed readable stream';
  const errEnqueueErroredStream =
        'Cannot enqueue a chunk into an errored readable stream';
  const errCloseClosedStream = 'Cannot close a closed readable stream';
  const errCloseErroredStream = 'Cannot close an errored readable stream';
  const errReaderConstructorBadArgument =
        'ReadableStreamReader constructor argument is not a readable stream';
  const errReaderConstructorStreamAlreadyLocked =
        'ReadableStreamReader constructor can only accept readable streams ' +
        'that are not yet locked to a reader';
  const errReleaseReaderWithPendingRead =
        'Cannot release a readable stream reader when it still has ' +
        'outstanding read() calls that have not yet settled';
  const errReleasedReaderClosedPromise =
        'This readable stream reader has been released and cannot be used ' +
        'to monitor the stream\'s state';

  const errDestinationStreamClosed = 'Destination stream closed';

  let useCounted = false;

  class ReadableStream {
    // TODO(ricea): Remove |internalArgument| once
    // blink::ReadableStreamOperations has been updated to use
    // CreateReadableStream.
    constructor(underlyingSource = {}, strategy = {},
                internalArgument = undefined) {
      const createdByUA =
            internalArgument === createWithExternalControllerSentinel;

      if (!useCounted && !createdByUA) {
        binding.countUse('ReadableStreamConstructor');
        useCounted = true;
      }

      InitializeReadableStream(this);
      const size = strategy.size;
      let highWaterMark = strategy.highWaterMark;
      const type = underlyingSource.type;
      const typeString = String(type);

      if (typeString === 'bytes') {
        throw new RangeError('bytes type is not yet implemented');
      }

      if (type !== undefined) {
        throw new RangeError(streamErrors.invalidType);
      }

      const sizeAlgorithm = MakeSizeAlgorithmFromSizeFunction(size);

      if (highWaterMark === undefined) {
        highWaterMark = 1;
      }

      highWaterMark = ValidateAndNormalizeHighWaterMark(highWaterMark);
      SetUpReadableStreamDefaultControllerFromUnderlyingSource(
          this, underlyingSource, highWaterMark, sizeAlgorithm);
    }
  }

  const ReadableStream_prototype = ReadableStream.prototype;

  function ReadableStreamPipeTo(
      readable, dest, preventClose, preventAbort, preventCancel) {
    // Callers of this function must ensure that the following invariants
    // are enforced:
    // assert(IsReadableStream(readable));
    // assert(binding.IsWritableStream(dest));
    // assert(!IsReadableStreamLocked(readable));
    // assert(!binding.IsWritableStreamLocked(dest));

    const reader = AcquireReadableStreamDefaultReader(readable);
    const writer = binding.AcquireWritableStreamDefaultWriter(dest);
    let shuttingDown = false;
    const promise = createPromise();
    let reading = false;
    let lastWrite;

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
      thenPromise(
          ReadableStreamDefaultReaderRead(reader), readFulfilled, readRejected);
    }

    function readFulfilled({value, done}) {
      reading = false;
      if (done) {
        readableClosed();
        return;
      }
      const write = binding.WritableStreamDefaultWriterWrite(writer, value);
      lastWrite = write;
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
      let p;
      if (shouldWriteQueuedChunks()) {
        p = thenPromise(writeQueuedChunks(),
                        () => applyFunction(action, undefined, args));
      } else {
        p = applyFunction(action, undefined, args);
      }
      thenPromise(
          p, () => finalize(originalError, errorGiven),
          newError => finalize(newError, true));
    }

    function shutdown(error = undefined, errorGiven = false) {
      if (shuttingDown) {
        return;
      }
      shuttingDown = true;
      if (shouldWriteQueuedChunks()) {
        thenPromise(writeQueuedChunks(), () => finalize(error, errorGiven));
      } else {
        finalize(error, errorGiven);
      }
    }

    function finalize(error, errorGiven) {
      binding.WritableStreamDefaultWriterRelease(writer);
      ReadableStreamReaderGenericRelease(reader);
      if (errorGiven) {
        rejectPromise(promise, error);
      } else {
        resolvePromise(promise, undefined);
      }
    }

    function shouldWriteQueuedChunks() {
      return binding.isWritableStreamWritable(dest) &&
          !binding.WritableStreamCloseQueuedOrInFlight(dest);
    }

    function writeQueuedChunks() {
      if (lastWrite) {
        // "Wait until every chunk that has been read has been written (i.e.
        // the corresponding promises have settled)"
        // This implies that we behave the same whether the promise fulfills or
        // rejects.
        return thenPromise(lastWrite, () => undefined, () => undefined);
      }
      return createResolvedPromise(undefined);
    }

    return promise;
  }

  //
  // Readable stream abstract operations
  //

  function AcquireReadableStreamDefaultReader(stream) {
    return new ReadableStreamDefaultReader(stream);
  }

  function CreateReadableStream(startAlgorithm, pullAlgorithm, cancelAlgorithm,
                                highWaterMark, sizeAlgorithm) {
    if (highWaterMark === undefined) {
      highWaterMark = 1;
    }
    if (sizeAlgorithm === undefined) {
      sizeAlgorithm = () => 1;
    }
    // assert(IsNonNegativeNumber(highWaterMark),
    //        '! IsNonNegativeNumber(highWaterMark) is true.');
    const stream = ObjectCreate(ReadableStream_prototype);
    InitializeReadableStream(stream);
    const controller = ObjectCreate(ReadableStreamDefaultController_prototype);
    SetUpReadableStreamDefaultController(
        stream, controller, startAlgorithm, pullAlgorithm, cancelAlgorithm,
        highWaterMark, sizeAlgorithm);
    return stream;
  }

  function InitializeReadableStream(stream) {
    stream[_readableStreamBits] = 0b0;
    ReadableStreamSetState(stream, STATE_READABLE);
    stream[_reader] = undefined;
    stream[_storedError] = undefined;
  }

  function IsReadableStream(x) {
    return hasOwnPropertyNoThrow(x, _controller);
  }

  function IsReadableStreamDisturbed(stream) {
    return stream[_readableStreamBits] & DISTURBED;
  }

  function IsReadableStreamLocked(stream) {
    return stream[_reader] !== undefined;
  }

  // TODO(domenic): cloneForBranch2 argument from spec not supported yet
  function ReadableStreamTee(stream) {
    const reader = AcquireReadableStreamDefaultReader(stream);

    let closedOrErrored = false;
    let canceled1 = false;
    let canceled2 = false;
    let reason1;
    let reason2;
    const cancelPromise = createPromise();

    function pullAlgorithm() {
      return thenPromise(
          ReadableStreamDefaultReaderRead(reader), ({value, done}) => {
            if (done && !closedOrErrored) {
              if (!canceled1) {
                ReadableStreamDefaultControllerClose(branch1controller);
              }
              if (!canceled2) {
                ReadableStreamDefaultControllerClose(branch2controller);
              }
              closedOrErrored = true;
            }

            if (closedOrErrored) {
              return;
            }

            // TODO(ricea): Implement these steps for cloning.
            //
            // vii. Let _value1_ and _value2_ be _value_.
            // viii. If _canceled2_ is false and _cloneForBranch2_ is true, set
            // value2 to ? StructuredDeserialize(? StructuredSerialize(value2),
            // the current Realm Record).

            if (!canceled1) {
              ReadableStreamDefaultControllerEnqueue(branch1controller, value);
            }

            if (!canceled2) {
              ReadableStreamDefaultControllerEnqueue(branch2controller, value);
            }
          });
    }

    function cancel1Algorithm(reason) {
      canceled1 = true;
      reason1 = reason;
      if (canceled2) {
        const cancelResult = ReadableStreamCancel(stream, [reason1, reason2]);
        resolvePromise(cancelPromise, cancelResult);
      }
      return cancelPromise;
    }

    function cancel2Algorithm(reason) {
      canceled2 = true;
      reason2 = reason;
      if (canceled1) {
        const cancelResult = ReadableStreamCancel(stream, [reason1, reason2]);
        resolvePromise(cancelPromise, cancelResult);
      }
      return cancelPromise;
    }

    const startAlgorithm = () => undefined;

    const branch1Stream = CreateReadableStream(
        startAlgorithm, pullAlgorithm, cancel1Algorithm);
    const branch2Stream = CreateReadableStream(
        startAlgorithm, pullAlgorithm, cancel2Algorithm);
    const branch1controller = branch1Stream[_controller];
    const branch2controller = branch2Stream[_controller];

    thenPromise(reader[_closedPromise], undefined, r => {
      if (closedOrErrored === true) {
        return;
      }

      ReadableStreamDefaultControllerError(branch1controller, r);
      ReadableStreamDefaultControllerError(branch2controller, r);
      closedOrErrored = true;
    });

    return [branch1Stream, branch2Stream];
  }

  //
  // Abstract Operations Used By Controllers
  //

  function ReadableStreamAddReadRequest(stream, forAuthorCode) {
    const promise = createPromise();
    stream[_reader][_readRequests].push({promise, forAuthorCode});
    return promise;
  }

  function ReadableStreamCancel(stream, reason) {
    stream[_readableStreamBits] |= DISTURBED;

    const state = ReadableStreamGetState(stream);
    if (state === STATE_CLOSED) {
      return createResolvedPromise(undefined);
    }
    if (state === STATE_ERRORED) {
      return createRejectedPromise(stream[_storedError]);
    }

    ReadableStreamClose(stream);

    const sourceCancelPromise =
          ReadableStreamDefaultControllerCancel(stream[_controller], reason);
    return thenPromise(sourceCancelPromise, () => undefined);
  }

  function ReadableStreamClose(stream) {
    ReadableStreamSetState(stream, STATE_CLOSED);

    const reader = stream[_reader];
    if (reader === undefined) {
      return;
    }

    if (IsReadableStreamDefaultReader(reader) === true) {
      reader[_readRequests].forEach(
          request =>
            resolvePromise(
                request.promise,
                ReadableStreamCreateReadResult(undefined, true,
                                               request.forAuthorCode)));
      reader[_readRequests] = new binding.SimpleQueue();
    }

    resolvePromise(reader[_closedPromise], undefined);
  }

  function ReadableStreamCreateReadResult(value, done, forAuthorCode) {
    // assert(typeof done === 'boolean', 'Type(_done_) is Boolean.');
    if (forAuthorCode) {
      return {value, done};
    }
    const obj = ObjectCreate(null);
    obj.value = value;
    obj.done = done;
    return obj;
  }

  function ReadableStreamError(stream, e) {
    ReadableStreamSetState(stream, STATE_ERRORED);
    stream[_storedError] = e;

    const reader = stream[_reader];
    if (reader === undefined) {
      return;
    }

    if (IsReadableStreamDefaultReader(reader) === true) {
      reader[_readRequests].forEach(request =>
                                    rejectPromise(request.promise, e));
      reader[_readRequests] = new binding.SimpleQueue();
    }

    rejectPromise(reader[_closedPromise], e);
    markPromiseAsHandled(reader[_closedPromise]);
  }

  function ReadableStreamFulfillReadRequest(stream, chunk, done) {
    const readRequest = stream[_reader][_readRequests].shift();
    resolvePromise(readRequest.promise,
                   ReadableStreamCreateReadResult(chunk, done,
                                                  readRequest.forAuthorCode));
  }

  function ReadableStreamGetNumReadRequests(stream) {
    const reader = stream[_reader];
    const readRequests = reader[_readRequests];
    return readRequests.length;
  }

  //
  // Class ReadableStreamDefaultReader
  //

  class ReadableStreamDefaultReader {
    constructor(stream) {
      // |stream| here can be either an external ReadableStream (i.e.,
      // IDL defined ReadableStream) or an internal ReadableStream (i.e.,
      // the class defined in this file). In the former case, the
      // internal stream is stored in [internalReadableStreamSymbol], so use it
      // from now on.
      if (stream[internalReadableStreamSymbol] !== undefined) {
        stream = stream[internalReadableStreamSymbol];
      }

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
        return createRejectedPromise(
            new TypeError(streamErrors.illegalInvocation));
      }

      return this[_closedPromise];
    }

    cancel(reason) {
      if (IsReadableStreamDefaultReader(this) === false) {
        return createRejectedPromise(
            new TypeError(streamErrors.illegalInvocation));
      }

      if (this[_ownerReadableStream] === undefined) {
        return createRejectedPromise(new TypeError(errCancelReleasedReader));
      }

      return ReadableStreamReaderGenericCancel(this, reason);
    }

    read() {
      if (IsReadableStreamDefaultReader(this) === false) {
        return createRejectedPromise(
            new TypeError(streamErrors.illegalInvocation));
      }

      if (this[_ownerReadableStream] === undefined) {
        return createRejectedPromise(new TypeError(errReadReleasedReader));
      }

      return ReadableStreamDefaultReaderRead(this, true);
    }

    releaseLock() {
      if (IsReadableStreamDefaultReader(this) === false) {
        throw new TypeError(streamErrors.illegalInvocation);
      }

      if (this[_ownerReadableStream] === undefined) {
        return;
      }

      if (this[_readRequests].length > 0) {
        throw new TypeError(errReleaseReaderWithPendingRead);
      }

      ReadableStreamReaderGenericRelease(this);
    }
  }

  //
  //  Readable Stream Reader Abstract Operations
  //

  function IsReadableStreamDefaultReader(x) {
    return hasOwnPropertyNoThrow(x, _readRequests);
  }

  function ReadableStreamReaderGenericCancel(reader, reason) {
    return ReadableStreamCancel(reader[_ownerReadableStream], reason);
  }

  function ReadableStreamReaderGenericInitialize(reader, stream) {
    reader[_ownerReadableStream] = stream;
    stream[_reader] = reader;

    switch (ReadableStreamGetState(stream)) {
      case STATE_READABLE:
        reader[_closedPromise] = createPromise();
        break;
      case STATE_CLOSED:
        reader[_closedPromise] = createResolvedPromise(undefined);
        break;
      case STATE_ERRORED:
        reader[_closedPromise] = createRejectedPromise(stream[_storedError]);
        markPromiseAsHandled(reader[_closedPromise]);
        break;
    }
  }

  function ReadableStreamReaderGenericRelease(reader) {
    if (ReadableStreamGetState(reader[_ownerReadableStream]) ===
        STATE_READABLE) {
      rejectPromise(
          reader[_closedPromise],
          new TypeError(errReleasedReaderClosedPromise));
    } else {
      reader[_closedPromise] =
          createRejectedPromise(new TypeError(errReleasedReaderClosedPromise));
    }
    markPromiseAsHandled(reader[_closedPromise]);

    reader[_ownerReadableStream][_reader] = undefined;
    reader[_ownerReadableStream] = undefined;
  }

  function ReadableStreamDefaultReaderRead(reader, forAuthorCode = false) {
    const stream = reader[_ownerReadableStream];
    stream[_readableStreamBits] |= DISTURBED;

    switch (ReadableStreamGetState(stream)) {
      case STATE_CLOSED:
        return createResolvedPromise(
            ReadableStreamCreateReadResult(undefined, true, forAuthorCode));

      case STATE_ERRORED:
        return createRejectedPromise(stream[_storedError]);

      default:
        return ReadableStreamDefaultControllerPull(stream[_controller],
                                                   forAuthorCode);
    }
  }

  //
  // Class ReadableStreamDefaultController
  //

  class ReadableStreamDefaultController {
    constructor() {
      throw new TypeError(streamErrors.illegalConstructor);
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

      if (ReadableStreamDefaultControllerCanCloseOrEnqueue(this) === false) {
        let errorDescription;
        if (this[_readableStreamDefaultControllerBits] & CLOSE_REQUESTED) {
          errorDescription = errCloseCloseRequestedStream;
        } else {
          const stream = this[_controlledReadableStream];
          switch (ReadableStreamGetState(stream)) {
            case STATE_ERRORED:
              errorDescription = errCloseErroredStream;
              break;

            case STATE_CLOSED:
              errorDescription = errCloseClosedStream;
              break;
          }
        }
        throw new TypeError(errorDescription);
      }

      return ReadableStreamDefaultControllerClose(this);
    }

    enqueue(chunk) {
      if (IsReadableStreamDefaultController(this) === false) {
        throw new TypeError(streamErrors.illegalInvocation);
      }

      if (!ReadableStreamDefaultControllerCanCloseOrEnqueue(this)) {
        const stream = this[_controlledReadableStream];
        throw getReadableStreamEnqueueError(stream, this);
      }

      return ReadableStreamDefaultControllerEnqueue(this, chunk);
    }

    error(e) {
      if (IsReadableStreamDefaultController(this) === false) {
        throw new TypeError(streamErrors.illegalInvocation);
      }

      return ReadableStreamDefaultControllerError(this, e);
    }
  }

  const ReadableStreamDefaultController_prototype =
        ReadableStreamDefaultController.prototype;

  // [[CancelSteps]] in the standard.
  function ReadableStreamDefaultControllerCancel(controller, reason) {
    controller[_queue] = new binding.SimpleQueue();
    return controller[_cancelAlgorithm](reason);
  }

  // [[PullSteps]] in the standard.
  function ReadableStreamDefaultControllerPull(controller, forAuthorCode) {
    const stream = controller[_controlledReadableStream];

    if (controller[_queue].length > 0) {
      const chunk = DequeueValue(controller);

      if ((controller[_readableStreamDefaultControllerBits] &
           CLOSE_REQUESTED) &&
          controller[_queue].length === 0) {
        ReadableStreamClose(stream);
      } else {
        ReadableStreamDefaultControllerCallPullIfNeeded(controller);
      }

      return createResolvedPromise(
          ReadableStreamCreateReadResult(chunk, false, forAuthorCode));
    }

    const pendingPromise = ReadableStreamAddReadRequest(stream, forAuthorCode);
    ReadableStreamDefaultControllerCallPullIfNeeded(controller);
    return pendingPromise;
  }

  //
  // Readable Stream Default Controller Abstract Operations
  //

  function IsReadableStreamDefaultController(x) {
    return hasOwnPropertyNoThrow(x, _controlledReadableStream);
  }

  function ReadableStreamDefaultControllerCallPullIfNeeded(controller) {
    const shouldPull =
          ReadableStreamDefaultControllerShouldCallPull(controller);
    if (shouldPull === false) {
      return;
    }

    if (controller[_readableStreamDefaultControllerBits] & PULLING) {
      controller[_readableStreamDefaultControllerBits] |= PULL_AGAIN;
      return;
    }

    controller[_readableStreamDefaultControllerBits] |= PULLING;

    thenPromise(
        controller[_pullAlgorithm](),
        () => {
          controller[_readableStreamDefaultControllerBits] &= ~PULLING;

          if (controller[_readableStreamDefaultControllerBits] & PULL_AGAIN) {
            controller[_readableStreamDefaultControllerBits] &= ~PULL_AGAIN;
            ReadableStreamDefaultControllerCallPullIfNeeded(controller);
          }
        },
        e => {
          ReadableStreamDefaultControllerError(controller, e);
        });
  }

  function ReadableStreamDefaultControllerShouldCallPull(controller) {
    if (!ReadableStreamDefaultControllerCanCloseOrEnqueue(controller)) {
      return false;
    }
    if (!(controller[_readableStreamDefaultControllerBits] & STARTED)) {
      return false;
    }

    const stream = controller[_controlledReadableStream];
    if (IsReadableStreamLocked(stream) === true &&
        ReadableStreamGetNumReadRequests(stream) > 0) {
      return true;
    }

    const desiredSize =
          ReadableStreamDefaultControllerGetDesiredSize(controller);
    // assert(desiredSize !== null, '_desiredSize_ is not *null*.');
    return desiredSize > 0;
  }

  function ReadableStreamDefaultControllerClose(controller) {
    controller[_readableStreamDefaultControllerBits] |= CLOSE_REQUESTED;

    if (controller[_queue].length === 0) {
      ReadableStreamClose(controller[_controlledReadableStream]);
    }
  }

  function ReadableStreamDefaultControllerEnqueue(controller, chunk) {
    const stream = controller[_controlledReadableStream];

    if (IsReadableStreamLocked(stream) === true &&
        ReadableStreamGetNumReadRequests(stream) > 0) {
      ReadableStreamFulfillReadRequest(stream, chunk, false);
    } else {
      let chunkSize;

      // TODO(ricea): Would it be more efficient if we avoided the
      // try ... catch when we're using the default strategy size algorithm?
      try {
        // Unlike other algorithms, strategySizeAlgorithm isn't indirected, so
        // we need to be careful with the |this| value.
        chunkSize = callFunction(controller[_strategySizeAlgorithm], undefined,
                                 chunk);
      } catch (chunkSizeE) {
        ReadableStreamDefaultControllerError(controller, chunkSizeE);
        throw chunkSizeE;
      }

      try {
        EnqueueValueWithSize(controller, chunk, chunkSize);
      } catch (enqueueE) {
        ReadableStreamDefaultControllerError(controller, enqueueE);
        throw enqueueE;
      }
    }

    ReadableStreamDefaultControllerCallPullIfNeeded(controller);
  }

  function ReadableStreamDefaultControllerError(controller, e) {
    const stream = controller[_controlledReadableStream];
    if (ReadableStreamGetState(stream) !== STATE_READABLE) {
      return;
    }
    controller[_queue] = new binding.SimpleQueue();
    ReadableStreamError(stream, e);
  }

  function ReadableStreamDefaultControllerGetDesiredSize(controller) {
    switch (ReadableStreamGetState(controller[_controlledReadableStream])) {
      case STATE_ERRORED:
        return null;

      case STATE_CLOSED:
        return 0;

      default:
        return controller[_strategyHWM] - controller[_queueTotalSize];
    }
  }

  function ReadableStreamDefaultControllerHasBackpressure(controller) {
    return !ReadableStreamDefaultControllerShouldCallPull(controller);
  }

  function ReadableStreamDefaultControllerCanCloseOrEnqueue(controller) {
    if (controller[_readableStreamDefaultControllerBits] & CLOSE_REQUESTED) {
      return false;
    }
    const state = ReadableStreamGetState(controller[_controlledReadableStream]);
    return state === STATE_READABLE;
  }

  function SetUpReadableStreamDefaultController(
      stream, controller, startAlgorithm, pullAlgorithm, cancelAlgorithm,
      highWaterMark, sizeAlgorithm) {
    controller[_controlledReadableStream] = stream;
    controller[_queue] = new binding.SimpleQueue();
    controller[_queueTotalSize] = 0;
    controller[_strategySizeAlgorithm] = sizeAlgorithm;
    controller[_strategyHWM] = highWaterMark;
    controller[_pullAlgorithm] = pullAlgorithm;
    controller[_cancelAlgorithm] = cancelAlgorithm;
    stream[_controller] = controller;

    thenPromise(createResolvedPromise(startAlgorithm()), () => {
      controller[_readableStreamDefaultControllerBits] |= STARTED;
      ReadableStreamDefaultControllerCallPullIfNeeded(controller);
    }, r =>  ReadableStreamDefaultControllerError(controller, r));
  }

  function SetUpReadableStreamDefaultControllerFromUnderlyingSource(
      stream, underlyingSource, highWaterMark, sizeAlgorithm) {
    const controller = ObjectCreate(ReadableStreamDefaultController_prototype);
    const startAlgorithm =
          () => CallOrNoop1(underlyingSource, 'start', controller,
                            'underlyingSource.start');
    const pullAlgorithm = CreateAlgorithmFromUnderlyingMethodPassingController(
        underlyingSource, 'pull', 0, controller, 'underlyingSource.pull');
    const cancelAlgorithm = CreateAlgorithmFromUnderlyingMethod(
        underlyingSource, 'cancel', 1, 'underlyingSource.cancel');
    SetUpReadableStreamDefaultController(
        stream, controller, startAlgorithm, pullAlgorithm, cancelAlgorithm,
        highWaterMark, sizeAlgorithm);
  }

  //
  // Functions for transferable streams.
  //

  // The |port| which is passed to this function must be a MessagePort which is
  // attached by a MessageChannel to the |port| that will be passed to
  // ReadableStreamDeserialize.
  function ReadableStreamSerialize(readable, port) {
    // assert(IsReadableStream(readable),
    //        `! IsReadableStream(_readable_) is true`);
    if (IsReadableStreamLocked(readable)) {
      throw new TypeError(streamErrors.cannotTransferLockedStream);
    }

    if (!binding.MessagePort_postMessage) {
      throw new TypeError(streamErrors.cannotTransferContext);
    }

    const writable = CreateCrossRealmTransformWritable(port);
    const promise =
          ReadableStreamPipeTo(readable, writable, false, false, false);
    markPromiseAsHandled(promise);
  }

  function ReadableStreamDeserialize(port) {
    return CreateCrossRealmTransformReadable(port);
  }

  //
  // Internal functions. Not part of the standard.
  //

  function ReadableStreamGetState(stream) {
    return (stream[_readableStreamBits] & STATE_MASK) >> STATE_BITS_OFFSET;
  }

  function ReadableStreamSetState(stream, state) {
    stream[_readableStreamBits] = (stream[_readableStreamBits] & ~STATE_MASK) |
        (state << STATE_BITS_OFFSET);
  }

  //
  // Functions exported for use by TransformStream. Not part of the standard.
  //

  function IsReadableStreamReadable(stream) {
    return ReadableStreamGetState(stream) === STATE_READABLE;
  }

  function IsReadableStreamClosed(stream) {
    return ReadableStreamGetState(stream) === STATE_CLOSED;
  }

  function IsReadableStreamErrored(stream) {
    return ReadableStreamGetState(stream) === STATE_ERRORED;
  }

  // Used internally by enqueue() and also by TransformStream.
  function getReadableStreamEnqueueError(stream, controller) {
    if (controller[_readableStreamDefaultControllerBits] & CLOSE_REQUESTED) {
      return new TypeError(errEnqueueCloseRequestedStream);
    }

    const state = ReadableStreamGetState(stream);
    if (state === STATE_ERRORED) {
      return new TypeError(errEnqueueErroredStream);
    }
    // assert(state === STATE_CLOSED, 'state is "closed"');
    return new TypeError(errEnqueueClosedStream);
  }

  //
  // Accessors used by TransformStream
  //

  function getReadableStreamController(stream) {
    // assert(
    //     IsReadableStream(stream), '! IsReadableStream(stream) is true.');
    return stream[_controller];
  }

  function getReadableStreamStoredError(stream) {
    // assert(
    //     IsReadableStream(stream), '! IsReadableStream(stream) is true.');
    return stream[_storedError];
  }

  // TODO(yhirano): Rename this to constructReadableStream.
  function createReadableStream(underlyingSource, strategy) {
    return new ReadableStream(underlyingSource, strategy);
  }

  // TODO(yhirano): Rename this to
  // constructReadableStreamWithExternalController.
  // TODO(ricea): Remove this once the C++ code switches to calling
  // CreateReadableStream().
  function createReadableStreamWithExternalController(
      underlyingSource, strategy) {
    return new ReadableStream(
        underlyingSource, strategy, createWithExternalControllerSentinel);
  }

  Object.assign(binding, {
    //
    // ReadableStream exports to Blink C++
    //
    AcquireReadableStreamDefaultReader,
    createReadableStream,
    createReadableStreamWithExternalController,
    IsReadableStream,
    IsReadableStreamDisturbed,
    IsReadableStreamLocked,
    IsReadableStreamReadable,
    IsReadableStreamClosed,
    IsReadableStreamErrored,
    IsReadableStreamDefaultReader,
    ReadableStreamDefaultReaderRead,
    ReadableStreamCancel,
    ReadableStreamTee,
    ReadableStreamPipeTo,
    ReadableStreamSerialize,
    ReadableStreamDeserialize,
    internalReadableStreamSymbol,

    //
    // Controller exports to Blink C++
    //
    ReadableStreamDefaultControllerClose,
    ReadableStreamDefaultControllerGetDesiredSize,
    ReadableStreamDefaultControllerEnqueue,
    ReadableStreamDefaultControllerError,

    //
    // Exports to TransformStream
    //
    CreateReadableStream,
    ReadableStreamDefaultControllerCanCloseOrEnqueue,
    ReadableStreamDefaultControllerHasBackpressure,

    getReadableStreamEnqueueError,
    getReadableStreamController,
    getReadableStreamStoredError,
  });
});
