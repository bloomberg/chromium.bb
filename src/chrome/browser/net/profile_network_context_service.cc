// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/profile_network_context_service.h"

#include <string>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_split.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_chrome_settings_factory.h"
#include "chrome/browser/domain_reliability/service_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths_internal.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/certificate_transparency/pref_names.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/language/core/browser/pref_names.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/common/url_constants.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/features.h"
#include "net/http/http_auth_preferences.h"
#include "net/http/http_util.h"
#include "net/ssl/client_cert_store.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "third_party/blink/public/common/features.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/certificate_provider/certificate_provider.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service.h"
#include "chrome/browser/chromeos/certificate_provider/certificate_provider_service_factory.h"
#include "chrome/browser/chromeos/net/client_cert_store_chromeos.h"
#include "chrome/browser/chromeos/policy/policy_cert_service.h"
#include "chrome/browser/chromeos/policy/policy_cert_service_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chromeos/constants/chromeos_switches.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#endif

#if defined(USE_NSS_CERTS)
#include "chrome/browser/ui/crypto_module_delegate_nss.h"
#include "net/ssl/client_cert_store_nss.h"
#endif  // defined(USE_NSS_CERTS)

#if defined(OS_WIN)
#include "net/ssl/client_cert_store_win.h"
#endif  // defined(OS_WIN)

#if defined(OS_MACOSX)
#include "net/ssl/client_cert_store_mac.h"
#endif  // defined(OS_MACOSX)

#if BUILDFLAG(TRIAL_COMPARISON_CERT_VERIFIER_SUPPORTED)
#include "chrome/browser/net/trial_comparison_cert_verifier_controller.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/constants.h"
#endif

namespace {

bool* g_discard_domain_reliability_uploads_for_testing = nullptr;

const char kHttpCacheFinchExperimentGroups[] =
    "profile_network_context_service.http_cache_finch_experiment_groups";

std::vector<std::string> TranslateStringArray(const base::ListValue* list) {
  std::vector<std::string> strings;
  for (const base::Value& value : *list) {
    DCHECK(value.is_string());
    strings.push_back(value.GetString());
  }
  return strings;
}

std::string ComputeAcceptLanguageFromPref(const std::string& language_pref) {
  std::string accept_languages_str =
      base::FeatureList::IsEnabled(features::kUseNewAcceptLanguageHeader)
          ? net::HttpUtil::ExpandLanguageList(language_pref)
          : language_pref;
  return net::HttpUtil::GenerateAcceptLanguageHeader(accept_languages_str);
}

#if defined(OS_CHROMEOS)
network::mojom::AdditionalCertificatesPtr GetAdditionalCertificates(
    const policy::PolicyCertService* policy_cert_service,
    const base::FilePath& storage_partition_path) {
  auto additional_certificates = network::mojom::AdditionalCertificates::New();
  policy_cert_service->GetPolicyCertificatesForStoragePartition(
      storage_partition_path, &(additional_certificates->all_certificates),
      &(additional_certificates->trust_anchors));
  return additional_certificates;
}
#endif  // defined (OS_CHROMEOS)

void InitializeCorsExtraSafelistedRequestHeaderNamesForProfile(
    Profile* profile,
    std::vector<std::string>* extra_safelisted_request_header_names) {
  PrefService* pref = profile->GetPrefs();
  bool has_managed_mitigation_list =
      pref->IsManagedPreference(prefs::kCorsMitigationList);

  // Set default mitigation parameters managed by the server for normal users
  // and enterprise users separately.
  const char* const feature_param_name =
      has_managed_mitigation_list
          ? "extra-safelisted-request-headers-for-enterprise"
          : "extra-safelisted-request-headers";
  *extra_safelisted_request_header_names =
      SplitString(base::GetFieldTrialParamValueByFeature(
                      features::kExtraSafelistedRequestHeadersForOutOfBlinkCors,
                      feature_param_name),
                  ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // We trust and append |pref|'s values only when they are set by the managed
  // policy. Chrome does not have any interface to set this preference manually.
  if (!base::FeatureList::IsEnabled(
          features::kHideCorsMitigationListPolicySupport) &&
      has_managed_mitigation_list) {
    for (const auto& header_name_value :
         *pref->GetList(prefs::kCorsMitigationList)) {
      extra_safelisted_request_header_names->push_back(
          header_name_value.GetString());
    }
  }
}

// Tests allowing ambient authentication with default credentials based on the
// profile type.
// TODO(https://crbug.com/458508): Currently, this is determined by OR of the
// feature flag and policy. Next steps would be changing the feature value to
// false after enough heads up and then removing the feature.
bool IsAmbientAuthAllowedForProfile(const Profile* profile) {
  if (profile->IsRegularProfile())
    return true;

  PrefService* local_state = g_browser_process->local_state();
  DCHECK(local_state);
  DCHECK(local_state->FindPreference(
      prefs::kAmbientAuthenticationInPrivateModesEnabled));

  net::AmbientAuthAllowedProfileTypes type =
      static_cast<net::AmbientAuthAllowedProfileTypes>(local_state->GetInteger(
          prefs::kAmbientAuthenticationInPrivateModesEnabled));

  if (profile->IsGuestSession()) {
    return base::FeatureList::IsEnabled(
               features::kEnableAmbientAuthenticationInGuestSession) ||
           type == net::AmbientAuthAllowedProfileTypes::GUEST_AND_REGULAR ||
           type == net::AmbientAuthAllowedProfileTypes::ALL;
  } else if (profile->IsIncognitoProfile()) {
    return base::FeatureList::IsEnabled(
               features::kEnableAmbientAuthenticationInIncognito) ||
           type == net::AmbientAuthAllowedProfileTypes::INCOGNITO_AND_REGULAR ||
           type == net::AmbientAuthAllowedProfileTypes::ALL;
  }

  // Profile type not yet supported.
  NOTREACHED();

  return false;
}

}  // namespace

ProfileNetworkContextService::ProfileNetworkContextService(Profile* profile)
    : profile_(profile), proxy_config_monitor_(profile) {
  PrefService* profile_prefs = profile->GetPrefs();
  quic_allowed_.Init(
      prefs::kQuicAllowed, profile_prefs,
      base::Bind(&ProfileNetworkContextService::DisableQuicIfNotAllowed,
                 base::Unretained(this)));
  pref_accept_language_.Init(
      language::prefs::kAcceptLanguages, profile_prefs,
      base::BindRepeating(&ProfileNetworkContextService::UpdateAcceptLanguage,
                          base::Unretained(this)));
  enable_referrers_.Init(
      prefs::kEnableReferrers, profile_prefs,
      base::BindRepeating(&ProfileNetworkContextService::UpdateReferrersEnabled,
                          base::Unretained(this)));
  cookie_settings_ = CookieSettingsFactory::GetForProfile(profile);
  cookie_settings_observer_.Add(cookie_settings_.get());

  DisableQuicIfNotAllowed();

  // Observe content settings so they can be synced to the network service.
  HostContentSettingsMapFactory::GetForProfile(profile_)->AddObserver(this);

  pref_change_registrar_.Init(profile_prefs);

  // When any of the following CT preferences change, we schedule an update
  // to aggregate the actual update using a |ct_policy_update_timer_|.
  pref_change_registrar_.Add(
      certificate_transparency::prefs::kCTRequiredHosts,
      base::BindRepeating(&ProfileNetworkContextService::ScheduleUpdateCTPolicy,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      certificate_transparency::prefs::kCTExcludedHosts,
      base::BindRepeating(&ProfileNetworkContextService::ScheduleUpdateCTPolicy,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      certificate_transparency::prefs::kCTExcludedSPKIs,
      base::BindRepeating(&ProfileNetworkContextService::ScheduleUpdateCTPolicy,
                          base::Unretained(this)));
  pref_change_registrar_.Add(
      certificate_transparency::prefs::kCTExcludedLegacySPKIs,
      base::BindRepeating(&ProfileNetworkContextService::ScheduleUpdateCTPolicy,
                          base::Unretained(this)));

  // Reflects CORS mitigation list policy updates dynamically.
  pref_change_registrar_.Add(
      prefs::kCorsMitigationList,
      base::BindRepeating(
          &ProfileNetworkContextService::UpdateCorsMitigationList,
          base::Unretained(this)));

  pref_change_registrar_.Add(
      prefs::kGloballyScopeHTTPAuthCacheEnabled,
      base::BindRepeating(&ProfileNetworkContextService::
                              UpdateSplitAuthCacheByNetworkIsolationKey,
                          base::Unretained(this)));
}

ProfileNetworkContextService::~ProfileNetworkContextService() = default;

void ProfileNetworkContextService::ConfigureNetworkContextParams(
    bool in_memory,
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextParams* network_context_params,
    network::mojom::CertVerifierCreationParams* cert_verifier_creation_params) {
  ConfigureNetworkContextParamsInternal(in_memory, relative_partition_path,
                                        network_context_params,
                                        cert_verifier_creation_params);

  if ((!in_memory && !profile_->IsOffTheRecord())) {
    // TODO(jam): delete this code 1 year after Network Service shipped to all
    // stable users, which would be after M83 branches.
    base::FilePath base_cache_path;
    chrome::GetUserCacheDirectory(GetPartitionPath(relative_partition_path),
                                  &base_cache_path);
    base::FilePath media_cache_path =
        base_cache_path.Append(chrome::kMediaCacheDirname);
    base::ThreadPool::PostTask(
        FROM_HERE,
        {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(base::IgnoreResult(&base::DeleteFile), media_cache_path,
                       true /* recursive */));
  }
}

#if defined(OS_CHROMEOS)
void ProfileNetworkContextService::UpdateAdditionalCertificates() {
  const policy::PolicyCertService* policy_cert_service =
      policy::PolicyCertServiceFactory::GetForProfile(profile_);
  if (!policy_cert_service)
    return;
  content::BrowserContext::ForEachStoragePartition(
      profile_, base::BindRepeating(
                    [](const policy::PolicyCertService* policy_cert_service,
                       content::StoragePartition* storage_partition) {
                      auto additional_certificates = GetAdditionalCertificates(
                          policy_cert_service, storage_partition->GetPath());
                      storage_partition->GetNetworkContext()
                          ->UpdateAdditionalCertificates(
                              std::move(additional_certificates));
                    },
                    policy_cert_service));
}
#endif

void ProfileNetworkContextService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kQuicAllowed, true);
  registry->RegisterBooleanPref(prefs::kGloballyScopeHTTPAuthCacheEnabled,
                                false);
}

// static
void ProfileNetworkContextService::RegisterLocalStatePrefs(
    PrefRegistrySimple* registry) {
  registry->RegisterListPref(prefs::kHSTSPolicyBypassList);
  registry->RegisterIntegerPref(
      prefs::kAmbientAuthenticationInPrivateModesEnabled,
      static_cast<int>(net::AmbientAuthAllowedProfileTypes::REGULAR_ONLY));

  // For information about whether to reset the HTTP Cache or not, defaults
  // to the empty string, which does not prompt a reset.
  registry->RegisterStringPref(kHttpCacheFinchExperimentGroups, "");
}

void ProfileNetworkContextService::DisableQuicIfNotAllowed() {
  if (!quic_allowed_.IsManaged())
    return;

  // If QUIC is allowed, do nothing (re-enabling QUIC is not supported).
  if (quic_allowed_.GetValue())
    return;

  g_browser_process->system_network_context_manager()->DisableQuic();
}

void ProfileNetworkContextService::UpdateAcceptLanguage() {
  content::BrowserContext::ForEachStoragePartition(
      profile_, base::BindRepeating(
                    [](const std::string& accept_language,
                       content::StoragePartition* storage_partition) {
                      storage_partition->GetNetworkContext()->SetAcceptLanguage(
                          accept_language);
                    },
                    ComputeAcceptLanguage()));
}

void ProfileNetworkContextService::OnThirdPartyCookieBlockingChanged(
    bool block_third_party_cookies) {
  content::BrowserContext::ForEachStoragePartition(
      profile_, base::BindRepeating(
                    [](bool block_third_party_cookies,
                       content::StoragePartition* storage_partition) {
                      storage_partition->GetCookieManagerForBrowserProcess()
                          ->BlockThirdPartyCookies(block_third_party_cookies);
                    },
                    block_third_party_cookies));
}

std::string ProfileNetworkContextService::ComputeAcceptLanguage() const {
  return ComputeAcceptLanguageFromPref(pref_accept_language_.GetValue());
}

void ProfileNetworkContextService::UpdateReferrersEnabled() {
  content::BrowserContext::ForEachStoragePartition(
      profile_,
      base::BindRepeating(
          [](bool enable_referrers,
             content::StoragePartition* storage_partition) {
            storage_partition->GetNetworkContext()->SetEnableReferrers(
                enable_referrers);
          },
          enable_referrers_.GetValue()));
}

network::mojom::CTPolicyPtr ProfileNetworkContextService::GetCTPolicy() {
  auto* prefs = profile_->GetPrefs();
  const base::ListValue* ct_required =
      prefs->GetList(certificate_transparency::prefs::kCTRequiredHosts);
  const base::ListValue* ct_excluded =
      prefs->GetList(certificate_transparency::prefs::kCTExcludedHosts);
  const base::ListValue* ct_excluded_spkis =
      prefs->GetList(certificate_transparency::prefs::kCTExcludedSPKIs);
  const base::ListValue* ct_excluded_legacy_spkis =
      prefs->GetList(certificate_transparency::prefs::kCTExcludedLegacySPKIs);

  std::vector<std::string> required(TranslateStringArray(ct_required));
  std::vector<std::string> excluded(TranslateStringArray(ct_excluded));
  std::vector<std::string> excluded_spkis(
      TranslateStringArray(ct_excluded_spkis));
  std::vector<std::string> excluded_legacy_spkis(
      TranslateStringArray(ct_excluded_legacy_spkis));

  return network::mojom::CTPolicy::New(std::move(required), std::move(excluded),
                                       std::move(excluded_spkis),
                                       std::move(excluded_legacy_spkis));
}

void ProfileNetworkContextService::UpdateCTPolicyForContexts(
    const std::vector<network::mojom::NetworkContext*>& contexts) {
  for (auto* context : contexts) {
    context->SetCTPolicy(GetCTPolicy());
  }
}

void ProfileNetworkContextService::UpdateCTPolicy() {
  std::vector<network::mojom::NetworkContext*> contexts;
  content::BrowserContext::ForEachStoragePartition(
      profile_,
      base::BindRepeating(
          [](std::vector<network::mojom::NetworkContext*>* contexts_ptr,
             content::StoragePartition* storage_partition) {
            contexts_ptr->push_back(storage_partition->GetNetworkContext());
          },
          &contexts));

  UpdateCTPolicyForContexts(contexts);
}

void ProfileNetworkContextService::ScheduleUpdateCTPolicy() {
  ct_policy_update_timer_.Start(FROM_HERE, base::TimeDelta::FromSeconds(0),
                                this,
                                &ProfileNetworkContextService::UpdateCTPolicy);
}

void ProfileNetworkContextService::UpdateCorsMitigationList() {
  std::vector<std::string> cors_extra_safelisted_request_header_names;
  InitializeCorsExtraSafelistedRequestHeaderNamesForProfile(
      profile_, &cors_extra_safelisted_request_header_names);

  content::BrowserContext::ForEachStoragePartition(
      profile_, base::BindRepeating(
                    [](std::vector<std::string>*
                           cors_extra_safelisted_request_header_names,
                       content::StoragePartition* storage_partition) {
                      storage_partition->GetNetworkContext()
                          ->SetCorsExtraSafelistedRequestHeaderNames(
                              *cors_extra_safelisted_request_header_names);
                    },
                    &cors_extra_safelisted_request_header_names));
}

bool ProfileNetworkContextService::ShouldSplitAuthCacheByNetworkIsolationKey()
    const {
  if (profile_->GetPrefs()->GetBoolean(
          prefs::kGloballyScopeHTTPAuthCacheEnabled))
    return false;
  return base::FeatureList::IsEnabled(
      network::features::kSplitAuthCacheByNetworkIsolationKey);
}

void ProfileNetworkContextService::UpdateSplitAuthCacheByNetworkIsolationKey() {
  bool split_auth_cache_by_network_isolation_key =
      ShouldSplitAuthCacheByNetworkIsolationKey();

  content::BrowserContext::ForEachStoragePartition(
      profile_, base::BindRepeating(
                    [](bool split_auth_cache_by_network_isolation_key,
                       content::StoragePartition* storage_partition) {
                      storage_partition->GetNetworkContext()
                          ->SetSplitAuthCacheByNetworkIsolationKey(
                              split_auth_cache_by_network_isolation_key);
                    },
                    split_auth_cache_by_network_isolation_key));
}

// static
network::mojom::CookieManagerParamsPtr
ProfileNetworkContextService::CreateCookieManagerParams(
    Profile* profile,
    const content_settings::CookieSettings& cookie_settings) {
  auto out = network::mojom::CookieManagerParams::New();
  out->block_third_party_cookies =
      cookie_settings.ShouldBlockThirdPartyCookies();
  out->secure_origin_cookies_allowed_schemes.push_back(
      content::kChromeUIScheme);
#if BUILDFLAG(ENABLE_EXTENSIONS)
  // TODO(chlily): To be consistent with the content_settings version of
  // CookieSettings, we should probably also add kExtensionScheme to the list of
  // matching_scheme_cookies_allowed_schemes.
  out->third_party_cookies_allowed_schemes.push_back(
      extensions::kExtensionScheme);
#endif

  ContentSettingsForOneType settings;
  HostContentSettingsMap* host_content_settings_map =
      HostContentSettingsMapFactory::GetForProfile(profile);
  host_content_settings_map->GetSettingsForOneType(ContentSettingsType::COOKIES,
                                                   std::string(), &settings);
  out->settings = std::move(settings);

  ContentSettingsForOneType settings_for_legacy_cookie_access;
  host_content_settings_map->GetSettingsForOneType(
      ContentSettingsType::LEGACY_COOKIE_ACCESS, std::string(),
      &settings_for_legacy_cookie_access);
  out->settings_for_legacy_cookie_access =
      std::move(settings_for_legacy_cookie_access);

  ContentSettingsForOneType settings_for_storage_access;
  if (base::FeatureList::IsEnabled(blink::features::kStorageAccessAPI)) {
    host_content_settings_map->GetSettingsForOneType(
        ContentSettingsType::STORAGE_ACCESS, std::string(),
        &settings_for_storage_access);
  }
  out->settings_for_storage_access = std::move(settings_for_storage_access);

  out->cookie_access_delegate_type =
      network::mojom::CookieAccessDelegateType::USE_CONTENT_SETTINGS;
  return out;
}

void ProfileNetworkContextService::FlushProxyConfigMonitorForTesting() {
  proxy_config_monitor_.FlushForTesting();
}

void ProfileNetworkContextService::SetDiscardDomainReliabilityUploadsForTesting(
    bool value) {
  g_discard_domain_reliability_uploads_for_testing = new bool(value);
}

std::unique_ptr<net::ClientCertStore>
ProfileNetworkContextService::CreateClientCertStore() {
  if (!client_cert_store_factory_.is_null())
    return client_cert_store_factory_.Run();

#if defined(OS_CHROMEOS)
  bool use_system_key_slot = false;
  // Enable client certificates for the Chrome OS sign-in frame, if this feature
  // is not disabled by a flag.
  // Note that while this applies to the whole sign-in profile, client
  // certificates will only be selected for the StoragePartition currently used
  // in the sign-in frame (see SigninPartitionManager).
  if (chromeos::switches::IsSigninFrameClientCertsEnabled() &&
      chromeos::ProfileHelper::IsSigninProfile(profile_)) {
    use_system_key_slot = true;
  }

  std::string username_hash;
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
  if (user && !user->username_hash().empty()) {
    username_hash = user->username_hash();

    // Use the device-wide system key slot only if the user is affiliated on
    // the device.
    if (user->IsAffiliated()) {
      use_system_key_slot = true;
    }
  }

  chromeos::CertificateProviderService* cert_provider_service =
      chromeos::CertificateProviderServiceFactory::GetForBrowserContext(
          profile_);
  std::unique_ptr<chromeos::CertificateProvider> certificate_provider;
  if (cert_provider_service) {
    certificate_provider = cert_provider_service->CreateCertificateProvider();
  }

  // ClientCertStoreChromeOS internally depends on NSS initialization that
  // happens when the ResourceContext is created. Call GetResourceContext() so
  // the dependency is explicit. See https://crbug.com/1018972.
  profile_->GetResourceContext();

  return std::make_unique<chromeos::ClientCertStoreChromeOS>(
      std::move(certificate_provider), use_system_key_slot, username_hash,
      base::Bind(&CreateCryptoModuleBlockingPasswordDelegate,
                 kCryptoModulePasswordClientAuth));
#elif defined(USE_NSS_CERTS)
  return std::make_unique<net::ClientCertStoreNSS>(
      base::Bind(&CreateCryptoModuleBlockingPasswordDelegate,
                 kCryptoModulePasswordClientAuth));
#elif defined(OS_WIN)
  return std::make_unique<net::ClientCertStoreWin>();
#elif defined(OS_MACOSX)
  return std::make_unique<net::ClientCertStoreMac>();
#elif defined(OS_ANDROID)
  // Android does not use the ClientCertStore infrastructure. On Android client
  // cert matching is done by the OS as part of the call to show the cert
  // selection dialog.
  return nullptr;
#else
#error Unknown platform.
#endif
}

bool GetHttpCacheBackendResetParam(PrefService* local_state) {
  // Get the field trial groups.  If the server cannot be reached, then
  // this corresponds to "None" for each experiment.
  base::FieldTrial* field_trial = base::FeatureList::GetFieldTrial(
      net::features::kSplitCacheByNetworkIsolationKey);
  std::string current_field_trial_status =
      (field_trial ? field_trial->group_name() : "None") + " ";
  field_trial = base::FeatureList::GetFieldTrial(
      net::features::kAppendFrameOriginToNetworkIsolationKey);
  current_field_trial_status +=
      (field_trial ? field_trial->group_name() : "None") + " ";
  field_trial = base::FeatureList::GetFieldTrial(
      net::features::kUseRegistrableDomainInNetworkIsolationKey);
  current_field_trial_status +=
      (field_trial ? field_trial->group_name() : "None");

  std::string previous_field_trial_status =
      local_state->GetString(kHttpCacheFinchExperimentGroups);
  local_state->SetString(kHttpCacheFinchExperimentGroups,
                         current_field_trial_status);

  return !previous_field_trial_status.empty() &&
         current_field_trial_status != previous_field_trial_status;
}

void ProfileNetworkContextService::ConfigureNetworkContextParamsInternal(
    bool in_memory,
    const base::FilePath& relative_partition_path,
    network::mojom::NetworkContextParams* network_context_params,
    network::mojom::CertVerifierCreationParams* cert_verifier_creation_params) {
  if (profile_->IsOffTheRecord())
    in_memory = true;
  base::FilePath path(GetPartitionPath(relative_partition_path));

  g_browser_process->system_network_context_manager()
      ->ConfigureDefaultNetworkContextParams(network_context_params,
                                             cert_verifier_creation_params);

  network_context_params->context_name = std::string("main");

  network_context_params->accept_language = ComputeAcceptLanguage();
  network_context_params->enable_referrers = enable_referrers_.GetValue();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kShortReportingDelay)) {
    network_context_params->reporting_delivery_interval =
        base::TimeDelta::FromMilliseconds(100);
  }

  InitializeCorsExtraSafelistedRequestHeaderNamesForProfile(
      profile_,
      &network_context_params->cors_extra_safelisted_request_header_names);
  network_context_params->cors_mode =
      profile_->ShouldEnableOutOfBlinkCors()
          ? network::mojom::NetworkContextParams::CorsMode::kEnable
          : network::mojom::NetworkContextParams::CorsMode::kDisable;

  // Always enable the HTTP cache.
  network_context_params->http_cache_enabled = true;

  network_context_params->http_auth_static_network_context_params =
      network::mojom::HttpAuthStaticNetworkContextParams::New();

  if (IsAmbientAuthAllowedForProfile(profile_)) {
    network_context_params->http_auth_static_network_context_params
        ->allow_default_credentials =
        net::HttpAuthPreferences::ALLOW_DEFAULT_CREDENTIALS;
  } else {
    network_context_params->http_auth_static_network_context_params
        ->allow_default_credentials =
        net::HttpAuthPreferences::DISALLOW_DEFAULT_CREDENTIALS;
  }

  network_context_params->cookie_manager_params =
      CreateCookieManagerParams(profile_, *cookie_settings_);

  // Configure on-disk storage for non-OTR profiles. OTR profiles just use
  // default behavior (in memory storage, default sizes).
  if (!in_memory) {
    PrefService* local_state = g_browser_process->local_state();
    // Configure the HTTP cache path and size.
    base::FilePath base_cache_path;
    chrome::GetUserCacheDirectory(path, &base_cache_path);
    base::FilePath disk_cache_dir =
        local_state->GetFilePath(prefs::kDiskCacheDir);
    if (!disk_cache_dir.empty())
      base_cache_path = disk_cache_dir.Append(base_cache_path.BaseName());
    network_context_params->http_cache_path =
        base_cache_path.Append(chrome::kCacheDirname);
    network_context_params->http_cache_max_size =
        local_state->GetInteger(prefs::kDiskCacheSize);

    // Currently this just contains HttpServerProperties, but that will likely
    // change.
    network_context_params->http_server_properties_path =
        path.Append(chrome::kNetworkPersistentStateFilename);

    base::FilePath cookie_path = path;
    cookie_path = cookie_path.Append(chrome::kCookieFilename);
    network_context_params->cookie_path = cookie_path;

    base::FilePath trust_token_path = path;
    trust_token_path = trust_token_path.Append(chrome::kTrustTokenFilename);
    network_context_params->trust_token_path = std::move(trust_token_path);

#if BUILDFLAG(ENABLE_REPORTING)
    base::FilePath reporting_and_nel_store_path = path;
    reporting_and_nel_store_path = reporting_and_nel_store_path.Append(
        chrome::kReportingAndNelStoreFilename);
    network_context_params->reporting_and_nel_store_path =
        reporting_and_nel_store_path;
#endif  // BUILDFLAG(ENABLE_REPORTING)

    if (relative_partition_path.empty()) {  // This is the main partition.
      network_context_params->restore_old_session_cookies =
          profile_->ShouldRestoreOldSessionCookies();
      network_context_params->persist_session_cookies =
          profile_->ShouldPersistSessionCookies();
    } else {
      // Copy behavior of ProfileImplIOData::InitializeAppRequestContext.
      network_context_params->restore_old_session_cookies = false;
      network_context_params->persist_session_cookies = false;
    }

    network_context_params->transport_security_persister_path = path;
  }
  const base::ListValue* hsts_policy_bypass_list =
      g_browser_process->local_state()->GetList(prefs::kHSTSPolicyBypassList);
  for (const auto& value : *hsts_policy_bypass_list) {
    std::string string_value;
    if (!value.GetAsString(&string_value)) {
      continue;
    }
    network_context_params->hsts_policy_bypass_list.push_back(string_value);
  }

  // NOTE(mmenke): Keep these protocol handlers and
  // ProfileIOData::SetUpJobFactoryDefaultsForBuilder in sync with
  // ProfileIOData::IsHandledProtocol().
  // TODO(mmenke): Find a better way of handling tracking supported schemes.
#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
  network_context_params->enable_ftp_url_support =
      base::FeatureList::IsEnabled(features::kFtpProtocol);
#endif  // !BUILDFLAG(DISABLE_FTP_SUPPORT)

  proxy_config_monitor_.AddToNetworkContextParams(network_context_params);

  network_context_params->enable_certificate_reporting = true;
  network_context_params->enable_expect_ct_reporting = true;

  network_context_params->ct_policy = GetCTPolicy();

#if BUILDFLAG(TRIAL_COMPARISON_CERT_VERIFIER_SUPPORTED)
  // Require the use_builtin_cert_verifier to be explicitly initialized, as
  // using the TrialComparisonCertVerifier requires knowing whether Chrome is
  // using the system verifier.
  DCHECK(cert_verifier_creation_params);
  DCHECK_NE(
      cert_verifier_creation_params->use_builtin_cert_verifier,
      network::mojom::CertVerifierCreationParams::CertVerifierImpl::kDefault);
  if (!in_memory &&
      cert_verifier_creation_params->use_builtin_cert_verifier ==
          network::mojom::CertVerifierCreationParams::CertVerifierImpl::
              kSystem &&
      TrialComparisonCertVerifierController::MaybeAllowedForProfile(profile_)) {
    mojo::PendingRemote<network::mojom::TrialComparisonCertVerifierConfigClient>
        config_client;
    auto config_client_receiver =
        config_client.InitWithNewPipeAndPassReceiver();

    cert_verifier_creation_params->trial_comparison_cert_verifier_params =
        network::mojom::TrialComparisonCertVerifierParams::New();

    if (!trial_comparison_cert_verifier_controller_) {
      trial_comparison_cert_verifier_controller_ =
          std::make_unique<TrialComparisonCertVerifierController>(profile_);
    }
    trial_comparison_cert_verifier_controller_->AddClient(
        std::move(config_client),
        cert_verifier_creation_params->trial_comparison_cert_verifier_params
            ->report_client.InitWithNewPipeAndPassReceiver());
    cert_verifier_creation_params->trial_comparison_cert_verifier_params
        ->initial_allowed =
        trial_comparison_cert_verifier_controller_->IsAllowed();
    cert_verifier_creation_params->trial_comparison_cert_verifier_params
        ->config_client_receiver = std::move(config_client_receiver);
  }
#endif

  if (domain_reliability::DomainReliabilityServiceFactory::
          ShouldCreateService()) {
    network_context_params->enable_domain_reliability = true;
    network_context_params->domain_reliability_upload_reporter =
        domain_reliability::DomainReliabilityServiceFactory::
            kUploadReporterString;
    network_context_params->discard_domain_reliablity_uploads =
        g_discard_domain_reliability_uploads_for_testing
            ? *g_discard_domain_reliability_uploads_for_testing
            : !g_browser_process->local_state()->GetBoolean(
                  metrics::prefs::kMetricsReportingEnabled);
  }

  auto* drp_settings =
      DataReductionProxyChromeSettingsFactory::GetForBrowserContext(profile_);
  if (drp_settings) {
    mojo::Remote<network::mojom::CustomProxyConfigClient> config_client;
    network_context_params->custom_proxy_config_client_receiver =
        config_client.BindNewPipeAndPassReceiver();
    drp_settings->AddCustomProxyConfigClient(std::move(config_client));
  }

#if defined(OS_CHROMEOS)
  bool profile_supports_policy_certs = false;
  if (chromeos::ProfileHelper::IsSigninProfile(profile_))
    profile_supports_policy_certs = true;
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  if (user_manager) {
    const user_manager::User* user =
        chromeos::ProfileHelper::Get()->GetUserByProfile(profile_);
    // No need to initialize NSS for users with empty username hash:
    // Getters for a user's NSS slots always return NULL slot if the user's
    // username hash is empty, even when the NSS is not initialized for the
    // user.
    if (user && !user->username_hash().empty()) {
      cert_verifier_creation_params->username_hash = user->username_hash();
      cert_verifier_creation_params->nss_path = profile_->GetPath();
      profile_supports_policy_certs = true;
    }
  }
  if (profile_supports_policy_certs &&
      policy::PolicyCertServiceFactory::CreateAndStartObservingForProfile(
          profile_)) {
    const policy::PolicyCertService* policy_cert_service =
        policy::PolicyCertServiceFactory::GetForProfile(profile_);
    network_context_params->initial_additional_certificates =
        GetAdditionalCertificates(policy_cert_service,
                                  GetPartitionPath(relative_partition_path));
  }
#endif

  // Should be initialized with existing per-profile CORS access lists.
  network_context_params->cors_origin_access_list =
      profile_->GetSharedCorsOriginAccessList()
          ->GetOriginAccessList()
          .CreateCorsOriginAccessPatternsList();

  network_context_params->reset_http_cache_backend =
      GetHttpCacheBackendResetParam(g_browser_process->local_state());

  network_context_params->split_auth_cache_by_network_isolation_key =
      ShouldSplitAuthCacheByNetworkIsolationKey();

  // All consumers of the main NetworkContext must provide NetworkIsolationKeys
  // / IsolationInfos, so storage can be isolated on a per-site basis.
  network_context_params->require_network_isolation_key = true;
}

base::FilePath ProfileNetworkContextService::GetPartitionPath(
    const base::FilePath& relative_partition_path) {
  base::FilePath path = profile_->GetPath();
  if (!relative_partition_path.empty())
    path = path.Append(relative_partition_path);
  return path;
}

void ProfileNetworkContextService::OnContentSettingChanged(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const std::string& resource_identifier) {
  if (content_type != ContentSettingsType::COOKIES &&
      content_type != ContentSettingsType::LEGACY_COOKIE_ACCESS &&
      content_type != ContentSettingsType::DEFAULT) {
    return;
  }

  if (content_type == ContentSettingsType::COOKIES ||
      content_type == ContentSettingsType::DEFAULT) {
    ContentSettingsForOneType cookies_settings;
    HostContentSettingsMapFactory::GetForProfile(profile_)
        ->GetSettingsForOneType(ContentSettingsType::COOKIES, std::string(),
                                &cookies_settings);
    content::BrowserContext::ForEachStoragePartition(
        profile_, base::BindRepeating(
                      [](ContentSettingsForOneType settings,
                         content::StoragePartition* storage_partition) {
                        storage_partition->GetCookieManagerForBrowserProcess()
                            ->SetContentSettings(settings);
                      },
                      cookies_settings));
  }

  if (content_type == ContentSettingsType::LEGACY_COOKIE_ACCESS ||
      content_type == ContentSettingsType::DEFAULT) {
    ContentSettingsForOneType legacy_cookie_access_settings;
    HostContentSettingsMapFactory::GetForProfile(profile_)
        ->GetSettingsForOneType(ContentSettingsType::LEGACY_COOKIE_ACCESS,
                                std::string(), &legacy_cookie_access_settings);
    content::BrowserContext::ForEachStoragePartition(
        profile_, base::BindRepeating(
                      [](ContentSettingsForOneType settings,
                         content::StoragePartition* storage_partition) {
                        storage_partition->GetCookieManagerForBrowserProcess()
                            ->SetContentSettingsForLegacyCookieAccess(settings);
                      },
                      legacy_cookie_access_settings));
  }
}
