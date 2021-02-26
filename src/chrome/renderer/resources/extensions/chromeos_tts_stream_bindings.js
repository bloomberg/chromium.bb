// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

if ((typeof mojo === 'undefined') || !mojo.bindingsLibraryInitialized) {
  loadScript('mojo_bindings');
}
mojo.config.autoLoadMojomDeps = false;

loadScript('chromeos.tts.mojom.tts_stream.mojom');

(function() {
  let ptr = new chromeos.tts.mojom.TtsStreamPtr;
  Mojo.bindInterface(
      chromeos.tts.mojom.TtsStream.name, mojo.makeRequest(ptr).handle);
  exports.$set('returnValue', ptr);
})();
