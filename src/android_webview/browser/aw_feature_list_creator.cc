// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_feature_list_creator.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "android_webview/browser/aw_browser_context.h"
#include "android_webview/browser/aw_browser_process.h"
#include "android_webview/browser/aw_metrics_service_client_delegate.h"
#include "android_webview/browser/metrics/aw_metrics_service_client.h"
#include "android_webview/browser/variations/variations_seed_loader.h"
#include "android_webview/common/aw_switches.h"
#include "android_webview/proto/aw_variations_seed.pb.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/path_service.h"
#include "base/strings/string_split.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/switches.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/embedder_support/android/metrics/android_metrics_service_client.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/persistent_histograms.h"
#include "components/policy/core/browser/configuration_policy_pref_store.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/in_memory_pref_store.h"
#include "components/prefs/json_pref_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/variations/entropy_provider.h"
#include "components/variations/pref_names.h"
#include "components/variations/service/safe_seed_manager.h"
#include "components/variations/service/variations_service.h"
#include "content/public/common/content_switch_dependent_feature_overrides.h"
#include "net/base/features.h"
#include "net/nqe/pref_names.h"
#include "services/preferences/tracked/segregated_pref_store.h"

namespace android_webview {

namespace {

bool g_signature_verification_enabled = true;

// These prefs go in the JsonPrefStore, and will persist across runs. Other
// prefs go in the InMemoryPrefStore, and will be lost when the process ends.
const char* const kPersistentPrefsAllowlist[] = {
    // Randomly-generated GUID which pseudonymously identifies uploaded metrics.
    metrics::prefs::kMetricsClientID,
    // Random seed value for variation's entropy providers. Used to assign
    // experiment groups.
    metrics::prefs::kMetricsLowEntropySource,
    // File metrics metadata.
    metrics::prefs::kMetricsFileMetricsMetadata,
    // Logged directly in the ChromeUserMetricsExtension proto.
    metrics::prefs::kInstallDate,
    metrics::prefs::kMetricsReportingEnabledTimestamp,
    metrics::prefs::kMetricsSessionID,
    // Logged in system_profile.stability fields.
    metrics::prefs::kStabilityFileMetricsUnsentFilesCount,
    metrics::prefs::kStabilityFileMetricsUnsentSamplesCount,
    metrics::prefs::kStabilityLaunchCount,
    metrics::prefs::kStabilityPageLoadCount,
    metrics::prefs::kStabilityRendererHangCount,
    metrics::prefs::kStabilityRendererLaunchCount,
    // Unsent logs.
    metrics::prefs::kMetricsInitialLogs,
    metrics::prefs::kMetricsOngoingLogs,
    // Unsent logs metadata.
    metrics::prefs::kMetricsInitialLogsMetadata,
    metrics::prefs::kMetricsOngoingLogsMetadata,
    net::nqe::kNetworkQualities,
    // Current and past country codes, to filter variations studies by country.
    variations::prefs::kVariationsCountry,
    variations::prefs::kVariationsPermanentConsistencyCountry,
    // Last variations seed fetch date/time, used for histograms and to
    // determine if the seed is expired.
    variations::prefs::kVariationsLastFetchTime,
    variations::prefs::kVariationsSeedDate,
};

void HandleReadError(PersistentPrefStore::PrefReadError error) {}

base::FilePath GetPrefStorePath() {
  base::FilePath path;
  base::PathService::Get(base::DIR_ANDROID_APP_DATA, &path);
  path = path.Append(FILE_PATH_LITERAL("pref_store"));
  return path;
}

// Adds WebView-specific switch-dependent feature overrides on top of the ones
// from the content layer.
std::vector<base::FeatureList::FeatureOverrideInfo>
GetSwitchDependentFeatureOverrides(const base::CommandLine& command_line) {
  std::vector<base::FeatureList::FeatureOverrideInfo> feature_overrides =
      content::GetSwitchDependentFeatureOverrides(command_line);

  // TODO(chlily): This can be removed when Schemeful Same-Site is enabled by
  // default.
  if (command_line.HasSwitch(switches::kWebViewEnableModernCookieSameSite)) {
    feature_overrides.push_back(
        std::make_pair(std::cref(net::features::kSchemefulSameSite),
                       base::FeatureList::OVERRIDE_ENABLE_FEATURE));
  }

  return feature_overrides;
}

}  // namespace

AwFeatureListCreator::AwFeatureListCreator()
    : aw_field_trials_(std::make_unique<AwFieldTrials>()) {}

AwFeatureListCreator::~AwFeatureListCreator() {}

std::unique_ptr<PrefService> AwFeatureListCreator::CreatePrefService() {
  auto pref_registry = base::MakeRefCounted<user_prefs::PrefRegistrySyncable>();

  AwMetricsServiceClient::RegisterPrefs(pref_registry.get());
  variations::VariationsService::RegisterPrefs(pref_registry.get());

  AwBrowserProcess::RegisterNetworkContextLocalStatePrefs(pref_registry.get());

  PrefServiceFactory pref_service_factory;

  std::set<std::string> persistent_prefs;
  for (const char* const pref_name : kPersistentPrefsAllowlist)
    persistent_prefs.insert(pref_name);

  persistent_prefs.insert(std::string(metrics::prefs::kMetricsLastSeenPrefix) +
                          kBrowserMetricsName);
  persistent_prefs.insert(std::string(metrics::prefs::kMetricsLastSeenPrefix) +
                          metrics::kCrashpadHistogramAllocatorName);

  // SegregatedPrefStore may be validated with a MAC (message authentication
  // code). On Android, the store is protected by app sandboxing, so validation
  // is unnnecessary. Thus validation_delegate is null.
  pref_service_factory.set_user_prefs(base::MakeRefCounted<SegregatedPrefStore>(
      base::MakeRefCounted<InMemoryPrefStore>(),
      base::MakeRefCounted<JsonPrefStore>(GetPrefStorePath()), persistent_prefs,
      mojo::Remote<::prefs::mojom::TrackedPreferenceValidationDelegate>()));

  pref_service_factory.set_managed_prefs(
      base::MakeRefCounted<policy::ConfigurationPolicyPrefStore>(
          browser_policy_connector_.get(),
          browser_policy_connector_->GetPolicyService(),
          browser_policy_connector_->GetHandlerList(),
          policy::POLICY_LEVEL_MANDATORY));

  pref_service_factory.set_read_error_callback(
      base::BindRepeating(&HandleReadError));

  return pref_service_factory.Create(pref_registry);
}

void AwFeatureListCreator::SetUpFieldTrials() {
  auto* metrics_client = AwMetricsServiceClient::GetInstance();

  // Chrome uses the default entropy provider here (rather than low entropy
  // provider). The default provider needs to know whether UMA is enabled, but
  // WebView determines UMA by querying GMS, which is very slow. So WebView
  // always uses the low entropy provider. Both providers guarantee permanent
  // consistency, which is the main requirement. The difference is that the low
  // entropy provider has fewer unique experiment combinations. This is better
  // for privacy (since experiment state doesn't identify users), but also means
  // fewer combinations tested in the wild.
  DCHECK(!field_trial_list_);
  field_trial_list_ = std::make_unique<base::FieldTrialList>(
      metrics_client->CreateLowEntropyProvider());

  // Convert the AwVariationsSeed proto to a SeedResponse object.
  std::unique_ptr<AwVariationsSeed> seed_proto = TakeSeed();
  std::unique_ptr<variations::SeedResponse> seed;
  base::Time seed_date;  // Initializes to null time.
  if (seed_proto) {
    seed = std::make_unique<variations::SeedResponse>();
    seed->data = seed_proto->seed_data();
    seed->signature = seed_proto->signature();
    seed->country = seed_proto->country();
    seed->date = seed_proto->date();
    seed->is_gzip_compressed = seed_proto->is_gzip_compressed();

    // We set the seed fetch time to when the service downloaded the seed rather
    // than base::Time::Now() because we want to compute seed freshness based on
    // the initial download time, which happened in the service at some earlier
    // point.
    seed_date = base::Time::FromJavaTime(seed->date);
  }

  client_ = std::make_unique<AwVariationsServiceClient>();
  auto seed_store = std::make_unique<variations::VariationsSeedStore>(
      local_state_.get(), /*initial_seed=*/std::move(seed),
      /*signature_verification_enabled=*/g_signature_verification_enabled,
      /*use_first_run_prefs=*/false);

  if (!seed_date.is_null())
    seed_store->RecordLastFetchTime(seed_date);

  variations::UIStringOverrider ui_string_overrider;
  variations_field_trial_creator_ =
      std::make_unique<variations::VariationsFieldTrialCreator>(
          local_state_.get(), client_.get(), std::move(seed_store),
          ui_string_overrider);
  variations_field_trial_creator_->OverrideVariationsPlatform(
      variations::Study::PLATFORM_ANDROID_WEBVIEW);

  // Safe Mode is a feature which reverts to a previous variations seed if the
  // current one is suspected to be causing crashes, or preventing new seeds
  // from being downloaded. It's not implemented for WebView because 1) it's
  // difficult for WebView to implement Safe Mode's crash detection, and 2)
  // downloading and disseminating seeds is handled by the WebView service,
  // which itself doesn't support variations; therefore a bad seed shouldn't be
  // able to break seed downloads. See https://crbug.com/801771 for more info.
  variations::SafeSeedManager ignored_safe_seed_manager(true,
                                                        local_state_.get());

  // Populate FieldTrialList. Since low_entropy_provider is null, it will fall
  // back to the provider we previously gave to FieldTrialList, which is a low
  // entropy provider. The X-Client-Data header is not reported on WebView, so
  // we pass an empty object as the |low_entropy_source_value|.
  variations_field_trial_creator_->SetupFieldTrials(
      cc::switches::kEnableGpuBenchmarking, switches::kEnableFeatures,
      switches::kDisableFeatures, std::vector<std::string>(),
      GetSwitchDependentFeatureOverrides(
          *base::CommandLine::ForCurrentProcess()),
      /*low_entropy_provider=*/nullptr, std::make_unique<base::FeatureList>(),
      aw_field_trials_.get(), &ignored_safe_seed_manager,
      /*low_entropy_source_value=*/base::nullopt);
}

void AwFeatureListCreator::CreateLocalState() {
  browser_policy_connector_ = std::make_unique<AwBrowserPolicyConnector>();
  local_state_ = CreatePrefService();
}

void AwFeatureListCreator::CreateFeatureListAndFieldTrials() {
  TRACE_EVENT0("startup",
               "AwFeatureListCreator::CreateFeatureListAndFieldTrials");
  CreateLocalState();
  AwMetricsServiceClient::SetInstance(std::make_unique<AwMetricsServiceClient>(
      std::make_unique<AwMetricsServiceClientDelegate>()));
  AwMetricsServiceClient::GetInstance()->Initialize(local_state_.get());
  SetUpFieldTrials();
}

void AwFeatureListCreator::DisableSignatureVerificationForTesting() {
  g_signature_verification_enabled = false;
}

}  // namespace android_webview
