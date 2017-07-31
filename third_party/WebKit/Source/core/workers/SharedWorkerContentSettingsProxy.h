// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SharedWorkerContentSettingsProxy_h
#define SharedWorkerContentSettingsProxy_h

#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebContentSettingsClient.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/web/shared_worker_content_settings_proxy.mojom-blink.h"

namespace blink {

// SharedWorkerContentSettingsProxy provides content settings information.
// This is created on the main thread and then called on the worker thread.
// Each information is requested via a Mojo connection to the browser process.
class SharedWorkerContentSettingsProxy : public WebContentSettingsClient {
 public:
  SharedWorkerContentSettingsProxy(
      SecurityOrigin*,
      mojom::blink::SharedWorkerContentSettingsProxyPtrInfo host_info);
  ~SharedWorkerContentSettingsProxy() override;

  // WebContentSettingsClient overrides.
  bool AllowIndexedDB(const WebString& name, const WebSecurityOrigin&) override;
  bool RequestFileSystemAccessSync() override;

 private:
  // To ensure the returned pointer is destructed on the same thread
  // where it constructed, this uses ThreadSpecific.
  mojom::blink::SharedWorkerContentSettingsProxyPtr& GetService();

  const RefPtr<blink::SecurityOrigin> security_origin_;

  // This is set on the main thread at the ctor,
  // and moved to a thread localstorage on the worker thread
  // when GetService() is called for the first time.
  mojom::blink::SharedWorkerContentSettingsProxyPtrInfo host_info_;
};

}  // namespace blink

#endif  // SharedWorkerContentSettingsProxy_h
