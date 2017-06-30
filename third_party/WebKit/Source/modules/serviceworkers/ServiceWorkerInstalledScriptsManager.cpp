// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/ServiceWorkerInstalledScriptsManager.h"

#include "core/html/parser/TextResourceDecoder.h"
#include "platform/instrumentation/tracing/TraceEvent.h"

namespace blink {

ServiceWorkerInstalledScriptsManager::ServiceWorkerInstalledScriptsManager() {}

bool ServiceWorkerInstalledScriptsManager::IsScriptInstalled(
    const KURL& script_url) const {
  // TODO(shimazu): implement here.
  return false;
}

Optional<InstalledScriptsManager::ScriptData>
ServiceWorkerInstalledScriptsManager::GetScriptData(const KURL& script_url) {
  // TODO(shimazu): implement here.
  return WTF::nullopt;
}

}  // namespace blink
