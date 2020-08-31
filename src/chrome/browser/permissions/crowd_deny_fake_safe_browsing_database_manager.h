// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_CROWD_DENY_FAKE_SAFE_BROWSING_DATABASE_MANAGER_H_
#define CHROME_BROWSER_PERMISSIONS_CROWD_DENY_FAKE_SAFE_BROWSING_DATABASE_MANAGER_H_

#include <map>
#include <set>

#include "base/memory/weak_ptr.h"
#include "components/safe_browsing/core/db/test_database_manager.h"
#include "components/safe_browsing/core/db/util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class CrowdDenyFakeSafeBrowsingDatabaseManager
    : public safe_browsing::TestSafeBrowsingDatabaseManager {
 public:
  CrowdDenyFakeSafeBrowsingDatabaseManager();

  void SetSimulatedMetadataForUrl(
      const GURL& url,
      const safe_browsing::ThreatMetadata& metadata);

  void RemoveAllBlacklistedUrls();

  void set_simulate_timeout(bool simulate_timeout) {
    simulate_timeout_ = simulate_timeout;
  }

  void set_simulate_synchronous_result(bool simulate_synchronous_result) {
    simulate_synchronous_result_ = simulate_synchronous_result;
  }

 protected:
  ~CrowdDenyFakeSafeBrowsingDatabaseManager() override;

  // safe_browsing::TestSafeBrowsingDatabaseManager:
  bool CheckApiBlacklistUrl(const GURL& url, Client* client) override;
  bool CancelApiCheck(Client* client) override;
  bool IsSupported() const override;
  bool ChecksAreAlwaysAsync() const override;

 private:
  safe_browsing::ThreatMetadata GetSimulatedMetadataOrSafe(const GURL& url);

  std::set<Client*> pending_clients_;
  std::map<GURL, safe_browsing::ThreatMetadata>
      url_to_simulated_threat_metadata_;
  bool simulate_timeout_ = false;
  bool simulate_synchronous_result_ = false;

  base::WeakPtrFactory<CrowdDenyFakeSafeBrowsingDatabaseManager> weak_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(CrowdDenyFakeSafeBrowsingDatabaseManager);
};

#endif  // CHROME_BROWSER_PERMISSIONS_CROWD_DENY_FAKE_SAFE_BROWSING_DATABASE_MANAGER_H_
