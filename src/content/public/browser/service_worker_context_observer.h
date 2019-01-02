// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_OBSERVER_H_
#define CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_OBSERVER_H_

#include "url/gurl.h"

namespace content {

class ServiceWorkerContextObserver {
 public:
  // Called when a service worker has been registered with scope |scope|.
  //
  // This is called when the ServiceWorkerContainer.register() promise is
  // resolved, which happens before the service worker registration is persisted
  // to disk.
  virtual void OnRegistrationCompleted(const GURL& scope) {}

  // Called when the service worker with id |version_id| changes status to
  // activated.
  virtual void OnVersionActivated(int64_t version_id, const GURL& scope) {}

  // Called when the service worker with id |version_id| changes status to
  // redundant.
  virtual void OnVersionRedundant(int64_t version_id, const GURL& scope) {}

  // Called when there are no more controllees for the service worker with id
  // |version_id|.
  virtual void OnNoControllees(int64_t version_id, const GURL& scope) {}

 protected:
  virtual ~ServiceWorkerContextObserver() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SERVICE_WORKER_CONTEXT_OBSERVER_H_
