// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/mojo/media_router_mojo_metrics.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/version.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bucket;
using testing::ElementsAre;

namespace media_router {

namespace {

// Tests that |record_cb| records metrics for each MediaRouteProvider in a
// histogram specific to the provider.
void TestRouteResultCodeHistogramsWithProviders(
    base::RepeatingCallback<void(MediaRouteProviderId,
                                 RouteRequestResult::ResultCode)> record_cb,
    MediaRouteProviderId provider1,
    const std::string& histogram_provider1,
    MediaRouteProviderId provider2,
    const std::string& histogram_provider2) {
  base::HistogramTester tester;
  tester.ExpectTotalCount(histogram_provider1, 0);
  tester.ExpectTotalCount(histogram_provider2, 0);

  record_cb.Run(provider1, RouteRequestResult::SINK_NOT_FOUND);
  record_cb.Run(provider2, RouteRequestResult::OK);
  record_cb.Run(provider1, RouteRequestResult::SINK_NOT_FOUND);
  record_cb.Run(provider2, RouteRequestResult::ROUTE_NOT_FOUND);
  record_cb.Run(provider1, RouteRequestResult::OK);

  tester.ExpectTotalCount(histogram_provider1, 3);
  EXPECT_THAT(
      tester.GetAllSamples(histogram_provider1),
      ElementsAre(
          Bucket(static_cast<int>(RouteRequestResult::OK), 1),
          Bucket(static_cast<int>(RouteRequestResult::SINK_NOT_FOUND), 2)));

  tester.ExpectTotalCount(histogram_provider2, 2);
  EXPECT_THAT(
      tester.GetAllSamples(histogram_provider2),
      ElementsAre(
          Bucket(static_cast<int>(RouteRequestResult::OK), 1),
          Bucket(static_cast<int>(RouteRequestResult::ROUTE_NOT_FOUND), 1)));
}

void TestRouteResultCodeHistograms(
    base::RepeatingCallback<void(MediaRouteProviderId,
                                 RouteRequestResult::ResultCode)> record_cb,
    const std::string& base_histogram_name) {
  TestRouteResultCodeHistogramsWithProviders(
      record_cb, MediaRouteProviderId::EXTENSION, base_histogram_name,
      MediaRouteProviderId::WIRED_DISPLAY,
      base_histogram_name + ".WiredDisplay");

  TestRouteResultCodeHistogramsWithProviders(
      record_cb, MediaRouteProviderId::CAST, base_histogram_name + ".Cast",
      MediaRouteProviderId::DIAL, base_histogram_name + ".DIAL");
}

}  // namespace

TEST(MediaRouterMojoMetricsTest, TestGetMediaRouteProviderVersion) {
  const base::Version kBrowserVersion("50.0.2396.71");
  EXPECT_EQ(MediaRouteProviderVersion::SAME_VERSION_AS_CHROME,
            MediaRouterMojoMetrics::GetMediaRouteProviderVersion(
                base::Version("50.0.2396.71"), kBrowserVersion));
  EXPECT_EQ(MediaRouteProviderVersion::SAME_VERSION_AS_CHROME,
            MediaRouterMojoMetrics::GetMediaRouteProviderVersion(
                base::Version("50.0.2100.0"), kBrowserVersion));
  EXPECT_EQ(MediaRouteProviderVersion::SAME_VERSION_AS_CHROME,
            MediaRouterMojoMetrics::GetMediaRouteProviderVersion(
                base::Version("51.0.2117.0"), kBrowserVersion));
  EXPECT_EQ(MediaRouteProviderVersion::ONE_VERSION_BEHIND_CHROME,
            MediaRouterMojoMetrics::GetMediaRouteProviderVersion(
                base::Version("49.0.2138.0"), kBrowserVersion));
  EXPECT_EQ(MediaRouteProviderVersion::MULTIPLE_VERSIONS_BEHIND_CHROME,
            MediaRouterMojoMetrics::GetMediaRouteProviderVersion(
                base::Version("47.0.1134.0"), kBrowserVersion));
  EXPECT_EQ(MediaRouteProviderVersion::UNKNOWN,
            MediaRouterMojoMetrics::GetMediaRouteProviderVersion(
                base::Version("blargh"), kBrowserVersion));
  EXPECT_EQ(MediaRouteProviderVersion::UNKNOWN,
            MediaRouterMojoMetrics::GetMediaRouteProviderVersion(
                base::Version(""), kBrowserVersion));
  EXPECT_EQ(MediaRouteProviderVersion::UNKNOWN,
            MediaRouterMojoMetrics::GetMediaRouteProviderVersion(
                base::Version("-1.0.0.0"), kBrowserVersion));
  EXPECT_EQ(MediaRouteProviderVersion::UNKNOWN,
            MediaRouterMojoMetrics::GetMediaRouteProviderVersion(
                base::Version("0"), kBrowserVersion));
}

TEST(MediaRouterMojoMetricsTest, RecordCreateRouteResultCode) {
  TestRouteResultCodeHistograms(
      base::BindRepeating(&MediaRouterMojoMetrics::RecordCreateRouteResultCode),
      "MediaRouter.Provider.CreateRoute.Result");
}

TEST(MediaRouterMojoMetricsTest, RecordJoinRouteResultCode) {
  TestRouteResultCodeHistograms(
      base::BindRepeating(&MediaRouterMojoMetrics::RecordJoinRouteResultCode),
      "MediaRouter.Provider.JoinRoute.Result");
}

TEST(MediaRouterMojoMetricsTest, RecordTerminateRouteResultCode) {
  TestRouteResultCodeHistograms(
      base::BindRepeating(
          &MediaRouterMojoMetrics::RecordMediaRouteProviderTerminateRoute),
      "MediaRouter.Provider.TerminateRoute.Result");
}

}  // namespace media_router
