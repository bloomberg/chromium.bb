// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function instantiateInWorker() {
  // the file incrementer.wasm is copied from
  // //v8/test/mjsunit/wasm. This is because currently we cannot
  // reference files outside the LayoutTests folder. When wasm format
  // changes require that file to be updated, there is a test on the
  // v8 side (same folder), ensure-wasm-binaries-up-to-date.js, which
  // fails and will require incrementer.wasm to be updated on that side.
  return fetch('incrementer.wasm')
    .then(response => {
      if (!response.ok) throw new Error(response.statusText);
      return response.arrayBuffer();
    })
    .then(data => {
      var mod = new WebAssembly.Module(data);
      var worker = new Worker("wasm_serialization_worker.js");
      return new Promise((resolve, reject) => {
        worker.postMessage(mod);
        worker.onmessage = function(event) {
          resolve(event.data);
        }
      });
    })
    .then(data => assert_equals(data, 43))
    .catch(error => assert_unreached(error));
}
