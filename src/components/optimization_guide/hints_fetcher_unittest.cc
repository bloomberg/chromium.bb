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
        features::kRemoteOptimizationGuideFetching, {});

    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    prefs::RegisterProfilePrefs(pref_service_->registry());

    hints_fetcher_ = std::make_unique<HintsFetcher>(
        shared_url_loader_factory_, GURL(optimization_guide_service_url),
        pref_service_.get());
    hints_fetcher_->SetTimeClockForTesting(task_environment_.GetMockClock());
  }

  ~HintsFetcherTest() override = default;

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

  // Updates the pref so that hints for each of the host in |hosts| are set to
  // expire at |host_invalid_time|.
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

  void SetTimeClockForTesting(base::Clock* clock) {
    hints_fetcher_->SetTimeClockForTesting(clock);
  }

 protected:
  bool FetchHints(const std::vector<std::string>& hosts,
                  const std::vector<GURL>& urls) {
    bool status = hints_fetcher_->FetchOptimizationGuideServiceHints(
        hosts, urls, {optimization_guide::proto::NOSCRIPT},
        optimization_guide::proto::CONTEXT_BATCH_UPDATE,
        base::BindOnce(&HintsFetcherTest::OnHintsFetched,
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
      EXPECT_EQ(pending_request.request.request_body->elements()->size(), 1u);
      auto& element =
          pending_request.request.request_body->elements_mutable()->front();
      last_request_body_ = std::string(element.bytes(), element.length());
    }
  }

  bool WasHostCoveredByFetch(const std::string& host) {
    return HintsFetcher::WasHostCoveredByFetch(pref_service(), host,
                                               GetMockClock());
  }

  void ResetHintsFetcher() { hints_fetcher_.reset(); }

  std::string last_request_body() const { return last_request_body_; }

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

  std::string last_request_body_;

  DISALLOW_COPY_AND_ASSIGN(HintsFetcherTest);
};

TEST_F(HintsFetcherTest,
       FetchOptimizationGuideServiceHintsLogsHistogramUponExiting) {
  base::HistogramTester histogram_tester;

  EXPECT_TRUE(FetchHints({"foo.com"}, {} /* urls */));
  VerifyHasPendingFetchRequests();
  ResetHintsFetcher();

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.ActiveRequestCanceled."
      "BatchUpdate",
      1, 1);
}

TEST_F(HintsFetcherTest, FetchOptimizationGuideServiceHints) {
  base::HistogramTester histogram_tester;

  std::string response_content;
  EXPECT_TRUE(FetchHints({"foo.com"}, {} /* urls */));
  VerifyHasPendingFetchRequests();
  EXPECT_TRUE(SimulateResponse(response_content, net::HTTP_OK));
  EXPECT_TRUE(hints_fetched());

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.FetchLatency", 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.FetchLatency.BatchUpdate",
      1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.RequestStatus.BatchUpdate",
      HintsFetcherRequestStatus::kSuccess, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.ActiveRequestCanceled."
      "BatchUpdate",
      0);
}

// Tests to ensure that multiple hint fetches by the same object cannot be in
// progress simultaneously.
TEST_F(HintsFetcherTest, FetchInProgress) {
  base::SimpleTestClock test_clock;
  SetTimeClockForTesting(&test_clock);

  // Fetch back to back without waiting for Fetch to complete,
  // |fetch_in_progress_| should cause early exit.
  {
    base::HistogramTester histogram_tester;
    EXPECT_TRUE(FetchHints({"foo.com"}, {} /* urls */));
    EXPECT_FALSE(FetchHints({"bar.com"}, {} /* urls */));
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintsFetcher.RequestStatus.BatchUpdate",
        HintsFetcherRequestStatus::kFetcherBusy, 1);
  }

  // Once response arrives, check to make sure a new fetch can start.
  {
    base::HistogramTester histogram_tester;
    std::string response_content;
    SimulateResponse(response_content, net::HTTP_OK);
    EXPECT_TRUE(FetchHints({"bar.com"}, {} /* urls */));
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.HintsFetcher.RequestStatus.BatchUpdate",
        HintsFetcherRequestStatus::kSuccess, 1);
  }
}

// Tests that the hints are refreshed again for hosts for whom hints were
// fetched recently.
TEST_F(HintsFetcherTest, FetchInProgress_HostsHintsRefreshed) {
  base::SimpleTestClock test_clock;
  SetTimeClockForTesting(&test_clock);

  std::string response_content;
  // Fetch back to back without waiting for Fetch to complete,
  // |fetch_in_progress_| should cause early exit.
  EXPECT_TRUE(FetchHints({"foo.com"}, {} /* urls */));
  EXPECT_FALSE(FetchHints({"foo.com"}, {} /* urls */));

  // Once response arrives, check to make sure that the fetch for the same host
  // is not started again.
  SimulateResponse(response_content, net::HTTP_OK);
  EXPECT_FALSE(FetchHints({"foo.com"}, {} /* urls */));
  // Ensure a new fetch for a different host can start.
  EXPECT_TRUE(FetchHints({"bar.com"}, {} /* urls */));
  SimulateResponse(response_content, net::HTTP_OK);

  EXPECT_FALSE(FetchHints({"foo.com"}, {} /* urls */));
  EXPECT_FALSE(FetchHints({"bar.com"}, {} /* urls */));

  std::vector<std::string> hosts{"foo.com", "bar.com"};
  // Advancing the clock so that it's still one hour before the hints need to be
  // refreshed.
  test_clock.Advance(features::StoredFetchedHintsFreshnessDuration() -
                     features::GetHintsFetchRefreshDuration() -
                     base::TimeDelta().FromHours(1));

  EXPECT_FALSE(FetchHints({"foo.com"}, {} /* urls */));
  EXPECT_FALSE(FetchHints({"bar.com"}, {} /* urls */));

  // Advancing the clock by a little bit more than 1 hour so that the hints are
  // now due for refresh.
  test_clock.Advance(base::TimeDelta::FromMinutes(61));

  EXPECT_TRUE(FetchHints({"foo.com"}, {} /* urls */));
  EXPECT_FALSE(FetchHints({"bar.com"}, {} /* urls */));
  SimulateResponse(response_content, net::HTTP_OK);

  // Hints should not be fetched again for foo.com since they were fetched
  // recently. Hints should still be fetched for bar.com.
  EXPECT_FALSE(FetchHints({"foo.com"}, {} /* urls */));
  EXPECT_TRUE(FetchHints({"bar.com"}, {} /* urls */));
  SimulateResponse(response_content, net::HTTP_OK);

  // Hints should not be fetched again for foo.com and bar.com since they were
  // fetched recently. For baz.com, hints should be fetched again.
  EXPECT_FALSE(FetchHints({"foo.com"}, {} /* urls */));
  EXPECT_FALSE(FetchHints({"bar.com"}, {} /* urls */));
  EXPECT_TRUE(FetchHints({"baz.com"}, {} /* urls */));
  proto::GetHintsResponse response;
  response.mutable_max_cache_duration()->set_seconds(60 * 60 * 24 * 20);
  response.SerializeToString(&response_content);
  SimulateResponse(response_content, net::HTTP_OK);

  // Advance clock for the default duration that the hint normally expires
  // under.
  test_clock.Advance(features::StoredFetchedHintsFreshnessDuration());

  // Max cache duration from response should be used for pref instead.
  EXPECT_FALSE(FetchHints({"baz.com"}, {} /* urls */));
}

// Tests 404 response from request.
TEST_F(HintsFetcherTest, FetchReturned404) {
  base::HistogramTester histogram_tester;

  std::string response_content;

  EXPECT_TRUE(FetchHints({"foo.com"}, {} /* urls */));

  // Send a 404 to HintsFetcher.
  SimulateResponse(response_content, net::HTTP_NOT_FOUND);
  EXPECT_FALSE(hints_fetched());

  // Make sure histograms are recorded correctly on bad response.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.FetchLatency", 0);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.RequestStatus.BatchUpdate",
      HintsFetcherRequestStatus::kResponseError, 1);
}

TEST_F(HintsFetcherTest, FetchReturnBadResponse) {
  base::HistogramTester histogram_tester;

  std::string response_content = "not proto";
  EXPECT_TRUE(FetchHints({"foo.com"}, {} /* urls */));
  VerifyHasPendingFetchRequests();
  EXPECT_TRUE(SimulateResponse(response_content, net::HTTP_OK));
  EXPECT_FALSE(hints_fetched());

  // Make sure histograms are recorded correctly on bad response.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.FetchLatency", 0);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.RequestStatus.BatchUpdate",
      HintsFetcherRequestStatus::kResponseError, 1);
}

TEST_F(HintsFetcherTest, FetchAttemptWhenNetworkOffline) {
  base::HistogramTester histogram_tester;

  SetConnectionOffline();
  std::string response_content;
  EXPECT_FALSE(FetchHints({"foo.com"}, {} /* urls */));
  EXPECT_FALSE(hints_fetched());

  // Make sure histograms are recorded correctly on bad response.
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.FetchLatency", 0);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.RequestStatus.BatchUpdate",
      HintsFetcherRequestStatus::kNetworkOffline, 1);

  SetConnectionOnline();
  EXPECT_TRUE(FetchHints({"foo.com"}, {} /* urls */));
  VerifyHasPendingFetchRequests();
  EXPECT_TRUE(SimulateResponse(response_content, net::HTTP_OK));
  EXPECT_TRUE(hints_fetched());

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.FetchLatency", 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.FetchLatency.BatchUpdate",
      1);
}

TEST_F(HintsFetcherTest, HintsFetchSuccessfulHostsRecorded) {
  std::vector<std::string> hosts{"host1.com", "host2.com"};
  std::string response_content;

  EXPECT_TRUE(FetchHints(hosts, {} /* urls */));
  VerifyHasPendingFetchRequests();
  EXPECT_TRUE(SimulateResponse(response_content, net::HTTP_OK));
  EXPECT_TRUE(hints_fetched());

  const base::DictionaryValue* hosts_fetched = pref_service()->GetDictionary(
      prefs::kHintsFetcherHostsSuccessfullyFetched);
  base::Optional<double> value;
  for (const std::string& host : hosts) {
    value = hosts_fetched->FindDoubleKey(HashHostForDictionary(host));
    // This reduces the necessary precision for the check on the expiry time for
    // the hosts stored in the pref. The exact time is not necessary, being
    // within 10 minutes is acceptable.
    EXPECT_NEAR((base::Time::FromDeltaSinceWindowsEpoch(
                     base::TimeDelta::FromSecondsD(*value)) -
                 GetMockClock()->Now())
                    .InMinutes(),
                base::TimeDelta::FromDays(7).InMinutes(), 10);
  }
}

TEST_F(HintsFetcherTest, HintsFetchFailsHostNotRecorded) {
  std::vector<std::string> hosts{"host1.com", "host2.com"};
  std::string response_content;

  EXPECT_TRUE(FetchHints(hosts, {} /* urls */));
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

  EXPECT_TRUE(FetchHints(hosts, {} /* urls */));
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
  std::vector<std::string> hosts{"host1.com", "host2.com"};
  base::Time host_invalid_time =
      base::Time::Now() + base::TimeDelta().FromHours(1);

  SeedCoveredHosts(hosts, host_invalid_time);

  EXPECT_TRUE(WasHostCoveredByFetch(hosts[0]));
  EXPECT_TRUE(WasHostCoveredByFetch(hosts[1]));
}

TEST_F(HintsFetcherTest, HintsFetcherCoveredHostExpired) {
  std::string response_content;
  std::vector<std::string> hosts{"host1.com", "host2.com"};
  base::Time host_invalid_time =
      GetMockClock()->Now() - base::TimeDelta().FromHours(1);

  SeedCoveredHosts(hosts, host_invalid_time);

  // Fetch hints for new hosts.
  std::vector<std::string> hosts_valid{"host3.com", "hosts4.com"};
  EXPECT_TRUE(FetchHints(hosts_valid, {} /* urls */));
  VerifyHasPendingFetchRequests();
  EXPECT_TRUE(SimulateResponse(response_content, net::HTTP_OK));
  EXPECT_TRUE(hints_fetched());

  // The first pair of hosts should be recorded as failed to be
  // covered by a recent hints fetcher as they have expired.
  EXPECT_FALSE(WasHostCoveredByFetch(hosts[0]));
  EXPECT_FALSE(WasHostCoveredByFetch(hosts[1]));

  // The first pair of hosts should be removed from the dictionary
  // pref as they have expired.
  DictionaryPrefUpdate hosts_fetched(
      pref_service(), prefs::kHintsFetcherHostsSuccessfullyFetched);
  EXPECT_EQ(2u, hosts_fetched->size());

  // Navigations to the valid hosts should be recorded as successfully
  // covered.
  EXPECT_TRUE(WasHostCoveredByFetch(hosts_valid[0]));
  EXPECT_TRUE(WasHostCoveredByFetch(hosts_valid[1]));
}

TEST_F(HintsFetcherTest, HintsFetcherHostNotCovered) {
  std::vector<std::string> hosts{"host1.com", "host2.com"};
  base::Time host_invalid_time =
      base::Time::Now() + base::TimeDelta().FromHours(1);

  SeedCoveredHosts(hosts, host_invalid_time);
  DictionaryPrefUpdate hosts_fetched(
      pref_service(), prefs::kHintsFetcherHostsSuccessfullyFetched);
  EXPECT_EQ(2u, hosts_fetched->size());

  EXPECT_TRUE(WasHostCoveredByFetch(hosts[0]));
  EXPECT_TRUE(WasHostCoveredByFetch(hosts[1]));
  EXPECT_FALSE(WasHostCoveredByFetch("newhost.com"));
}

TEST_F(HintsFetcherTest, HintsFetcherRemoveExpiredOnSuccessfullyFetched) {
  std::string response_content;
  std::vector<std::string> hosts_expired{"host1.com", "host2.com"};
  base::Time host_invalid_time =
      GetMockClock()->Now() - base::TimeDelta().FromHours(1);

  SeedCoveredHosts(hosts_expired, host_invalid_time);

  std::vector<std::string> hosts_valid{"host3.com", "host4.com"};

  EXPECT_TRUE(FetchHints(hosts_valid, {} /* urls */));
  VerifyHasPendingFetchRequests();
  EXPECT_TRUE(SimulateResponse(response_content, net::HTTP_OK));
  EXPECT_TRUE(hints_fetched());

  // The two expired hosts should be removed from the dictionary pref as they
  // have expired.
  DictionaryPrefUpdate hosts_fetched(
      pref_service(), prefs::kHintsFetcherHostsSuccessfullyFetched);
  EXPECT_EQ(2u, hosts_fetched->size());

  EXPECT_FALSE(WasHostCoveredByFetch(hosts_expired[0]));
  EXPECT_FALSE(WasHostCoveredByFetch(hosts_expired[1]));

  EXPECT_TRUE(WasHostCoveredByFetch(hosts_valid[0]));
  EXPECT_TRUE(WasHostCoveredByFetch(hosts_valid[1]));
}

TEST_F(HintsFetcherTest, HintsFetcherSuccessfullyFetchedHostsFull) {
  std::string response_content;
  std::vector<std::string> hosts;
  size_t max_hosts =
      optimization_guide::features::MaxHostsForRecordingSuccessfullyCovered();
  for (size_t i = 0; i < max_hosts - 1; ++i) {
    hosts.push_back("host" + base::NumberToString(i) + ".com");
  }
  base::Time host_expiry_time =
      GetMockClock()->Now() + base::TimeDelta().FromHours(1);

  SeedCoveredHosts(hosts, host_expiry_time);

  std::vector<std::string> extra_hosts{"extra1.com", "extra2.com"};

  EXPECT_TRUE(FetchHints(extra_hosts, {} /* urls */));
  VerifyHasPendingFetchRequests();
  EXPECT_TRUE(SimulateResponse(response_content, net::HTTP_OK));
  EXPECT_TRUE(hints_fetched());

  // Navigations to both the extra hosts should be recorded.
  DictionaryPrefUpdate hosts_fetched(
      pref_service(), prefs::kHintsFetcherHostsSuccessfullyFetched);
  EXPECT_EQ(200u, hosts_fetched->size());

  EXPECT_TRUE(WasHostCoveredByFetch(extra_hosts[0]));
  EXPECT_TRUE(WasHostCoveredByFetch(extra_hosts[1]));
}

TEST_F(HintsFetcherTest, MaxHostsForOptimizationGuideServiceHintsFetch) {
  std::string response_content;
  std::vector<std::string> all_hosts;

  // Invalid hosts, IP addresses, and localhosts should be skipped.
  all_hosts.push_back("localhost");
  all_hosts.push_back("8.8.8.8");
  all_hosts.push_back("probably%20not%20Canonical");

  size_t max_hosts_in_fetch_request = optimization_guide::features::
      MaxHostsForOptimizationGuideServiceHintsFetch();
  for (size_t i = 0; i < max_hosts_in_fetch_request; ++i) {
    all_hosts.push_back("host" + base::NumberToString(i) + ".com");
  }

  all_hosts.push_back("extra1.com");
  all_hosts.push_back("extra2.com");

  EXPECT_TRUE(FetchHints(all_hosts, {} /* urls */));
  VerifyHasPendingFetchRequests();
  EXPECT_TRUE(SimulateResponse(response_content, net::HTTP_OK));
  EXPECT_TRUE(hints_fetched());

  DictionaryPrefUpdate hosts_fetched(
      pref_service(), prefs::kHintsFetcherHostsSuccessfullyFetched);
  EXPECT_EQ(max_hosts_in_fetch_request, hosts_fetched->size());
  EXPECT_EQ(all_hosts.size(), max_hosts_in_fetch_request + 5);

  for (size_t i = 0; i < max_hosts_in_fetch_request; ++i) {
    EXPECT_TRUE(
        WasHostCoveredByFetch("host" + base::NumberToString(i) + ".com"));
  }
}

TEST_F(HintsFetcherTest, MaxUrlsForOptimizationGuideServiceHintsFetch) {
  base::HistogramTester histogram_tester;
  std::string response_content;
  std::vector<GURL> all_urls;

  // IP addresses, and localhosts should be skipped.
  all_urls.push_back(GURL("localhost"));
  all_urls.push_back(GURL("8.8.8.8"));

  size_t max_urls_in_fetch_request = optimization_guide::features::
      MaxUrlsForOptimizationGuideServiceHintsFetch();
  for (size_t i = 0; i < max_urls_in_fetch_request; ++i) {
    all_urls.push_back(GURL("https://url" + base::NumberToString(i) + ".com/"));
  }

  all_urls.push_back(GURL("https://notfetched.com/"));
  all_urls.push_back(GURL("https://notfetched-2.com/"));

  EXPECT_TRUE(FetchHints({} /* hosts */, all_urls));
  VerifyHasPendingFetchRequests();
  EXPECT_TRUE(SimulateResponse(response_content, net::HTTP_OK));
  EXPECT_TRUE(hints_fetched());

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.UrlCount",
      max_urls_in_fetch_request, 1);

  proto::GetHintsRequest last_request;
  last_request.ParseFromString(last_request_body());
  EXPECT_EQ(static_cast<size_t>(last_request.urls_size()),
            max_urls_in_fetch_request);
  for (size_t i = 0; i < max_urls_in_fetch_request; ++i) {
    EXPECT_EQ(last_request.urls(i).url(),
              "https://url" + base::NumberToString(i) + ".com/");
  }
}

TEST_F(HintsFetcherTest, OnlyURLsToFetch) {
  base::HistogramTester histogram_tester;
  std::string response_content;

  EXPECT_TRUE(FetchHints({}, {GURL("https://baz.com/r/werd")}));
  VerifyHasPendingFetchRequests();
  EXPECT_TRUE(SimulateResponse(response_content, net::HTTP_OK));
  EXPECT_TRUE(hints_fetched());

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.FetchLatency", 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.HintsFetcher.GetHintsRequest.FetchLatency.BatchUpdate",
      1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.RequestStatus.BatchUpdate",
      static_cast<int>(HintsFetcherRequestStatus::kSuccess), 1);
}

TEST_F(HintsFetcherTest, NoHostsOrURLsToFetch) {
  base::HistogramTester histogram_tester;
  std::string response_content;

  EXPECT_FALSE(FetchHints({} /* hosts */, {} /* urls */));
  EXPECT_FALSE(hints_fetched());
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.HintsFetcher.RequestStatus.BatchUpdate",
      static_cast<int>(HintsFetcherRequestStatus::kNoHostsOrURLsToFetch), 1);
}

}  // namespace optimization_guide
