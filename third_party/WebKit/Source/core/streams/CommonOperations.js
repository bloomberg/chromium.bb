// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of functions that are shared between ReadableStream and
// WritableStream.

(function(global, binding, v8) {
  'use strict';

  // Javascript functions. It is important to use these copies for security and
  // robustness. See "V8 Extras Design Doc", section "Security Considerations".
  // https://docs.google.com/document/d/1AT5-T0aHGp7Lt29vPWFr2-qG8r3l9CByyvKwEuA8Ec0/edit#heading=h.9yixony1a18r
  const Boolean = global.Boolean;
  const hasOwnProperty = v8.uncurryThis(global.Object.hasOwnProperty);

  function hasOwnPropertyNoThrow(x, property) {
    // The cast of |x| to Boolean will eliminate undefined and null, which would
    // cause hasOwnProperty to throw a TypeError, as well as some other values
    // that can't be objects and so will fail the check anyway.
    return Boolean(x) && hasOwnProperty(x, property);
  }

  binding.streamOperations = { hasOwnPropertyNoThrow };
});
