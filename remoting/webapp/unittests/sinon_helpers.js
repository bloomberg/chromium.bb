// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var sinonHelpers = {};

sinonHelpers.reset = function() {

/**
 * @param {Object} obj
 * @param {string} method
 * @return {sinon.TestStub}
 * @suppress {reportUnknownTypes}
 */
sinon.$setupStub = function(obj, method) {
  sinon.stub(obj, method);
  obj[method].$testStub = /** @type {sinon.TestStub} */ (obj[method]);
  return obj[method].$testStub;
};

};
