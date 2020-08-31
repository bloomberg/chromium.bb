// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_UNITTEST_UTILS_H_
#define COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_UNITTEST_UTILS_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/persistence/site_data/site_data_impl.h"
#include "components/performance_manager/persistence/site_data/site_data_store.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager {
namespace testing {

class MockSiteDataImplOnDestroyDelegate
    : public internal::SiteDataImpl::OnDestroyDelegate {
 public:
  MockSiteDataImplOnDestroyDelegate();
  ~MockSiteDataImplOnDestroyDelegate();

  MOCK_METHOD1(OnSiteDataImplDestroyed, void(internal::SiteDataImpl*));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSiteDataImplOnDestroyDelegate);
};

// An implementation of a SiteDataStore that doesn't record anything.
class NoopSiteDataStore : public SiteDataStore {
 public:
  NoopSiteDataStore();
  ~NoopSiteDataStore() override;

  // SiteDataStore:
  void ReadSiteDataFromStore(const url::Origin& origin,
                             ReadSiteDataFromStoreCallback callback) override;
  void WriteSiteDataIntoStore(
      const url::Origin& origin,
      const SiteDataProto& site_characteristic_proto) override;
  void RemoveSiteDataFromStore(
      const std::vector<url::Origin>& site_origins) override;
  void ClearStore() override;
  void GetStoreSize(GetStoreSizeCallback callback) override;
  void SetInitializationCallbackForTesting(base::OnceClosure callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NoopSiteDataStore);
};

}  // namespace testing
}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_UNITTEST_UTILS_H_
