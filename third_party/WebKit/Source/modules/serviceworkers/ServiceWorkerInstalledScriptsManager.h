// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServiceWorkerInstalledScriptsManager_h
#define ServiceWorkerInstalledScriptsManager_h

#include "core/workers/InstalledScriptsManager.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerInstalledScriptsManager.h"

namespace blink {

// ServiceWorkerInstalledScriptsManager provides the main script and imported
// scripts of an installed service worker. The scripts are streamed from the
// browser process in parallel with worker thread initialization.
class ServiceWorkerInstalledScriptsManager : public InstalledScriptsManager {
 public:
  explicit ServiceWorkerInstalledScriptsManager(
      std::unique_ptr<WebServiceWorkerInstalledScriptsManager>);

  // InstalledScriptsManager implementation.
  bool IsScriptInstalled(const KURL& script_url) const override;
  ScriptStatus GetScriptData(const KURL& script_url,
                             ScriptData* out_script_data) override;

 private:
  std::unique_ptr<WebServiceWorkerInstalledScriptsManager> manager_;
};

}  // namespace blink

#endif  // WorkerInstalledScriptsManager_h
