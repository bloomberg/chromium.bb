// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_MOCK_SERVICE_WORKER_HELPER_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_MOCK_SERVICE_WORKER_HELPER_H_

#include <list>
#include <map>

#include "base/callback.h"
#include "base/macros.h"
#include "components/browsing_data/content/service_worker_helper.h"

namespace content {
class BrowserContext;
}

namespace browsing_data {

// Mock for ServiceWorkerHelper.
// Use AddServiceWorkerSamples() or add directly to response_ list, then
// call Notify().
class MockServiceWorkerHelper : public ServiceWorkerHelper {
 public:
  explicit MockServiceWorkerHelper(content::BrowserContext* browser_context);

  // Adds some ServiceWorkerInfo samples.
  void AddServiceWorkerSamples();

  // Notifies the callback.
  void Notify();

  // Marks all service worker files as existing.
  void Reset();

  // Returns true if all service worker files were deleted since the last
  // Reset() invokation.
  bool AllDeleted();

  // ServiceWorkerHelper.
  void StartFetching(FetchCallback callback) override;
  void DeleteServiceWorkers(const GURL& origin) override;

 private:
  ~MockServiceWorkerHelper() override;

  FetchCallback callback_;
  std::map<GURL, bool> origins_;
  std::list<content::StorageUsageInfo> response_;

  DISALLOW_COPY_AND_ASSIGN(MockServiceWorkerHelper);
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_MOCK_SERVICE_WORKER_HELPER_H_
