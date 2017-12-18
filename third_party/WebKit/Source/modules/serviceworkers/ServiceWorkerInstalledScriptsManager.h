// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServiceWorkerInstalledScriptsManager_h
#define ServiceWorkerInstalledScriptsManager_h

#include "core/workers/InstalledScriptsManager.h"
#include "modules/ModulesExport.h"
#include "modules/serviceworkers/ThreadSafeScriptContainer.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/KURLHash.h"
#include "platform/wtf/HashSet.h"
#include "public/platform/WebVector.h"
#include "third_party/WebKit/common/service_worker/service_worker_installed_scripts_manager.mojom-blink.h"

namespace blink {

class WebURL;

// ServiceWorkerInstalledScriptsManager provides the main script and imported
// scripts of an installed service worker. The scripts are streamed from the
// browser process in parallel with worker thread initialization.
class MODULES_EXPORT ServiceWorkerInstalledScriptsManager
    : public InstalledScriptsManager {
 public:
  ServiceWorkerInstalledScriptsManager(
      WebVector<WebURL> installed_urls,
      mojom::blink::ServiceWorkerInstalledScriptsManagerRequest,
      mojom::blink::ServiceWorkerInstalledScriptsManagerHostPtrInfo,
      scoped_refptr<base::SingleThreadTaskRunner> io_task_runner);
  virtual ~ServiceWorkerInstalledScriptsManager() = default;

  // InstalledScriptsManager implementation.
  bool IsScriptInstalled(const KURL& script_url) const override;
  ScriptStatus GetScriptData(const KURL& script_url,
                             ScriptData* out_script_data) override;

 private:
  friend class ServiceWorkerInstalledScriptsManagerTest;

  std::unique_ptr<ThreadSafeScriptContainer::RawScriptData> GetRawScriptData(
      const KURL& script_url);

  WTF::HashSet<KURL> installed_urls_;
  scoped_refptr<ThreadSafeScriptContainer> script_container_;
  scoped_refptr<
      mojom::blink::ThreadSafeServiceWorkerInstalledScriptsManagerHostPtr>
      manager_host_;
};

}  // namespace blink

#endif  // ServiceWorkerInstalledScriptsManager_h
