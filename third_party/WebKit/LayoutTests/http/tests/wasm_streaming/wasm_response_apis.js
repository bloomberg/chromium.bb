// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const incrementer_url = '../wasm/incrementer.wasm';

function TestStreamedCompile() {
  return fetch(incrementer_url)
    .then(WebAssembly.compile)
    .then(m => new WebAssembly.Instance(m))
    .then(i => assert_equals(5, i.exports.increment(4)));
}

function TestShortFormStreamedCompile() {
  return WebAssembly.compile(fetch(incrementer_url))
    .then(m => new WebAssembly.Instance(m))
    .then(i => assert_equals(5, i.exports.increment(4)));
}

function NegativeTestStreamedCompilePromise() {
  return WebAssembly.compile(new Promise((resolve, reject)=>{resolve(5);}))
    .then(assert_unreached,
          e => assert_true(e instanceof TypeError));
}

function BlankResponse() {
  return WebAssembly.compile(new Response())
    .then(assert_unreached,
          e => assert_true(e instanceof TypeError));
}

function FromArrayBuffer() {
  return fetch(incrementer_url)
    .then(r => r.arrayBuffer())
    .then(arr => new Response(arr))
    .then(WebAssembly.compile)
    .then(m => new WebAssembly.Instance(m))
    .then(i => assert_equals(6, i.exports.increment(5)));
}

function FromInvalidArrayBuffer() {
  var arr = new ArrayBuffer(10);
  var view = new Uint8Array(arr);
  for (var i = 0; i < view.length; ++i) view[i] = i;

  return WebAssembly.compile(new Response(arr))
    .then(assert_unreached,
          e => assert_true(e instanceof Error));
}
