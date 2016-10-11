// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/NavigationPreloadManager.h"

namespace blink {

ScriptPromise NavigationPreloadManager::enable(ScriptState*) {
  NOTIMPLEMENTED();
  return ScriptPromise();
}

ScriptPromise NavigationPreloadManager::disable(ScriptState*) {
  NOTIMPLEMENTED();
  return ScriptPromise();
}

ScriptPromise NavigationPreloadManager::setHeaderValue(ScriptState*,
                                                       const String& value) {
  NOTIMPLEMENTED();
  return ScriptPromise();
}

ScriptPromise NavigationPreloadManager::getState(ScriptState*) {
  NOTIMPLEMENTED();
  return ScriptPromise();
}

NavigationPreloadManager::NavigationPreloadManager() {}

}  // namespace blink
