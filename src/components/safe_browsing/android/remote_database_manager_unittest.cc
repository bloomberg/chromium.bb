// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/android/remote_database_manager.h"

#include <map>
#include <memory>

#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "components/safe_browsing/android/safe_browsing_api_handler.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/test/browser_task_environment.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

namespace {

class TestSafeBrowsingApiHandler : public SafeBrowsingApiHandler {
 public:
  void StartURLCheck(std::unique_ptr<URLCheckCallbackMeta> callback,
                     const GURL& url,
                     const SBThreatTypeSet& threat_types) override {}
  bool StartCSDAllowlistCheck(const GURL& url) override { return false; }
  bool StartHighConfidenceAllowlistCheck(const GURL& url) override {
    return false;
  }
};

}  // namespace

class RemoteDatabaseManagerTest : public testing::Test {
 protected:
  RemoteDatabaseManagerTest() {}

  void SetUp() override {
    SafeBrowsingApiHandler::SetInstance(&api_handler_);
    db_ = new RemoteSafeBrowsingDatabaseManager();
  }

  void TearDown() override {
    db_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

  // Setup the two field trial params.  These are read in db_'s ctor.
  void SetFieldTrialParams(const std::string types_to_check_val) {
    variations::testing::ClearAllVariationIDs();
    variations::testing::ClearAllVariationParams();

    const std::string group_name = "GroupFoo";  // Value not used
    const std::string experiment_name = "SafeBrowsingAndroid";
    ASSERT_TRUE(
        base::FieldTrialList::CreateFieldTrial(experiment_name, group_name));

    std::map<std::string, std::string> params;
    if (!types_to_check_val.empty())
      params["types_to_check"] = types_to_check_val;

    ASSERT_TRUE(variations::AssociateVariationParams(experiment_name,
                                                     group_name, params));
  }

  content::BrowserTaskEnvironment task_environment_;
  TestSafeBrowsingApiHandler api_handler_;
  scoped_refptr<RemoteSafeBrowsingDatabaseManager> db_;
};

TEST_F(RemoteDatabaseManagerTest, DisabledViaNull) {
  EXPECT_TRUE(db_->IsSupported());

  SafeBrowsingApiHandler::SetInstance(nullptr);
  EXPECT_FALSE(db_->IsSupported());
}

TEST_F(RemoteDatabaseManagerTest, DestinationsToCheckDefault) {
  // Most are true, a few are false.
  for (int t_int = 0;
       t_int <= static_cast<int>(network::mojom::RequestDestination::kMaxValue);
       t_int++) {
    network::mojom::RequestDestination t =
        static_cast<network::mojom::RequestDestination>(t_int);
    switch (t) {
      case network::mojom::RequestDestination::kStyle:
      case network::mojom::RequestDestination::kImage:
      case network::mojom::RequestDestination::kFont:
        EXPECT_FALSE(db_->CanCheckRequestDestination(t));
        break;
      default:
        EXPECT_TRUE(db_->CanCheckRequestDestination(t));
        break;
    }
  }
}

TEST_F(RemoteDatabaseManagerTest, DestinationsToCheckFromTrial) {
  SetFieldTrialParams("7,16,blah, 20");
  db_ = new RemoteSafeBrowsingDatabaseManager();
  EXPECT_TRUE(db_->CanCheckRequestDestination(
      network::mojom::RequestDestination::kDocument));  // defaulted
  EXPECT_TRUE(db_->CanCheckRequestDestination(
      network::mojom::RequestDestination::kIframe));
  EXPECT_TRUE(db_->CanCheckRequestDestination(
      network::mojom::RequestDestination::kFrame));
  EXPECT_TRUE(db_->CanCheckRequestDestination(
      network::mojom::RequestDestination::kFencedframe));
  EXPECT_TRUE(db_->CanCheckRequestDestination(
      network::mojom::RequestDestination::kStyle));
  EXPECT_FALSE(db_->CanCheckRequestDestination(
      network::mojom::RequestDestination::kScript));
  EXPECT_FALSE(db_->CanCheckRequestDestination(
      network::mojom::RequestDestination::kImage));
  // ...
  EXPECT_FALSE(db_->CanCheckRequestDestination(
      network::mojom::RequestDestination::kVideo));
  EXPECT_TRUE(db_->CanCheckRequestDestination(
      network::mojom::RequestDestination::kWorker));
}

}  // namespace safe_browsing
