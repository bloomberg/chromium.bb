// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/metrics/histogram_macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/offline_pages/core/offline_clock.h"
#include "components/offline_pages/core/prefetch/prefetch_prefs.h"
#include "components/offline_pages/core/prefetch/prefetch_request_test_base.h"
#include "components/offline_pages/core/prefetch/proto/offline_pages.pb.h"
#include "components/offline_pages/core/prefetch/proto/operation.pb.h"
#include "components/offline_pages/core/prefetch/server_forbidden_check_request.h"
#include "components/offline_pages/core/prefetch/test_prefetch_network_request_factory.h"
#include "components/prefs/testing_pref_service.h"
#include "net/http/http_status_code.h"
#include "services/network/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

class PrefService;

namespace offline_pages {
class ServerForbiddenCheckRequestTest : public PrefetchRequestTestBase {
 public:
  ServerForbiddenCheckRequestTest();

  void SetUp() override;

  void MakeRequest();

  PrefService* prefs() { return &pref_service_; }
  PrefetchNetworkRequestFactory* request_factory() { return &request_factory_; }

 private:
  TestingPrefServiceSimple pref_service_;
  TestPrefetchNetworkRequestFactory request_factory_;
};

ServerForbiddenCheckRequestTest::ServerForbiddenCheckRequestTest()
    : request_factory_(shared_url_loader_factory(), prefs()) {}

void ServerForbiddenCheckRequestTest::SetUp() {
  PrefetchRequestTestBase::SetUp();

  prefetch_prefs::RegisterPrefs(pref_service_.registry());

  // Ensure check will happen.
  prefetch_prefs::SetPrefetchingEnabledInSettings(prefs(), true);
}

void ServerForbiddenCheckRequestTest::MakeRequest() {
  CheckIfEnabledByServer(request_factory(), prefs());
}

TEST_F(ServerForbiddenCheckRequestTest, StillForbidden) {
  base::HistogramTester histogram_tester;

  prefetch_prefs::SetEnabledByServer(prefs(), false);
  MakeRequest();
  RespondWithHttpErrorAndData(net::HTTP_FORBIDDEN, "request forbidden by OPS");
  RunUntilIdle();

  EXPECT_FALSE(prefetch_prefs::IsForbiddenCheckDue(prefs()));
  EXPECT_FALSE(prefetch_prefs::IsEnabledByServer(prefs()));

  // Ensure that the status was recorded in UMA.
  histogram_tester.ExpectUniqueSample(
      "OfflinePages.Prefetching.ServiceGetPageBundleStatus",
      static_cast<int>(PrefetchRequestStatus::kShouldSuspendForbiddenByOPS), 1);
}

TEST_F(ServerForbiddenCheckRequestTest, NoLongerForbidden) {
  base::HistogramTester histogram_tester;
  MakeRequest();

  std::string operation_data, bundle_data;
  proto::Operation operation;

  proto::PageBundle bundle;
  bundle.Clear();
  bundle.SerializeToString(&bundle_data);

  operation.set_name("operations/empty");
  operation.set_done(true);
  operation.mutable_response()->set_type_url(
      "type.googleapis.com/google.internal.chrome.offlinepages.v1.PageBundle");
  operation.mutable_response()->set_value(bundle_data);
  operation.SerializeToString(&operation_data);

  RespondWithData(operation_data);
  RunUntilIdle();

  EXPECT_FALSE(prefetch_prefs::IsForbiddenCheckDue(prefs()));
  EXPECT_TRUE(prefetch_prefs::IsEnabledByServer(prefs()));

  // Ensure the request was recorded in UMA.
  histogram_tester.ExpectUniqueSample(
      "OfflinePages.Prefetching.ServiceGetPageBundleStatus",
      static_cast<int>(PrefetchRequestStatus::kEmptyRequestSuccess), 1);
}

}  // namespace offline_pages
