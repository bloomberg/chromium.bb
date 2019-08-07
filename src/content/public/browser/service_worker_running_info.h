// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_RUNNING_INFO_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_RUNNING_INFO_H_

#include "content/common/content_export.h"
#include "content/public/common/child_process_host.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"
#include "url/gurl.h"

namespace content {

// A struct containing information about a running service worker.
struct CONTENT_EXPORT ServiceWorkerRunningInfo {
  ServiceWorkerRunningInfo(const GURL& script_url,
                           int64_t id,
                           int64_t process_id)
      : script_url(script_url), version_id(id), process_id(process_id) {}

  ServiceWorkerRunningInfo() = default;

  // The service worker script URL.
  const GURL script_url;

  // The unique identifier for this service worker within the same
  // ServiceWorkerContext.
  const int64_t version_id = blink::mojom::kInvalidServiceWorkerVersionId;

  const int process_id = content::ChildProcessHost::kInvalidUniqueID;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_RUNNING_INFO_H_
