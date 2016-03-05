// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PromiseResolver is a helper class that allows creating a
 * Promise that will be fulfilled (resolved or rejected) some time later.
 *
 * Example:
 *  var resolver = new PromiseResolver();
 *  resolver.promise.then(function(result) {
 *    console.log('resolved with', result);
 *  });
 *  ...
 *  ...
 *  resolver.resolve({hello: 'world'});
 */

/**
 * @constructor @struct
 * @template T
 */
function PromiseResolver() {
  /** @type {function(T): void} */
  this.resolve;

  /** @type {function(*=): void} */
  this.reject;

  /** @type {!Promise<T>} */
  this.promise = new Promise(function(resolve, reject) {
    this.resolve = resolve;
    this.reject = reject;
  }.bind(this));
}
