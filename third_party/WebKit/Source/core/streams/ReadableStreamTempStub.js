// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function(global, binding, v8) {
  'use strict';

  // This file exists to help in the transition between IDL-implemented ReadableStream and V8 extras-implemented
  // ReadableStream. Before it was introduced, there was an IDL-implemented property of the global object named
  // "ReadableStream" that had no useful behavior, but might have been depended on by web code for some reason.

  // This file thus emulates that old IDL-implemented property, _when experimental web platform features are disabled_.
  // When experimental web platform features are _enabled_, the definition of global.ReadableStream in
  // ReadableStream.js takes priority.

  // Eventually, when the ReadableStream.js implementation is ready to be enabled by default, we can remove this file.

  const TypeError = global.TypeError;
  const defineProperty = global.Object.defineProperty;

  class ReadableStream {
    constructor() {
      throw new TypeError('Illegal constructor');
    }

    getReader() {
      throw new TypeError('Illegal invocation');
    }

    cancel() {
      throw new TypeError('Illegal invocation');
    }
  }

  defineProperty(global, 'ReadableStream', {
    value: ReadableStream,
    enumerable: false,
    configurable: true,
    writable: true
  });
});
