// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "content/public/test/browser_test.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/test/base/android/android_browser_test.h"
#else
#include "chrome/test/base/in_process_browser_test.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace {

// Callback from changing whether reporting is enabled.
void OnMetricsReportingStateChanged(bool* new_state_ptr,
                                    base::OnceClosure run_loop_closure,
                                    bool new_state) {
  *new_state_ptr = new_state;
  std::move(run_loop_closure).Run();
}

// Changes the metrics reporting state to |enabled|. Returns the actual state
// metrics reporting was changed to after completion.
bool ChangeMetricsReporting(bool enabled) {
  bool value_after_change;
  base::RunLoop run_loop;
  ChangeMetricsReportingStateWithReply(
      enabled, base::BindOnce(OnMetricsReportingStateChanged,
                              &value_after_change, run_loop.QuitClosure()));
  run_loop.Run();
  return value_after_change;
}

}  // namespace

class SampledOutClientIdSavedBrowserTest : public PlatformBrowserTest {
 public:
  SampledOutClientIdSavedBrowserTest() = default;

  SampledOutClientIdSavedBrowserTest(
      const SampledOutClientIdSavedBrowserTest&) = delete;
  SampledOutClientIdSavedBrowserTest& operator=(
      const SampledOutClientIdSavedBrowserTest&) = delete;

  ~SampledOutClientIdSavedBrowserTest() override = default;

  void SetUp() override {
    // Because metrics reporting is disabled in non-Chrome-branded builds,
    // IsMetricsReportingEnabled() always returns false. Enable it here for
    // test consistency between Chromium and Chrome builds, otherwise
    // ChangeMetricsReportingStateWithReply() will not have the intended effects
    // for non-Chrome-branded builds.
    ChromeMetricsServiceAccessor::SetForceIsMetricsReportingEnabledPrefLookup(
        true);

    // Disable |kMetricsReportingFeature| to simulate being sampled out. For
    // Android Chrome, we instead disable |kPostFREFixMetricsReportingFeature|
    // since that is the feature used to verify sampling for clients that newly
    // enable metrics reporting.
#if BUILDFLAG(IS_ANDROID)
    feature_list_.InitAndDisableFeature(
        metrics::internal::kPostFREFixMetricsReportingFeature);
#else
    feature_list_.InitAndDisableFeature(
        metrics::internal::kMetricsReportingFeature);
#endif  // BUILDFLAG(IS_ANDROID)

    PlatformBrowserTest::SetUp();
  }

  metrics::MetricsService* metrics_service() {
    return g_browser_process->GetMetricsServicesManager()->GetMetricsService();
  }

  PrefService* local_state() { return g_browser_process->local_state(); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Verifies that a client ID is written to Local State if metrics reporting is
// turned on—even when the user is sampled out. For Android Chrome, this also
// verifies that clients that have ever went through any of these situations
// should use the post-FRE-fix sampling trial/feature to determine sampling:
// 1) On start up, we determined that they had not consented to metrics
//    reporting (including first run users), or,
// 2) They disabled metrics reporting.
// TODO(crbug.com/1324877): Re-enable this test
#if defined(ADDRESS_SANITIZER)
#define MAYBE_ClientIdSaved DISABLED_ClientIdSaved
#else
#define MAYBE_ClientIdSaved ClientIdSaved
#endif
IN_PROC_BROWSER_TEST_F(SampledOutClientIdSavedBrowserTest,
                       MAYBE_ClientIdSaved) {
  // Verify that the client ID is initially empty.
  ASSERT_TRUE(metrics_service()->GetClientId().empty());
  ASSERT_TRUE(
      local_state()->GetString(metrics::prefs::kMetricsClientID).empty());

#if BUILDFLAG(IS_ANDROID)
  // On Android Chrome, since we have not yet consented to metrics reporting,
  // the new sampling trial should be used to verify sampling.
  EXPECT_TRUE(
      local_state()->GetBoolean(metrics::prefs::kUsePostFREFixSamplingTrial));
#endif  // BUILDFLAG(IS_ANDROID)

  // Verify that we are considered sampled out.
  EXPECT_FALSE(ChromeMetricsServicesManagerClient::IsClientInSample());

  // Enable metrics reporting, and verify that it was successful.
  ASSERT_TRUE(ChangeMetricsReporting(true));
  ASSERT_TRUE(
      local_state()->GetBoolean(metrics::prefs::kMetricsReportingEnabled));

  // Verify that we are still considered sampled out.
  EXPECT_FALSE(ChromeMetricsServicesManagerClient::IsClientInSample());

  // Verify that we are neither recording nor uploading metrics. This also
  // verifies that we are sampled out according to the metrics code, since
  // recording and reporting will not be enabled only if we were sampled out.
  EXPECT_FALSE(metrics_service()->recording_active());
  EXPECT_FALSE(metrics_service()->reporting_active());

  // Verify that the client ID is set and in the Local State.
  std::string client_id = metrics_service()->GetClientId();
  EXPECT_FALSE(client_id.empty());
  EXPECT_EQ(client_id,
            local_state()->GetString(metrics::prefs::kMetricsClientID));

#if BUILDFLAG(IS_ANDROID)
  // Set the pref that dictates whether the new sampling trial should be used to
  // false so that we can verify that upon disabling metrics reporting, this
  // pref is again set to true.
  local_state()->SetBoolean(metrics::prefs::kUsePostFREFixSamplingTrial, false);

  // Disable metrics reporting, and verify that it was successful.
  ASSERT_FALSE(ChangeMetricsReporting(false));
  ASSERT_FALSE(
      local_state()->GetBoolean(metrics::prefs::kMetricsReportingEnabled));

  // Verify that the pref dictating whether we use new sampling trial should be
  // used is set to true.
  EXPECT_TRUE(
      local_state()->GetBoolean(metrics::prefs::kUsePostFREFixSamplingTrial));
#endif  // BUILDFLAG(IS_ANDROID)
}
