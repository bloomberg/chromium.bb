// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

// Function to test that a promise rejects with the expected error type and
// message.
function assert_promise_rejects_with_message(promise, expected, description) {
  return promise.then(() => {
    assert_unreached('Promise should have rejected: ' + description);
  }, error => {
    assert_equals(error.name, expected.name, 'Unexpected Error Name:');
    if (expected.message) {
      assert_equals(error.message, expected.message, 'Unexpected Error Message:');
    }
  });
}
