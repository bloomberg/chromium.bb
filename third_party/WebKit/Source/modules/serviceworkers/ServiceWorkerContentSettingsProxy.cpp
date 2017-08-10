// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/serviceworkers/ServiceWorkerContentSettingsProxy.h"

#include <memory>
#include <string>

namespace blink {

ServiceWorkerContentSettingsProxy::ServiceWorkerContentSettingsProxy(
    mojom::blink::WorkerContentSettingsProxyPtrInfo host_info)
    : host_info_(std::move(host_info)) {}

ServiceWorkerContentSettingsProxy::~ServiceWorkerContentSettingsProxy() =
    default;

void ServiceWorkerContentSettingsProxy::SetSecurityOrigin(
    RefPtr<blink::SecurityOrigin> security_origin) {
  DCHECK(!security_origin_);
  security_origin_ = security_origin->IsolatedCopy();
}

bool ServiceWorkerContentSettingsProxy::RequestFileSystemAccessSync() {
  NOTREACHED();
  return false;
}

bool ServiceWorkerContentSettingsProxy::AllowIndexedDB(
    const blink::WebString& name,
    const blink::WebSecurityOrigin&) {
  bool result = false;
  GetService()->AllowIndexedDB(security_origin_, name, &result);
  return result;
}

// Use ThreadSpecific to ensure that |content_setting_instance_host| is
// destructed on worker thread.
// Each worker has a dedicated thread so this is safe.
mojom::blink::WorkerContentSettingsProxyPtr&
ServiceWorkerContentSettingsProxy::GetService() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      ThreadSpecific<mojom::blink::WorkerContentSettingsProxyPtr>,
      content_setting_instance_host, ());
  if (!content_setting_instance_host.IsSet()) {
    DCHECK(host_info_.is_valid());
    content_setting_instance_host->Bind(std::move(host_info_));
  }
  return *content_setting_instance_host;
}

}  // namespace blink
