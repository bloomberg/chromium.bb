// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/mock_service_worker_helper.h"

#include "base/callback.h"
#include "base/containers/contains.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/storage_usage_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browsing_data {

MockServiceWorkerHelper::MockServiceWorkerHelper(
    content::BrowserContext* browser_context)
    : ServiceWorkerHelper(browser_context->GetDefaultStoragePartition()
                              ->GetServiceWorkerContext()) {}

MockServiceWorkerHelper::~MockServiceWorkerHelper() {}

void MockServiceWorkerHelper::StartFetching(FetchCallback callback) {
  ASSERT_FALSE(callback.is_null());
  ASSERT_TRUE(callback_.is_null());
  callback_ = std::move(callback);
}

void MockServiceWorkerHelper::DeleteServiceWorkers(const url::Origin& origin) {
  ASSERT_TRUE(base::Contains(origins_, origin));
  origins_[origin] = false;
}

void MockServiceWorkerHelper::AddServiceWorkerSamples() {
  const url::Origin kOrigin1 = url::Origin::Create(GURL("https://swhost1:1/"));
  const url::Origin kOrigin2 = url::Origin::Create(GURL("https://swhost2:2/"));

  response_.emplace_back(kOrigin1, 1, base::Time());
  origins_[kOrigin1] = true;

  response_.emplace_back(kOrigin2, 2, base::Time());
  origins_[kOrigin2] = true;
}

void MockServiceWorkerHelper::Notify() {
  std::move(callback_).Run(response_);
}

void MockServiceWorkerHelper::Reset() {
  for (auto& pair : origins_)
    pair.second = true;
}

bool MockServiceWorkerHelper::AllDeleted() {
  for (const auto& pair : origins_) {
    if (pair.second)
      return false;
  }
  return true;
}

}  // namespace browsing_data
