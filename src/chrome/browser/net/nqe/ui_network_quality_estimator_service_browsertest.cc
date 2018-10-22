// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/io_thread.h"
#include "chrome/browser/net/nqe/ui_network_quality_estimator_service.h"
#include "chrome/browser/net/nqe/ui_network_quality_estimator_service_factory.h"
#include "chrome/browser/net/nqe/ui_network_quality_estimator_service_test_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "net/base/network_change_notifier.h"
#include "net/nqe/cached_network_quality.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_id.h"
#include "net/nqe/network_quality_estimator.h"
#include "testing/gtest/include/gtest/gtest.h"

class Profile;

namespace {

class UINetworkQualityEstimatorServiceBrowserTest
    : public InProcessBrowserTest {
 public:
  UINetworkQualityEstimatorServiceBrowserTest() {}

  // Verifies that the network quality prefs are written amd read correctly.
  void VerifyWritingReadingPrefs() {
    variations::testing::ClearAllVariationParams();
    std::map<std::string, std::string> variation_params;

    variations::AssociateVariationParams("NetworkQualityEstimator", "Enabled",
                                         variation_params);
    base::FieldTrialList::CreateFieldTrial("NetworkQualityEstimator",
                                           "Enabled");

    // Verifies that NQE notifying EffectiveConnectionTypeObservers causes the
    // UINetworkQualityEstimatorService to receive an updated
    // EffectiveConnectionType.
    Profile* profile = ProfileManager::GetActiveUserProfile();

    UINetworkQualityEstimatorService* nqe_service =
        UINetworkQualityEstimatorServiceFactory::GetForProfile(profile);
    ASSERT_NE(nullptr, nqe_service);
    // NetworkQualityEstimator must be notified of the read prefs at startup.
    EXPECT_FALSE(histogram_tester_.GetAllSamples("NQE.Prefs.ReadSize").empty());

    {
      base::HistogramTester histogram_tester;
      nqe_test_util::OverrideEffectiveConnectionTypeAndWait(
          net::EFFECTIVE_CONNECTION_TYPE_OFFLINE);

      // Prefs are written only if persistent caching was enabled.
      EXPECT_FALSE(
          histogram_tester.GetAllSamples("NQE.Prefs.WriteCount").empty());
      histogram_tester.ExpectTotalCount("NQE.Prefs.ReadCount", 0);

      // NetworkQualityEstimator should not be notified of change in prefs.
      histogram_tester.ExpectTotalCount("NQE.Prefs.ReadSize", 0);
    }

    {
      base::HistogramTester histogram_tester;
      nqe_test_util::OverrideEffectiveConnectionTypeAndWait(
          net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G);

      // Prefs are written even if the network id was unavailable.
      EXPECT_FALSE(
          histogram_tester.GetAllSamples("NQE.Prefs.WriteCount").empty());
      histogram_tester.ExpectTotalCount("NQE.Prefs.ReadCount", 0);

      // NetworkQualityEstimator should not be notified of change in prefs.
      histogram_tester.ExpectTotalCount("NQE.Prefs.ReadSize", 0);
    }

    // Verify the contents of the prefs by reading them again.
    std::map<net::nqe::internal::NetworkID,
             net::nqe::internal::CachedNetworkQuality>
        read_prefs = nqe_service->ForceReadPrefsForTesting();
    // Number of entries must be between 1 and 2. It's possible that 2 entries
    // are added if the connection type is unknown to network quality estimator
    // at the time of startup, and shortly after it receives a notification
    // about the change in the connection type.
    EXPECT_LE(1u, read_prefs.size());
    EXPECT_GE(2u, read_prefs.size());

    // Verify that the cached network quality was written correctly.
    EXPECT_EQ(net::EFFECTIVE_CONNECTION_TYPE_SLOW_2G,
              read_prefs.begin()->second.effective_connection_type());
    if (net::NetworkChangeNotifier::GetConnectionType() ==
        net::NetworkChangeNotifier::CONNECTION_ETHERNET) {
      // Verify that the network ID was written correctly.
      net::nqe::internal::NetworkID ethernet_network_id(
          net::NetworkChangeNotifier::CONNECTION_ETHERNET, std::string(),
          INT32_MIN);
      EXPECT_EQ(ethernet_network_id, read_prefs.begin()->first);
      }
  }

 private:
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(UINetworkQualityEstimatorServiceBrowserTest);
};

}  // namespace

// Verify that prefs are writen and read correctly.
IN_PROC_BROWSER_TEST_F(UINetworkQualityEstimatorServiceBrowserTest,
                       WritingReadingToPrefsEnabled) {
  VerifyWritingReadingPrefs();
}
