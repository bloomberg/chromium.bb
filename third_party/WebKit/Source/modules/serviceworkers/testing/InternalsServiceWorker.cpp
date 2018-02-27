// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/testing/InternalsServiceWorker.h"

#include "modules/serviceworkers/ServiceWorker.h"

namespace blink {

ScriptPromise InternalsServiceWorker::terminateServiceWorker(
    ScriptState* script_state,
    Internals& internals,
    ServiceWorker* worker) {
  return worker->InternalsTerminate(script_state);
}

}  // namespace blink
