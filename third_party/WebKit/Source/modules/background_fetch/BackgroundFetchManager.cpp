// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/background_fetch/BackgroundFetchManager.h"

#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "modules/serviceworkers/ServiceWorkerRegistration.h"

namespace blink {

BackgroundFetchManager::BackgroundFetchManager(
    ServiceWorkerRegistration* registration)
    : m_registration(registration) {
  DCHECK(registration);
}

ScriptPromise BackgroundFetchManager::getTags(ScriptState* scriptState) {
  return ScriptPromise::rejectWithDOMException(
      scriptState,
      DOMException::create(NotSupportedError, "Not implemented yet."));
}

DEFINE_TRACE(BackgroundFetchManager) {
  visitor->trace(m_registration);
}

}  // namespace blink
