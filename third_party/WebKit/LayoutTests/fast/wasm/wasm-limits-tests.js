// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var limit = Math.pow(2, 12);

function TestBuffersAreCorrect() {
  var buffs = createTestBuffers(limit);
  assert_equals(buffs.small.byteLength, limit);
  assert_equals(buffs.large.byteLength, limit + 1);
}

function compileFailsWithError(buffer, error_type) {
  try {
    new WebAssembly.Module(buffer);
  } catch (e) {
    assert_true(e instanceof error_type);
  }
}

function TestSyncCompile() {
  var buffs = createTestBuffers(limit);
  assert_true(new WebAssembly.Module(buffs.small)
              instanceof WebAssembly.Module);
  compileFailsWithError(buffs.large, RangeError);
}

function TestPromiseCompile() {
  return WebAssembly.compile(createTestBuffers(limit).large)
    .then(m => assert_true(m instanceof WebAssembly.Module));
}

function TestWorkerCompileAndInstantiate() {
  var worker = new Worker("wasm-limits-worker.js");
  return new Promise((resolve, reject) => {
    worker.postMessage(createTestBuffers(limit).large);
    worker.onmessage = function(event) {
      resolve(event.data);
    }
  })
    .then(ans => assert_true(ans),
          assert_unreached);
}

function TestPromiseCompileSyncInstantiate() {
  return WebAssembly.compile(createTestBuffers(limit).large)
    .then (m => new WebAssembly.Instance(m))
    .then(assert_unreached,
          e => assert_true(e instanceof RangeError));
}

function TestPromiseCompileAsyncInstantiateFromBuffer() {
  return WebAssembly.instantiate(createTestBuffers(limit).large)
    .then(i => assert_true(i.instance instanceof WebAssembly.Instance),
          assert_unreached);
}

function TestPromiseCompileAsyncInstantiateFromModule() {
  return WebAssembly.compile(createTestBuffers(limit).large)
    .then(m => {
      assert_true(m instanceof WebAssembly.Module);
      return WebAssembly.instantiate(m).
        then(i => assert_true(i instanceof WebAssembly.Instance),
             assert_unreached);
    },
    assert_unreached);
}
