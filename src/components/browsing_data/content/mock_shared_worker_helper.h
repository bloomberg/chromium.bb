// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_MOCK_SHARED_WORKER_HELPER_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_MOCK_SHARED_WORKER_HELPER_H_

#include <list>
#include <map>

#include "components/browsing_data/content/shared_worker_helper.h"

namespace content {
class BrowserContext;
}

namespace browsing_data {

class MockSharedWorkerHelper : public SharedWorkerHelper {
 public:
  explicit MockSharedWorkerHelper(content::BrowserContext* browser_context);

  // Adds some shared worker samples.
  void AddSharedWorkerSamples();

  // Notifies the callback.
  void Notify();

  // Marks all shared worker files as existing.
  void Reset();

  // Returns true if all shared worker files were deleted since the last
  // Reset() invokation.
  bool AllDeleted();

  // SharedWorkerHelper methods.
  void StartFetching(FetchCallback callback) override;
  void DeleteSharedWorker(const GURL& worker,
                          const std::string& name,
                          const url::Origin& constructor_origin) override;

 private:
  ~MockSharedWorkerHelper() override;

  FetchCallback callback_;
  std::map<SharedWorkerInfo, bool> workers_;
  std::list<SharedWorkerInfo> response_;

  DISALLOW_COPY_AND_ASSIGN(MockSharedWorkerHelper);
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_MOCK_SHARED_WORKER_HELPER_H_
