// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of TransformStream for Blink.  See
// https://streams.spec.whatwg.org/#ts. The implementation closely follows the
// standard, except where required for performance or integration with Blink.
// In particular, classes, methods and abstract operations are implemented in
// the same order as in the standard, to simplify side-by-side reading.

(function(global, binding, v8) {
  'use strict';

  // Private symbols. These correspond to the internal slots in the standard.
  // "[[X]]" in the standard is spelt _X in this implementation.

  const _backpressure = v8.createPrivateSymbol('[[backpressure]]');
  const _backpressureChangePromise =
        v8.createPrivateSymbol('[[backpressureChangePromise]]');
  const _readable = v8.createPrivateSymbol('[[readable]]');
  const _transformer = v8.createPrivateSymbol('[[transformer]]');
  const _transformStreamController =
        v8.createPrivateSymbol('[[transformStreamController]]');
  const _writable = v8.createPrivateSymbol('[[writable]]');
  const _controlledTransformStream =
        v8.createPrivateSymbol('[[controlledTransformStream]]');
  const _ownerTransformStream =
        v8.createPrivateSymbol('[[ownerTransformStream]]');
  const _startPromise = v8.createPrivateSymbol('[[startPromise]]');

  // Javascript functions. It is important to use these copies, as the ones on
  // the global object may have been overwritten. See "V8 Extras Design Doc",
  // section "Security Considerations".
  // https://docs.google.com/document/d/1AT5-T0aHGp7Lt29vPWFr2-qG8r3l9CByyvKwEuA8Ec0/edit#heading=h.9yixony1a18r
  const defineProperty = global.Object.defineProperty;

  const Function_call = v8.uncurryThis(global.Function.prototype.call);

  const TypeError = global.TypeError;
  const RangeError = global.RangeError;

  const Promise = global.Promise;
  const thenPromise = v8.uncurryThis(Promise.prototype.then);
  const Promise_resolve = v8.simpleBind(Promise.resolve, Promise);
  const Promise_reject = v8.simpleBind(Promise.reject, Promise);

  // From CommonOperations.js
  const {
    hasOwnPropertyNoThrow,
    resolvePromise,
    CallOrNoop1,
    PromiseCallOrNoop1
  } = binding.streamOperations;

  // User-visible strings.
  const streamErrors = binding.streamErrors;
  const errWritableStreamAborted = 'The writable stream has been aborted';
  const errStreamTerminated = 'The transform stream has been terminated';
  const templateErrorIsNotAFunction = f => `${f} is not a function`;

  class TransformStream {
    constructor(
        transformer = {}, writableStrategy = undefined,
        {size, highWaterMark = 0} = {}) {
      this[_transformer] = transformer;

      // readable and writableType are extension points for future byte streams.
      const readableType = transformer.readableType;
      if (readableType !== undefined) {
        throw new RangeError(streamErrors.invalidType);
      }

      const writableType = transformer.writableType;
      if (writableType !== undefined) {
        throw new RangeError(streamErrors.invalidType);
      }

      this[_transformStreamController] = undefined;
      const controller = new TransformStreamDefaultController(this);
      this[_transformStreamController] = controller;

      const startPromise = v8.createPromise();
      const source = new TransformStreamDefaultSource(this, startPromise);
      const readableStrategy = {size, highWaterMark};
      this[_readable] = new binding.ReadableStream(source, readableStrategy);

      const sink = new TransformStreamDefaultSink(this, startPromise);
      this[_writable] = new binding.WritableStream(sink, writableStrategy);

      // this[_backpressure] and this[_backpressureChangePromise]] are already
      // undefined, so save a tiny amount of code by not setting them
      // explicitly.
      TransformStreamSetBackpressure(this, true);

      const startResult = CallOrNoop1(transformer, 'start', controller,
                                      'transformer.start');
      resolvePromise(startPromise, startResult);
    }

    get readable() {
      if (!IsTransformStream(this)) {
        throw new TypeError(streamErrors.illegalInvocation);
      }

      return this[_readable];
    }

    get writable() {
      if (!IsTransformStream(this)) {
        throw new TypeError(streamErrors.illegalInvocation);
      }

      return this[_writable];
    }
  }

  function IsTransformStream(x) {
    return hasOwnPropertyNoThrow(x, _transformStreamController);
  }

  function TransformStreamError(stream, e) {
    const readable = stream[_readable];
    if (binding.IsReadableStreamReadable(readable)) {
      binding.ReadableStreamDefaultControllerError(
          binding.getReadableStreamController(readable), e);
    }

    TransformStreamErrorWritableAndUnblockWrite(stream, e);
  }

  function TransformStreamErrorWritableAndUnblockWrite(stream, e) {
    binding.WritableStreamDefaultControllerErrorIfNeeded(
        binding.getWritableStreamController(stream[_writable]), e);

    if (stream[_backpressure]) {
      TransformStreamSetBackpressure(stream, false);
    }
  }

  function TransformStreamSetBackpressure(stream, backpressure) {
    // assert(
    //     stream[_backpressure] !== backpressure,
    //     'stream.[[backpressure]] is not backpressure');

    if (stream[_backpressureChangePromise] !== undefined) {
      resolvePromise(stream[_backpressureChangePromise], undefined);
    }

    stream[_backpressureChangePromise] = v8.createPromise();
    stream[_backpressure] = backpressure;
  }

  class TransformStreamDefaultController {
    constructor(stream) {
      if (!IsTransformStream(stream)) {
        throw new TypeError(streamErrors.illegalConstructor);
      }

      if (stream[_transformStreamController] !== undefined) {
        throw new TypeError(streamErrors.illegalConstructor);
      }

      this[_controlledTransformStream] = stream;
    }

    get desiredSize() {
      if (!IsTransformStreamDefaultController(this)) {
        throw new TypeError(streamErrors.illegalInvocation);
      }

      const readableController = binding.getReadableStreamController(
          this[_controlledTransformStream][_readable]);
      return binding.ReadableStreamDefaultControllerGetDesiredSize(
          readableController);
    }

    enqueue(chunk) {
      if (!IsTransformStreamDefaultController(this)) {
        throw new TypeError(streamErrors.illegalInvocation);
      }

      TransformStreamDefaultControllerEnqueue(this, chunk);
    }

    error(reason) {
      if (!IsTransformStreamDefaultController(this)) {
        throw new TypeError(streamErrors.illegalInvocation);
      }

      TransformStreamDefaultControllerError(this, reason);
    }

    terminate() {
      if (!IsTransformStreamDefaultController(this)) {
        throw new TypeError(streamErrors.illegalInvocation);
      }

      TransformStreamDefaultControllerTerminate(this);
    }
  }

  function IsTransformStreamDefaultController(x) {
    return hasOwnPropertyNoThrow(x, _controlledTransformStream);
  }

  function TransformStreamDefaultControllerEnqueue(controller, chunk) {
    const stream = controller[_controlledTransformStream];
    const readableController =
          binding.getReadableStreamController(stream[_readable]);

    if (!binding.ReadableStreamDefaultControllerCanCloseOrEnqueue(
        readableController)) {
      throw binding.getReadableStreamEnqueueError(stream[_readable]);
    }

    try {
      binding.ReadableStreamDefaultControllerEnqueue(readableController, chunk);
    } catch (e) {
      TransformStreamErrorWritableAndUnblockWrite(stream, e);
      throw binding.getReadableStreamStoredError(stream[_readable]);
    }

    const backpressure = binding.ReadableStreamDefaultControllerHasBackpressure(
        readableController);
    if (backpressure !== stream[_backpressure]) {
      // assert(backpressure, 'backpressure is true');
      TransformStreamSetBackpressure(stream, true);
    }
  }

  function TransformStreamDefaultControllerError(controller, e) {
    TransformStreamError(controller[_controlledTransformStream], e);
  }

  function TransformStreamDefaultControllerTerminate(controller) {
    const stream = controller[_controlledTransformStream];
    const readableController =
          binding.getReadableStreamController(stream[_readable]);

    if (binding.ReadableStreamDefaultControllerCanCloseOrEnqueue(
        readableController)) {
      binding.ReadableStreamDefaultControllerClose(readableController);
    }

    const error = new TypeError(errStreamTerminated);
    TransformStreamErrorWritableAndUnblockWrite(stream, error);
  }

  class TransformStreamDefaultSink {
    constructor(stream, startPromise) {
      this[_ownerTransformStream] = stream;
      this[_startPromise] = startPromise;
    }

    start() {
      const startPromise = this[_startPromise];
      // Permit GC of the promise.
      this[_startPromise] = undefined;
      return startPromise;
    }

    write(chunk) {
      const stream = this[_ownerTransformStream];
      // assert(
      //     binding.isWritableStreamWritable(stream[_writable]),
      //     `stream.[[writable]][[state]] is "writable"`);

      if (stream[_backpressure]) {
        const backpressureChangePromise = stream[_backpressureChangePromise];
        // assert(
        //     backpressureChangePromise !== undefined,
        //     `backpressureChangePromise is not undefined`);

        return thenPromise(backpressureChangePromise, () => {
          const writable = stream[_writable];
          if (binding.isWritableStreamErroring(writable)) {
            throw binding.getWritableStreamStoredError(writable);
          }
          // assert(
          //     binding.isWritableStreamWritable(writable),
          //     `state is "writable"`);

          return TransformStreamDefaultSinkTransform(this, chunk);
        });
      }

      return TransformStreamDefaultSinkTransform(this, chunk);
    }

    abort() {
      const e = new TypeError(errWritableStreamAborted);
      TransformStreamError(this[_ownerTransformStream], e);
    }

    close() {
      const stream = this[_ownerTransformStream];
      const readable = stream[_readable];

      const flushPromise = PromiseCallOrNoop1(
          stream[_transformer], 'flush', stream[_transformStreamController],
          'transformer.flush');

      return thenPromise(
          flushPromise,
          () => {
            if (binding.IsReadableStreamErrored(readable)) {
              throw binding.getReadableStreamStoredError(readable);
            }

            const readableController =
                  binding.getReadableStreamController(readable);
            if (binding.ReadableStreamDefaultControllerCanCloseOrEnqueue(
                readableController)) {
              binding.ReadableStreamDefaultControllerClose(readableController);
            }
          },
          r => {
            TransformStreamError(stream, r);
            throw binding.getReadableStreamStoredError(readable);
          });
    }
  }

  function TransformStreamDefaultSinkInvokeTransform(stream, chunk) {
    const controller = stream[_transformStreamController];
    const transformer = stream[_transformer];
    const method = transformer.transform;

    if (method === undefined) {
      TransformStreamDefaultControllerEnqueue(controller, chunk);
      return undefined;
    }

    if (typeof method !== 'function') {
      throw new TypeError(templateErrorIsNotAFunction('transform'));
    }

    return Function_call(method, transformer, chunk, controller);
  }

  function TransformStreamDefaultSinkTransform(sink, chunk) {
    const stream = sink[_ownerTransformStream];
    // assert(
    //     !binding.IsReadableStreamErrored(stream[_readable]),
    //     'stream.[[readable]].[[state]] is not "errored"');
    // assert(!stream[_backpressure], 'stream.[[backpressure]] is false');

    let transformPromise;
    try {
      transformPromise = Promise_resolve(
          TransformStreamDefaultSinkInvokeTransform(stream, chunk));
    } catch (e) {
      transformPromise = Promise_reject(e);
    }

    return thenPromise(transformPromise, undefined, e => {
      TransformStreamError(stream, e);
      throw e;
    });
  }

  class TransformStreamDefaultSource {
    constructor(stream, startPromise) {
      this[_ownerTransformStream] = stream;
      this[_startPromise] = startPromise;
    }

    start() {
      const startPromise = this[_startPromise];
      // Permit GC of the promise.
      this[_startPromise] = undefined;
      return startPromise;
    }

    pull() {
      const stream = this[_ownerTransformStream];
      // assert(stream[_backpressure], 'stream.[[backpressure]] is true');
      // assert(
      //     stream[_backpressureChangePromise] !== undefined,
      //     'stream.[[backpressureChangePromise]] is not undefined');

      TransformStreamSetBackpressure(stream, false);
      return stream[_backpressureChangePromise];
    }

    cancel(reason) {
      TransformStreamErrorWritableAndUnblockWrite(
          this[_ownerTransformStream], reason);
    }
  }

  //
  // Additions to the global object
  //

  defineProperty(global, 'TransformStream', {
    value: TransformStream,
    enumerable: false,
    configurable: true,
    writable: true
  });
});
