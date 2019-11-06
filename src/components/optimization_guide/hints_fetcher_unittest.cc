// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/hints_fetcher.h"

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "base/test/task_environment.h"
#include "components/optimization_guide/hint_cache.h"
#include "components/optimization_guide/hints_processing_util.h"
#include "components/optimization_guide/optimization_guide_features.h"
#include "components/optimization_guide/optimization_guide_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

constexpr char optimization_guide_service_url[] = "https://hintsserver.com/";

class HintsFetcherTest : public testing::Test {
 public:
  HintsFetcherTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::UI,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    base::test::ScopedFeatureList scoped_list;
    scoped_list.InitAndEnableFeatureWithParameters(
        features::kOptimizationHintsFetching, {});

    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    prefs::RegisterProfilePrefs(pref_service_->registry());

    hints_fetcher_ = std::make_unique<HintsFetcher>(
        shared_url_loader_factory_, GURL(optimization_guide_service_url),
        pref_service_.get());
    hints_fetcher_->SetTimeClockForTesting(task_environment_.GetMockClock());
  }

  ~HintsFetcherTest() override {}

  void OnHintsFetched(base::Optional<std::unique_ptr<proto::GetHintsResponse>>
                          get_hints_response) {
    if (get_hints_response)
      hints_fetched_ = true;
  }

  bool hints_fetched() { return hints_fetched_; }

  void SetConnectionOffline() {
    network_tracker_ = network::TestNetworkConnectionTracker::GetInstance();
    network_tracker_->SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_NONE);
  }

  void SetConnectionOnline() {
    network_tracker_ = network::TestNetworkConnectionTracker::GetInstance();
    network_tracker_->SetConnectionType(
        network::mojom::ConnectionType::CONNECTION_4G);
  }

  void SeedCoveredHosts(const std::vector<std::string>& hosts,
                        base::Time host_invalid_time) {
    DictionaryPrefUpdate hosts_fetched(
        pref_service(), prefs::kHintsFetcherHostsSuccessfullyFetched);

    for (const std::string& host : hosts) {
      hosts_fetched->SetDoubleKey(
          HashHostForDictionary(host),
          host_invalid_time.ToDeltaSinceWindowsEpoch().InSecondsF());
    }
  }

  PrefService* pref_service() { return pref_service_.get(); }

  const base::Clock* GetMockClock() const {
    return task_environment_.GetMockClock();
  }

 protected:
  bool FetchHints(const std::vector<std::string>& hosts) {
    bool status = hints_fetcher_->FetchOptimizationGuideServiceHints(
        hosts, base::BindOnce(&HintsFetcherTest::OnHintsFetched,
                              base::Unretained(this)));
    RunUntilIdle();
    return status;
  }

  // Return a 200 response with provided content to any pending requests.
  bool SimulateResponse(const std::string& content,
                        net::HttpStatusCode http_status) {
    return test_url_loader_factory_.SimulateResponseForPendingRequest(
        optimization_guide_service_url, content, http_status,
        network::TestURLLoaderFactory::kUrlMatchPrefix);
  }

  void VerifyHasPendingFetchRequests() {
    EXPECT_GE(test_url_loader_factory_.NumPending(), 1);
    std::string key_value;
    for (const auto& pending_request :
         *test_url_loader_factory_.pending_requests()) {
      EXPECT_EQ(pending_request.request.method, "POST");
      EXPECT_TRUE(net::GetValueForKeyInQuery(pending_request.request.url, "key",
                                             &key_value));
    }
  }

  void SimulateNavigation(const std::string& host) {
    HintsFetcher::RecordHintsFetcherCoverage(pref_service(), host);
  }

 private:
  void RunUntilIdle() {
    task_environment_.RunUntilIdle();
    base::RunLoop().RunUntilIdle();
  }

  bool hints_fetched_ = false;
  base::test::TaskEnvironment task_environment_;

  std::unique_ptr<HintsFetcher> hints_fetcher_;

  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  network::TestNetworkConnectionTracker* network_tracker_;

  DISALLOW_COPY_AND_ASSIGN(HintsFetcherTest);
};

TEST_F(HintsFetcherTest, FetchOptimizationGuideServiceHints) {
  std::string response_content;
  EXPECT_TRUE(FetchHints(std::vector<std::string>()));
  VerifyHasPendingFetchRequests();
  EXPECT_TRUE(SimulateResponse(response_content, net::HTTP_OK));
  EXPECT_TRUE(hints_fetched());
}

// Tests to ensure that multiple hint fetches by the same object cannot be in
// progress simultaneously.
TEST_F(HintsFetcherTest, FetchInProcess) {
  std::string response_content;
  // Fetch back to back without waiting for Fetch to complete,
  // |fetch_in_progress_| should cause early exit.
  EXPECT_TRUE(FetchHints(std::vector<std::string>()));
  EXPECT_FALSE(FetchHints(std::vector<std::string>()));

  // Once response arrives, check to make sure a new fetch can start.
  SimulateResponse(response_content, net::HTTP_OK);
  EXPECT_TRUE(FetchHints(std::vector<std::string>()));
}

// Tests 404 response from request.
TEST_F(HintsFetcherTest, FetchReturned404) {
  std::string response_content;

  EXPECT_TRUE(FetchHints(std::vector<std::string>()));

  // Send a 404 to HintsFetcher.
  SimulateResponse(response_content, net::HTTP_NOT_FOUND);
  EXPECT_FALSE(hints_fetched());
}

TEST_F(HintsFetcherTest, FetchReturnBadResponse) {
  std::string response_content = "not proto";
  EXPECT_TRUE(FetchHints(std::vector<std::string>()));
  VerifyHasPendingFetchRequests();
  EXPECT_TRUE(SimulateResponse(response_content, net::HTTP_OK));
  EXPECT_FALSE(hints_fetched());
}

TEST_F(HintsFetcherTest, FetchAttemptWhenNetworkOffline) {
  SetConnectionOffline();
  std::string response_content;
  EXPECT_FALSE(FetchHints(std::vector<std::string>()));
  EXPECT_FALSE(hints_fetched());

  SetConnectionOnline();
  EXPECT_TRUE(FetchHints(std::vector<std::string>()));
  VerifyHasPendingFetchRequests();
  EXPECT_TRUE(SimulateResponse(response_content, net::HTTP_OK));
  EXPECT_TRUE(hints_fetched());
}

TEST_F(HintsFetcherTest, HintsFetchSuccessfulHostsRecorded) {
  std::vector<std::string> hosts{"host1.com", "host2.com"};
  std::string response_content;

  EXPECT_TRUE(FetchHints(hosts));
  VerifyHasPendingFetchRequests();
  EXPECT_TRUE(SimulateResponse(response_content, net::HTTP_OK));
  EXPECT_TRUE(hints_fetched());

  const base::DictionaryValue* hosts_fetched = pref_service()->GetDictionary(
      prefs::kHintsFetcherHostsSuccessfullyFetched);
  base::Optional<double> value;
  for (const std::string& host : hosts) {
    value = hosts_fetched->FindDoubleKey(HashHostForDictionary(host));
    EXPECT_EQ(base::Time::FromDeltaSinceWindowsEpoch(
                  base::TimeDelta::FromSecondsD(*value)),
              GetMockClock()->Now() + base::TimeDelta::FromDays(7));
  }
}

TEST_F(HintsFetcherTest, HintsFetchFailsHostNotRecorded) {
  std::vector<std::string> hosts{"host1.com", "host2.com"};
  std::string response_content;

  EXPECT_TRUE(FetchHints(hosts));
  VerifyHasPendingFetchRequests();
  EXPECT_TRUE(SimulateResponse(response_content, net::HTTP_NOT_FOUND));
  EXPECT_FALSE(hints_fetched());

  const base::DictionaryValue* hosts_fetched = pref_service()->GetDictionary(
      prefs::kHintsFetcherHostsSuccessfullyFetched);
  for (const std::string& host : hosts) {
    EXPECT_FALSE(hosts_fetched->FindDoubleKey(HashHostForDictionary(host)));
  }
}

TEST_F(HintsFetcherTest, HintsFetchClearHostsSuccessfullyFetched) {
  std::vector<std::string> hosts{"host1.com", "host2.com"};
  std::string response_content;

  EXPECT_TRUE(FetchHints(hosts));
  VerifyHasPendingFetchRequests();
  EXPECT_TRUE(SimulateResponse(response_content, net::HTTP_OK));
  EXPECT_TRUE(hints_fetched());

  const base::DictionaryValue* hosts_fetched = pref_service()->GetDictionary(
      prefs::kHintsFetcherHostsSuccessfullyFetched);
  for (const std::string& host : hosts) {
    EXPECT_TRUE(hosts_fetched->FindDoubleKey(HashHostForDictionary(host)));
  }

  HintsFetcher::ClearHostsSuccessfullyFetched(pref_service());
  hosts_fetched = pref_service()->GetDictionary(
      prefs::kHintsFetcherHostsSuccessfullyFetched);
  for (const std::string& host : hosts) {
    EXPECT_FALSE(hosts_fetched->FindDoubleKey(HashHostForDictionary(host)));
  }
}

TEST_F(HintsFetcherTest, HintsFetcherHostsCovered) {
  base::HistogramTester histogram_tester;
  std::vector<std::string> hosts{"host1.com", "host2.com"};
  base::Time host_invalid_time =
      base::Time::Now() + base::TimeDelta().FromHours(1);

  SeedCoveredHosts(hosts, host_invalid_time);

  SimulateNavigation(hosts[0]);
  SimulateNavigation(hosts[1]);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintsFetcher.WasHostCoveredByFetch", true, 2);
}

TEST_F(HintsFetcherTest, HintsFetcherCoveredHostExpired) {
  base::HistogramTester histogram_tester;
  std::string response_content;
  std::vector<std::string> hosts{"host1.com", "host2.com"};
  base::Time host_invalid_time =
      GetMockClock()->Now() - base::TimeDelta().FromHours(1);

  SeedCoveredHosts(hosts, host_invalid_time);

  // Fetch hints for new hosts.
  std::vector<std::string> hosts_valid{"host3.com", "hosts4.com"};
  EXPECT_TRUE(FetchHints(hosts_valid));
  VerifyHasPendingFetchRequests();
  EXPECT_TRUE(SimulateResponse(response_content, net::HTTP_OK));
  EXPECT_TRUE(hints_fetched());

  // The first pair of hosts should be recorded as failed to be
  // covered by a recent hints fetcher as they have expired.
  SimulateNavigation(hosts[0]);
  SimulateNavigation(hosts[1]);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.WasHostCoveredByFetch", false, 2);

  // The first pair of hosts should be removed from the dictionary
  // pref as they have expired.
  DictionaryPrefUpdate hosts_fetched(
      pref_service(), prefs::kHintsFetcherHostsSuccessfullyFetched);
  EXPECT_EQ(2u, hosts_fetched->size());

  // Navigations to the valid hosts should be recorded as successfully
  // covered.
  SimulateNavigation(hosts_valid[0]);
  SimulateNavigation(hosts_valid[1]);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintsFetcher.WasHostCoveredByFetch", true, 2);
}

TEST_F(HintsFetcherTest, HintsFetcherHostNotCovered) {
  base::HistogramTester histogram_tester;
  std::vector<std::string> hosts{"host1.com", "host2.com"};
  base::Time host_invalid_time =
      base::Time::Now() + base::TimeDelta().FromHours(1);

  SeedCoveredHosts(hosts, host_invalid_time);
  DictionaryPrefUpdate hosts_fetched(
      pref_service(), prefs::kHintsFetcherHostsSuccessfullyFetched);
  EXPECT_EQ(2u, hosts_fetched->size());

  SimulateNavigation(hosts[0]);
  SimulateNavigation(hosts[1]);
  SimulateNavigation("newhost.com");
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintsFetcher.WasHostCoveredByFetch", true, 2);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintsFetcher.WasHostCoveredByFetch", false, 1);
}

TEST_F(HintsFetcherTest, HintsFetcherRemoveExpiredOnSuccessfullyFetched) {
  base::HistogramTester histogram_tester;
  std::string response_content;
  std::vector<std::string> hosts_expired{"host1.com", "host2.com"};
  base::Time host_invalid_time =
      GetMockClock()->Now() - base::TimeDelta().FromHours(1);

  SeedCoveredHosts(hosts_expired, host_invalid_time);

  std::vector<std::string> hosts_valid{"host3.com", "host4.com"};

  EXPECT_TRUE(FetchHints(hosts_valid));
  VerifyHasPendingFetchRequests();
  EXPECT_TRUE(SimulateResponse(response_content, net::HTTP_OK));
  EXPECT_TRUE(hints_fetched());

  // The two expired hosts should be removed from the dictionary pref as they
  // have expired.
  DictionaryPrefUpdate hosts_fetched(
      pref_service(), prefs::kHintsFetcherHostsSuccessfullyFetched);
  EXPECT_EQ(2u, hosts_fetched->size());

  SimulateNavigation(hosts_expired[0]);
  SimulateNavigation(hosts_expired[1]);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintsFetcher.WasHostCoveredByFetch", false, 2);

  SimulateNavigation(hosts_valid[0]);
  SimulateNavigation(hosts_valid[1]);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.HintsFetcher.WasHostCoveredByFetch", true, 2);
}

TEST_F(HintsFetcherTest, HintsFetcherSuccessfullyFetchedHostsFull) {
  base::HistogramTester histogram_tester;
  std::string response_content;
  std::vector<std::string> hosts;
  size_t max_hosts =
      optimization_guide::features::MaxHostsForRecordingSuccessfullyCovered();
  for (size_t i = 0; i < max_hosts - 1; i++) {
    hosts.push_back("host" + base::NumberToString(i) + ".com");
  }
  base::Time host_expiry_time =
      GetMockClock()->Now() + base::TimeDelta().FromHours(1);

  SeedCoveredHosts(hosts, host_expiry_time);

  std::vector<std::string> extra_hosts{"extra1.com", "extra2.com"};

  EXPECT_TRUE(FetchHints(extra_hosts));
  VerifyHasPendingFetchRequests();
  EXPECT_TRUE(SimulateResponse(response_content, net::HTTP_OK));
  EXPECT_TRUE(hints_fetched());

  // Navigations to both the extra hosts should be recorded.
  DictionaryPrefUpdate hosts_fetched(
      pref_service(), prefs::kHintsFetcherHostsSuccessfullyFetched);
  EXPECT_EQ(200u, hosts_fetched->size());

  SimulateNavigation(extra_hosts[0]);
  SimulateNavigation(extra_hosts[1]);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.WasHostCoveredByFetch", true, 2);
}

}  // namespace optimization_guide
