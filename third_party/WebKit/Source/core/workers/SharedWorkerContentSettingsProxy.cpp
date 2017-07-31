// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/workers/SharedWorkerContentSettingsProxy.h"

#include <memory>
#include <utility>
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "third_party/WebKit/Source/platform/wtf/text/WTFString.h"

namespace blink {

SharedWorkerContentSettingsProxy::SharedWorkerContentSettingsProxy(
    SecurityOrigin* security_origin,
    mojom::blink::SharedWorkerContentSettingsProxyPtrInfo host_info)
    : security_origin_(security_origin->IsolatedCopy()),
      host_info_(std::move(host_info)) {}
SharedWorkerContentSettingsProxy::~SharedWorkerContentSettingsProxy() = default;

bool SharedWorkerContentSettingsProxy::AllowIndexedDB(
    const WebString& name,
    const WebSecurityOrigin& origin) {
  bool result = false;
  GetService()->AllowIndexedDB(security_origin_, name, &result);
  return result;
}

bool SharedWorkerContentSettingsProxy::RequestFileSystemAccessSync() {
  bool result = false;
  GetService()->RequestFileSystemAccessSync(security_origin_, &result);
  return result;
}

// Use ThreadSpecific to ensure that |sontent_setting_instance_host| is
// destructed on worker thread.
// Each worker has a dedicated thread so this is safe.
mojom::blink::SharedWorkerContentSettingsProxyPtr&
SharedWorkerContentSettingsProxy::GetService() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(
      ThreadSpecific<mojom::blink::SharedWorkerContentSettingsProxyPtr>,
      content_setting_instance_host, ());
  if (!content_setting_instance_host.IsSet()) {
    DCHECK(host_info_.is_valid());
    content_setting_instance_host->Bind(std::move(host_info_));
  }
  return *content_setting_instance_host;
}

}  // namespace blink
