// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/feature_list_creator.h"

#include "base/base_switches.h"
#include "base/debug/leak_annotations.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/prefs/pref_service.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/variations_crash_keys.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/common/content_switch_dependent_feature_overrides.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "weblayer/browser/android/metrics/weblayer_metrics_service_client.h"
#include "weblayer/browser/system_network_context_manager.h"
#include "weblayer/browser/weblayer_variations_service_client.h"

#if defined(OS_ANDROID)
namespace switches {
const char kDisableBackgroundNetworking[] = "disable-background-networking";
}  // namespace switches
#endif

namespace weblayer {
namespace {

FeatureListCreator* feature_list_creator_instance = nullptr;

}  // namespace

FeatureListCreator::FeatureListCreator(PrefService* local_state)
    : local_state_(local_state) {
  DCHECK(local_state_);
  DCHECK(!feature_list_creator_instance);
  feature_list_creator_instance = this;
}

FeatureListCreator::~FeatureListCreator() {
  feature_list_creator_instance = nullptr;
}

// static
FeatureListCreator* FeatureListCreator::GetInstance() {
  DCHECK(feature_list_creator_instance);
  return feature_list_creator_instance;
}

void FeatureListCreator::SetSystemNetworkContextManager(
    SystemNetworkContextManager* system_network_context_manager) {
  system_network_context_manager_ = system_network_context_manager;
}

void FeatureListCreator::SetUpFieldTrials() {
#if defined(OS_ANDROID)
  auto* metrics_client = WebLayerMetricsServiceClient::GetInstance();

  // Initialize FieldTrialList to support FieldTrials. If an instance already
  // exists, this is likely a test scenario with a ScopedFeatureList active,
  // so use that one to apply any overrides.
  if (!base::FieldTrialList::GetInstance()) {
    // Note: This is intentionally leaked since it needs to live for the
    // duration of the browser process and there's no benefit in cleaning it up
    // at exit.
    // Note: We deliberately use CreateLowEntropyProvider because
    // CreateDefaultEntropyProvider needs to know if user conset has been given
    // but getting consent from GMS is slow.
    base::FieldTrialList* leaked_field_trial_list = new base::FieldTrialList(
        metrics_client->metrics_state_manager()->CreateLowEntropyProvider());
    ANNOTATE_LEAKING_OBJECT_PTR(leaked_field_trial_list);
    ignore_result(leaked_field_trial_list);
  }

  DCHECK(system_network_context_manager_);
  variations_service_ = variations::VariationsService::Create(
      std::make_unique<WebLayerVariationsServiceClient>(
          system_network_context_manager_),
      local_state_, metrics_client->metrics_state_manager(),
      switches::kDisableBackgroundNetworking, variations::UIStringOverrider(),
      base::BindOnce(&content::GetNetworkConnectionTracker));
  variations_service_->OverridePlatform(
      variations::Study::PLATFORM_ANDROID_WEBLAYER, "android_weblayer");

  std::set<std::string> unforceable_field_trials;
  std::vector<std::string> variation_ids;
  auto feature_list = std::make_unique<base::FeatureList>();

  variations_service_->SetupFieldTrials(
      cc::switches::kEnableGpuBenchmarking, switches::kEnableFeatures,
      switches::kDisableFeatures, unforceable_field_trials, variation_ids,
      content::GetSwitchDependentFeatureOverrides(
          *base::CommandLine::ForCurrentProcess()),
      std::move(feature_list), &weblayer_field_trials_);
  variations::InitCrashKeys();
#else
  // TODO(weblayer-dev): Support variations on desktop.
#endif
}

void FeatureListCreator::CreateFeatureListAndFieldTrials() {
#if defined(OS_ANDROID)
  WebLayerMetricsServiceClient::GetInstance()->Initialize(local_state_);
#endif
  SetUpFieldTrials();
}

void FeatureListCreator::PerformPreMainMessageLoopStartup() {
#if defined(OS_ANDROID)
  // It is expected this is called after SetUpFieldTrials().
  DCHECK(variations_service_);
  variations_service_->PerformPreMainMessageLoopStartup();
#endif
}

void FeatureListCreator::OnBrowserFragmentStarted() {
  if (has_browser_fragment_started_)
    return;

  has_browser_fragment_started_ = true;
  // It is expected this is called after SetUpFieldTrials().
  DCHECK(variations_service_);

  // This function is called any time a BrowserFragment is started.
  // OnAppEnterForeground() really need only be called once, and because our
  // notion of a fragment doesn't really map to the Application as a whole,
  // call this function once.
  variations_service_->OnAppEnterForeground();
}

}  // namespace weblayer
