// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_ACCESSOR_H_
#define CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_ACCESSOR_H_

#include <stdint.h>

#include "base/gtest_prod_util.h"
#include "base/strings/string_piece.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/chrome_browser_field_trials_mobile.h"
#include "chrome/browser/metrics/metrics_reporting_state.h"
#include "chrome/common/metrics.mojom.h"
#include "components/metrics/metrics_service_accessor.h"
#include "ppapi/buildflags/buildflags.h"

class BreadcrumbsStatus;
class ChromeMetricsServiceClient;
class ChromePasswordManagerClient;
class HttpsFirstModeService;
class NavigationMetricsRecorder;
class PrefService;
class Profile;

namespace {
class CrashesDOMHandler;
class FlashDOMHandler;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
class ChromeCameraAppUIDelegate;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace autofill_assistant {
class AssistantFieldTrialUtilChrome;
}  // namespace autofill_assistant

namespace domain_reliability {
class DomainReliabilityServiceFactory;
}

namespace extensions {
class ChromeGuestViewManagerDelegate;
class ChromeMetricsPrivateDelegate;
class FileManagerPrivateIsUMAEnabledFunction;
}

namespace first_run {
class FirstRunMasterPrefsVariationsSeedTest;
}

namespace metrics {
class ChromeOSPerUserMetricsBrowserTestBase;
class UkmConsentParamBrowserTest;
}

namespace heap_profiling {
class BackgroundProfilingTriggers;
}

namespace welcome {
void JoinOnboardingGroup(Profile* profile);
}

namespace safe_browsing {
class ChromeCleanerControllerDelegate;
class ChromeSafeBrowsingUIManagerDelegate;
class DownloadUrlSBClient;
class IncidentReportingService;
class SafeBrowsingService;

namespace internal {
class ReporterRunner;
}  // namespace internal
}  // namespace safe_browsing

namespace settings {
class MetricsReportingHandler;
}

namespace feed {
class FeedServiceBridge;
class FeedServiceDelegateImpl;
class WebFeedSubscriptionCoordinator;
}  // namespace feed

namespace browser_sync {
class DeviceInfoSyncClientImpl;
}  // namespace browser_sync

namespace webauthn {
namespace authenticator {
class IsMetricsAndCrashReportingEnabled;
}
}  // namespace webauthn

// This class limits and documents access to metrics service helper methods.
// Since these methods are private, each user has to be explicitly declared
// as a 'friend' below.
class ChromeMetricsServiceAccessor : public metrics::MetricsServiceAccessor {
 public:
  ChromeMetricsServiceAccessor() = delete;
  ChromeMetricsServiceAccessor(const ChromeMetricsServiceAccessor&) = delete;
  ChromeMetricsServiceAccessor& operator=(const ChromeMetricsServiceAccessor&) =
      delete;

  // This test method is public so tests don't need to befriend this class.

  // If arg is non-null, the value will be returned from future calls to
  // IsMetricsAndCrashReportingEnabled().  Pointer must be valid until
  // it is reset to null here.
  static void SetMetricsAndCrashReportingForTesting(const bool* value);

 private:
  friend class autofill_assistant::AssistantFieldTrialUtilChrome;
  friend class ::CrashesDOMHandler;
  friend class ::FlashDOMHandler;
  friend class BreadcrumbsStatus;
  friend class ChromeBrowserFieldTrials;
  // For ClangPGO.
  friend class ChromeBrowserMainExtraPartsMetrics;
  // For ThreadProfilerConfiguration.
  friend class ChromeBrowserMainParts;
  friend class ChromeContentBrowserClient;
  friend class ChromeMetricsServicesManagerClient;
  friend class DataReductionProxyChromeSettings;
  friend class domain_reliability::DomainReliabilityServiceFactory;
  friend class extensions::ChromeGuestViewManagerDelegate;
  friend class extensions::ChromeMetricsPrivateDelegate;
  friend class extensions::FileManagerPrivateIsUMAEnabledFunction;
  friend void ChangeMetricsReportingStateWithReply(
      bool,
      OnMetricsReportingCallbackType);
  friend void ApplyMetricsReportingPolicy();
  friend class heap_profiling::BackgroundProfilingTriggers;
  friend class settings::MetricsReportingHandler;
  friend class UmaSessionStats;
  friend class safe_browsing::ChromeCleanerControllerDelegate;
  friend class safe_browsing::ChromeSafeBrowsingUIManagerDelegate;
  friend class safe_browsing::DownloadUrlSBClient;
  friend class safe_browsing::IncidentReportingService;
  friend class safe_browsing::internal::ReporterRunner;
  friend class safe_browsing::SafeBrowsingService;
  friend class ChromeMetricsServiceClient;
  friend class ChromePasswordManagerClient;
  friend void welcome::JoinOnboardingGroup(Profile* profile);
  friend class NavigationMetricsRecorder;
  friend class ChromeBrowserMainExtraPartsGpu;
  friend class Browser;
  friend class OptimizationGuideKeyedService;
  friend class WebUITabStripFieldTrial;
  friend class feed::FeedServiceDelegateImpl;
  friend class browser_sync::DeviceInfoSyncClientImpl;
  friend class feed::FeedServiceBridge;
  friend class feed::WebFeedSubscriptionCoordinator;
  friend class HttpsFirstModeService;
  friend class webauthn::authenticator::IsMetricsAndCrashReportingEnabled;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  friend class ChromeCameraAppUIDelegate;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // For RegisterSyntheticFieldTrial.
  friend class FieldTrialObserver;
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

  // Testing related friends.
  friend class first_run::FirstRunMasterPrefsVariationsSeedTest;
  friend class ForceFieldTrialsBrowserTest;
  friend class MetricsReportingStateTest;
  friend class metrics::UkmConsentParamBrowserTest;
  friend class ClonedInstallClientIdResetBrowserTest;
  friend class metrics::ChromeOSPerUserMetricsBrowserTestBase;
  FRIEND_TEST_ALL_PREFIXES(ChromeMetricsServiceAccessorTest,
                           MetricsReportingEnabled);
  FRIEND_TEST_ALL_PREFIXES(ChromeMetricsServicesManagerClientTest,
                           ForceTrialsDisablesReporting);

  // Returns true if metrics reporting is enabled. This does NOT necessary mean
  // that it is active as configuration may prevent it on some devices (i.e.
  // the "MetricsReporting" field trial that controls sampling). To include
  // that, call: metrics_services_manager->IsReportingEnabled().
  //
  // For Ash Chrome, if a user is logged in and the device has an owner or is
  // managed, the current user's consent (if applicable) will be used if metrics
  // reporting for the device has been enabled.
  //
  // TODO(gayane): Consolidate metric prefs on all platforms.
  // http://crbug.com/362192,  http://crbug.com/532084
  static bool IsMetricsAndCrashReportingEnabled();

  // This is identical to the function without the |local_state| param but can
  // be called before |g_browser_process| has been created by specifying the
  // Local State pref service.
  static bool IsMetricsAndCrashReportingEnabled(PrefService* local_state);

  // Calls metrics::MetricsServiceAccessor::RegisterSyntheticFieldTrial() with
  // g_browser_process->metrics_service(). See that function's declaration for
  // details.
  static bool RegisterSyntheticFieldTrial(base::StringPiece trial_name,
                                          base::StringPiece group_name);

  // Cover for function of same name in MetricsServiceAccssor. See
  // ChromeMetricsServiceAccessor for details.
  static void SetForceIsMetricsReportingEnabledPrefLookup(bool value);

#if BUILDFLAG(ENABLE_PLUGINS)
  // Provides an implementation of chrome::mojom::MetricsService.
  static void BindMetricsServiceReceiver(
      mojo::PendingReceiver<chrome::mojom::MetricsService> receiver);
#endif  // BUILDFLAG(ENABLE_PLUGINS)
};

#endif  // CHROME_BROWSER_METRICS_CHROME_METRICS_SERVICE_ACCESSOR_H_
