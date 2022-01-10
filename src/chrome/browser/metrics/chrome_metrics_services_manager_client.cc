// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"

#include <map>
#include <string>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/chrome_metrics_service_client.h"
#include "chrome/browser/metrics/variations/chrome_variations_service_client.h"
#include "chrome/browser/metrics/variations/ui_string_overrider_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/ui/browser_otr_state.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/metrics/enabled_state_provider.h"
#include "components/metrics/metrics_state_manager.h"
#include "components/prefs/pref_service.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/variations_associated_data.h"
#include "components/version_info/channel.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/metrics/uma_session_stats.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#else
#include "chrome/browser/ui/browser_list.h"
#endif  // defined(OS_ANDROID)

#if defined(OS_WIN)
#include "base/win/registry.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/install_static/install_util.h"
#include "components/crash/core/app/crash_export_thunks.h"
#include "components/crash/core/app/crashpad.h"
#endif  // OS_WIN

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/settings/stats_reporting_controller.h"
#include "components/metrics/structured/neutrino_logging.h"       // nogncheck
#include "components/metrics/structured/neutrino_logging_util.h"  // nogncheck
#include "components/metrics/structured/recorder.h"               // nogncheck

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif

namespace metrics {
namespace internal {

// Metrics reporting feature. This feature, along with user consent, controls if
// recording and reporting are enabled. If the feature is enabled, but no
// consent is given, then there will be no recording or reporting.
const base::Feature kMetricsReportingFeature{"MetricsReporting",
                                             base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace internal
}  // namespace metrics

namespace {

// Name of the variations param that defines the sampling rate.
const char kRateParamName[] = "sampling_rate_per_mille";

// Posts |GoogleUpdateSettings::StoreMetricsClientInfo| on blocking pool thread
// because it needs access to IO and cannot work from UI thread.
void PostStoreMetricsClientInfo(const metrics::ClientInfo& client_info) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&GoogleUpdateSettings::StoreMetricsClientInfo,
                     client_info));
}

// Appends a group to the sampling controlling |trial|. The group will be
// associated with a variation param for reporting sampling |rate| in per mille.
void AppendSamplingTrialGroup(const std::string& group_name,
                              int rate,
                              base::FieldTrial* trial) {
  std::map<std::string, std::string> params = {
      {kRateParamName, base::NumberToString(rate)}};
  variations::AssociateVariationParams(trial->trial_name(), group_name, params);
  trial->AppendGroup(group_name, rate);
}

// Implementation of IsClientInSample() that takes a PrefService param.
bool IsClientInSampleImpl(PrefService* local_state) {
  // Test the MetricsReporting feature for all users to ensure that the trial
  // is reported.
  return base::FeatureList::IsEnabled(
      metrics::internal::kMetricsReportingFeature);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Callback to update the metrics reporting state when the Chrome OS metrics
// reporting setting changes.
void OnCrosMetricsReportingSettingChange() {
  bool enable_metrics = ash::StatsReportingController::Get()->IsEnabled();
  ChangeMetricsReportingState(enable_metrics);

  // TODO(crbug.com/1234538): This call ensures that structured metrics' state
  // is deleted when the reporting state is disabled. Long-term this should
  // happen via a call to all MetricsProviders eg. OnClientStateCleared. This is
  // temporarily called here because it is close to the settings UI, and doesn't
  // greatly affect the logging in crbug.com/1227585.
  auto* recorder = metrics::structured::Recorder::GetInstance();
  if (recorder) {
    recorder->OnReportingStateChanged(enable_metrics);
  }
}
#endif

// Returns the name of a key under HKEY_CURRENT_USER that can be used to store
// backups of metrics data. Unused except on Windows.
std::wstring GetRegistryBackupKey() {
#if defined(OS_WIN)
  return install_static::GetRegistryPath().append(L"\\StabilityMetrics");
#else
  return std::wstring();
#endif
}

}  // namespace


class ChromeMetricsServicesManagerClient::ChromeEnabledStateProvider
    : public metrics::EnabledStateProvider {
 public:
  explicit ChromeEnabledStateProvider(PrefService* local_state)
      : local_state_(local_state) {}

  ChromeEnabledStateProvider(const ChromeEnabledStateProvider&) = delete;
  ChromeEnabledStateProvider& operator=(const ChromeEnabledStateProvider&) =
      delete;

  ~ChromeEnabledStateProvider() override {}

  bool IsConsentGiven() const override {
    return ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled(
        local_state_);
  }

  bool IsReportingEnabled() const override {
    return metrics::EnabledStateProvider::IsReportingEnabled() &&
           IsClientInSampleImpl(local_state_);
  }

 private:
  const raw_ptr<PrefService> local_state_;
};

ChromeMetricsServicesManagerClient::ChromeMetricsServicesManagerClient(
    PrefService* local_state)
    : enabled_state_provider_(
          std::make_unique<ChromeEnabledStateProvider>(local_state)),
      local_state_(local_state) {
  DCHECK(local_state);
}

ChromeMetricsServicesManagerClient::~ChromeMetricsServicesManagerClient() {}

metrics::MetricsStateManager*
ChromeMetricsServicesManagerClient::GetMetricsStateManagerForTesting() {
  return GetMetricsStateManager();
}

// static
void ChromeMetricsServicesManagerClient::CreateFallbackSamplingTrial(
    version_info::Channel channel,
    base::FeatureList* feature_list) {
  // The trial name must be kept in sync with the server config controlling
  // sampling. If they don't match, then clients will be shuffled into different
  // groups when the server config takes over from the fallback trial.
  static const char kTrialName[] = "MetricsAndCrashSampling";
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          kTrialName, 1000, "Default", base::FieldTrial::ONE_TIME_RANDOMIZED,
          nullptr));

  // On all channels except stable, we sample out at a minimal rate to ensure
  // the code paths are exercised in the wild before hitting stable.
  int sampled_in_rate = 990;
  int sampled_out_rate = 10;
  if (channel == version_info::Channel::STABLE) {
    sampled_in_rate = 100;
    sampled_out_rate = 900;
  }

  // Like the trial name, the order that these two groups are added to the trial
  // must be kept in sync with the order that they appear in the server config.
  // For future sanity purposes, the desired order is:
  // OutOfReportingSample, InReportingSample

  static const char kSampledOutGroup[] = "OutOfReportingSample";
  AppendSamplingTrialGroup(kSampledOutGroup, sampled_out_rate, trial.get());

  static const char kInSampleGroup[] = "InReportingSample";
  AppendSamplingTrialGroup(kInSampleGroup, sampled_in_rate, trial.get());

  // Setup the feature. This must be done after all groups are added since
  // GetGroupNameWithoutActivation() will finalize the group choice.
  const std::string& group_name = trial->GetGroupNameWithoutActivation();
  feature_list->RegisterFieldTrialOverride(
      metrics::internal::kMetricsReportingFeature.name,
      group_name == kSampledOutGroup
          ? base::FeatureList::OVERRIDE_DISABLE_FEATURE
          : base::FeatureList::OVERRIDE_ENABLE_FEATURE,
      trial.get());
}

// static
bool ChromeMetricsServicesManagerClient::IsClientInSample() {
  return IsClientInSampleImpl(g_browser_process->local_state());
}

// static
bool ChromeMetricsServicesManagerClient::GetSamplingRatePerMille(int* rate) {
  std::string rate_str = variations::GetVariationParamValueByFeature(
      metrics::internal::kMetricsReportingFeature, kRateParamName);
  if (rate_str.empty())
    return false;

  if (!base::StringToInt(rate_str, rate) || *rate > 1000)
    return false;

  return true;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void ChromeMetricsServicesManagerClient::OnCrosSettingsCreated() {
  reporting_setting_subscription_ =
      ash::StatsReportingController::Get()->AddObserver(
          base::BindRepeating(&OnCrosMetricsReportingSettingChange));
  // Invoke the callback once initially to set the metrics reporting state.
  OnCrosMetricsReportingSettingChange();
}
#endif

const metrics::EnabledStateProvider&
ChromeMetricsServicesManagerClient::GetEnabledStateProviderForTesting() {
  return *enabled_state_provider_;
}

std::unique_ptr<variations::VariationsService>
ChromeMetricsServicesManagerClient::CreateVariationsService() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  metrics::structured::NeutrinoDevicesLogWithLocalState(
      local_state_,
      metrics::structured::NeutrinoDevicesLocation::kCreateVariationsService);
#endif
  return variations::VariationsService::Create(
      std::make_unique<ChromeVariationsServiceClient>(), local_state_,
      GetMetricsStateManager(), switches::kDisableBackgroundNetworking,
      chrome_variations::CreateUIStringOverrider(),
      base::BindOnce(&content::GetNetworkConnectionTracker));
}

std::unique_ptr<metrics::MetricsServiceClient>
ChromeMetricsServicesManagerClient::CreateMetricsServiceClient() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  metrics::structured::NeutrinoDevicesLogWithLocalState(
      local_state_, metrics::structured::NeutrinoDevicesLocation::
                        kCreateMetricsServiceClient);
#endif
  return ChromeMetricsServiceClient::Create(GetMetricsStateManager());
}

metrics::MetricsStateManager*
ChromeMetricsServicesManagerClient::GetMetricsStateManager() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (!metrics_state_manager_) {
    base::FilePath user_data_dir;
    base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);

    metrics::StartupVisibility startup_visibility;
#if defined(OS_ANDROID)
    startup_visibility = UmaSessionStats::HasVisibleActivity()
                             ? metrics::StartupVisibility::kForeground
                             : metrics::StartupVisibility::kBackground;
#else
    startup_visibility = metrics::StartupVisibility::kForeground;
#endif  // defined(OS_ANDROID)

    std::string client_id;
#if BUILDFLAG(IS_CHROMEOS_LACROS)
    // Read metrics service client id from ash chrome if it's present.
    auto* init_params = chromeos::LacrosService::Get()->init_params();
    if (init_params->metrics_service_client_id.has_value())
      client_id = init_params->metrics_service_client_id.value();
#endif

    metrics_state_manager_ = metrics::MetricsStateManager::Create(
        local_state_, enabled_state_provider_.get(), GetRegistryBackupKey(),
        user_data_dir, startup_visibility, chrome::GetChannel(),
        base::BindRepeating(&PostStoreMetricsClientInfo),
        base::BindRepeating(&GoogleUpdateSettings::LoadMetricsClientInfo),
        client_id);
  }
  return metrics_state_manager_.get();
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromeMetricsServicesManagerClient::GetURLLoaderFactory() {
  return g_browser_process->system_network_context_manager()
      ->GetSharedURLLoaderFactory();
}

bool ChromeMetricsServicesManagerClient::IsMetricsReportingEnabled() {
  return enabled_state_provider_->IsReportingEnabled();
}

bool ChromeMetricsServicesManagerClient::IsMetricsConsentGiven() {
  return enabled_state_provider_->IsConsentGiven();
}

bool ChromeMetricsServicesManagerClient::IsOffTheRecordSessionActive() {
#if defined(OS_ANDROID)
  // This differs from TabModelList::IsOffTheRecordSessionActive in that it
  // does not ignore TabModels that have no open tabs, because it may be checked
  // before tabs get added to the TabModel. This means it may be more
  // conservative in case unused TabModels are not cleaned up, but it seems to
  // work correctly.
  // TODO(crbug/741888): Check if TabModelList's version can be updated safely.
  // TODO(crbug/1023759): This function should return true for Incognito CCTs.
  for (const TabModel* model : TabModelList::models()) {
    if (model->IsOffTheRecord())
      return true;
  }

  return false;
#else
  // Depending directly on BrowserList, since that is the implementation
  // that we get correct notifications for.
  return BrowserList::IsOffTheRecordBrowserActive();
#endif
}

#if defined(OS_WIN)
void ChromeMetricsServicesManagerClient::UpdateRunningServices(
    bool may_record,
    bool may_upload) {
  // First, set the registry value so that Crashpad will have the sampling state
  // now and for subsequent runs.
  install_static::SetCollectStatsInSample(IsClientInSample());

  // Next, get Crashpad to pick up the sampling state for this session.
  // Crashpad will use the kRegUsageStatsInSample registry value to apply
  // sampling correctly, but may_record already reflects the sampling state.
  // This isn't a problem though, since they will be consistent.
  SetUploadConsent_ExportThunk(may_record && may_upload);
}
#endif  // defined(OS_WIN)
