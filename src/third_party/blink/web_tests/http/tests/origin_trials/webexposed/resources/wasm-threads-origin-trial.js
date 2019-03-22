// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Build a WebAssembly module which uses thread-features.
function createBuilderWithThreads() {
  const builder = new WasmModuleBuilder();
  const shared = true;
  builder.addMemory(2, 10, false, shared);
  builder.addFunction('main', kSig_i_ii)
      .addBody([
        kExprGetLocal, 0, kExprGetLocal, 1, kAtomicPrefix, kExprI32AtomicAdd, 2,
        0
      ])
      .exportFunc();
  return builder;
}

// Starts the function provided by blobURL on a worker and checks that the data
// the worker sends with postMessage is 'true'.
function testModuleCompilationOnWorker(blobURL) {
  let resolve;
  // Create a promise which fulfills when the worker finishes.
  let promise = new Promise(function(res, rej) {
    resolve = res;
  });

  let worker = new Worker(blobURL);
  worker.addEventListener('message', e => resolve(e.data));

  // Send the module bytes to the worker, because the worker does not have
  // access to the WasmModuleBuilder.
  worker.postMessage(createBuilderWithThreads().toBuffer());

  return promise.then(result => assert_true(result));
}

// Test that WebAssembly threads are disabled and a WebAssembly module which
// uses shared memory and atomics cannot be compiled.
function testWasmThreadsDisabled() {
  // If WebAssembly threads are enabled even without origin trial token, then we
  // do not run this test.
  if (window.internals.runtimeFlags.webAssemblyThreadsEnabled)
    return;

  try {
    // Compiling the WebAssembly bytes should throw a CompileError.
    createBuilderWithThreads().toModule();
    assert_unreachable();
  } catch (e) {
    assert_true(e instanceof WebAssembly.CompileError);
  }
}

// Test that WebAssembly threads are disabled and a WebAssembly module which
// uses shared memory and atomics cannot be compiled on a worker.
function testWasmThreadsDisabledOnWorker() {
  // If WebAssembly threads are enabled even without origin trial token, then
  // we do not run this test.
  if (window.internals.runtimeFlags.webAssemblyThreadsEnabled) {
    return Promise.resolve();
  }

  const blobURL = URL.createObjectURL(new Blob(
      [
        '(',
        function() {
          self.addEventListener('message', data => {
            try {
              // Compiling the WebAssembly bytes should throw a CompileError.
              new WebAssembly.Module(data.data);
              self.postMessage(false);
            } catch (e) {
              self.postMessage(e instanceof WebAssembly.CompileError);
            }
          });
        }.toString(),
        ')()'
      ],
      {type: 'application/javascript'}));

  return testModuleCompilationOnWorker(blobURL);
}

// Test that WebAssembly threads are enabled and a WebAssembly module which
// uses shared memory and atomics can be compiled.
function testWasmThreadsEnabled() {
  const module = createBuilderWithThreads().toModule();
  assert_true(module !== undefined);
  assert_true(module instanceof WebAssembly.Module);
}

// Test that WebAssembly threads are enabled and a WebAssembly module which
// uses shared memory and atomics can be compiled on a worker.
function testWasmThreadsEnabledOnWorker() {
  let blobURL = URL.createObjectURL(new Blob(
      [
        '(',
        function() {
          self.addEventListener('message', data => {
            try {
              const module = new WebAssembly.Module(data.data);
              self.postMessage(module !== undefined);
            } catch (e) {
              console.log(e);
              self.postMessage(false);
            }
          });
        }.toString(),
        ')()'
      ],
      {type: 'application/javascript'}));

  return testModuleCompilationOnWorker(blobURL);
}
