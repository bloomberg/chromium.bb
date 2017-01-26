// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include pipeTo() and pipeThrough() on the global ReadableStream object when
// the experimental flag is enabled.

(function(global, binding, v8) {
  'use strict';

  const defineProperty = global.Object.defineProperty;
  defineProperty(global.ReadableStream.prototype, 'pipeThrough', {
    value: binding.ReadableStream_prototype_pipeThrough,
    enumerable: false,
    configurable: true,
    writable: true
  });

  defineProperty(global.ReadableStream.prototype, 'pipeTo', {
    value: binding.ReadableStream_prototype_pipeTo,
    enumerable: false,
    configurable: true,
    writable: true
  });
});
