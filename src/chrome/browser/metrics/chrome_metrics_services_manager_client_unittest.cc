// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_reporting_default_state.h"
#include "components/metrics/metrics_service_accessor.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(ChromeMetricsServicesManagerClientTest, ForceTrialsDisablesReporting) {
  TestingPrefServiceSimple local_state;

  metrics::RegisterMetricsReportingStatePrefs(local_state.registry());

  // First, test with UMA reporting setting defaulting to off.
  local_state.registry()->RegisterBooleanPref(
      metrics::prefs::kMetricsReportingEnabled, false);
  // Force the pref to be used, even in unofficial builds.
  ChromeMetricsServiceAccessor::SetForceIsMetricsReportingEnabledPrefLookup(
      true);

  ChromeMetricsServicesManagerClient client(&local_state);
  const metrics::EnabledStateProvider& provider =
      client.GetEnabledStateProviderForTesting();
  metrics_services_manager::MetricsServicesManagerClient* base_client = &client;

  // The provider and client APIs should agree.
  EXPECT_EQ(provider.IsConsentGiven(), base_client->IsMetricsConsentGiven());
  EXPECT_EQ(provider.IsReportingEnabled(),
            base_client->IsMetricsReportingEnabled());

  // Both consent and reporting should be false.
  EXPECT_FALSE(provider.IsConsentGiven());
  EXPECT_FALSE(provider.IsReportingEnabled());

  // Set the pref to true.
  local_state.SetBoolean(metrics::prefs::kMetricsReportingEnabled, true);

  // The provider and client APIs should agree.
  EXPECT_EQ(provider.IsConsentGiven(), base_client->IsMetricsConsentGiven());
  EXPECT_EQ(provider.IsReportingEnabled(),
            base_client->IsMetricsReportingEnabled());

  // Both consent and reporting should be true.
  EXPECT_TRUE(provider.IsConsentGiven());
  EXPECT_TRUE(provider.IsReportingEnabled());

  // Set --force-fieldtrials= command-line flag, which should disable reporting
  // but not consent.
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kForceFieldTrials, "Foo/Bar");

  // The provider and client APIs should agree.
  EXPECT_EQ(provider.IsConsentGiven(), base_client->IsMetricsConsentGiven());
  EXPECT_EQ(provider.IsReportingEnabled(),
            base_client->IsMetricsReportingEnabled());

  // Consent should be true but reporting should be false.
  EXPECT_TRUE(provider.IsConsentGiven());
  EXPECT_FALSE(provider.IsReportingEnabled());
}
