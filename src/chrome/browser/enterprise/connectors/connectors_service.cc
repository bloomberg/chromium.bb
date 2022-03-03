// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/connectors_service.h"

#include <memory>

#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/utf_string_conversions.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/connectors/connectors_manager.h"
#include "chrome/browser/enterprise/connectors/service_provider_config.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/dm_token_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/reporting_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/managed_ui.h"
#include "components/embedder_support/user_agent_utils.h"
#include "components/enterprise/browser/controller/browser_dm_token_storage.h"
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/common/cloud/cloud_policy_store.h"
#include "components/policy/core/common/cloud/cloud_policy_util.h"
#include "components/policy/core/common/cloud/dm_token.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_manager.h"
#include "components/policy/core/common/cloud/machine_level_user_cloud_policy_store.h"
#include "components/policy/core/common/cloud/user_cloud_policy_manager.h"
#include "components/policy/core/common/policy_types.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/user_prefs/user_prefs.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/url_constants.h"
#include "device_management_backend.pb.h"
#include "google_apis/gaia/gaia_auth_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/policy/core/user_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "components/user_manager/user.h"
#include "extensions/common/constants.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "components/policy/core/common/policy_loader_lacros.h"
#endif

namespace enterprise_connectors {

namespace {

void PopulateBrowserMetadata(bool include_device_info,
                             ClientMetadata::Browser* browser_proto) {
  base::FilePath browser_id;
  if (base::PathService::Get(base::DIR_EXE, &browser_id))
    browser_proto->set_browser_id(browser_id.AsUTF8Unsafe());
  browser_proto->set_chrome_version(version_info::GetVersionNumber());
  if (include_device_info)
    browser_proto->set_machine_user(policy::GetOSUsername());
}

void PopulateDeviceMetadata(const ReportingSettings& reporting_settings,
                            Profile* profile,
                            ClientMetadata::Device* device_proto) {
  if (!reporting_settings.per_profile && !device_proto->has_dm_token())
    device_proto->set_dm_token(reporting_settings.dm_token);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::string client_id;
  auto* manager = profile->GetUserCloudPolicyManagerAsh();
  if (manager && manager->core() && manager->core()->client())
    client_id = manager->core()->client()->client_id();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // TODO(crbug.com/1252802): Add the client ID for LaCrOS.
  std::string client_id = "";
#else
  std::string client_id =
      policy::BrowserDMTokenStorage::Get()->RetrieveClientId();
#endif
  device_proto->set_client_id(client_id);
  device_proto->set_os_version(policy::GetOSVersion());
  device_proto->set_os_platform(policy::GetOSPlatform());
  device_proto->set_name(policy::GetDeviceName());
}

bool IsURLExemptFromAnalysis(const GURL& url) {
  if (url.SchemeIs(content::kChromeUIScheme))
    return true;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (url.SchemeIs(extensions::kExtensionScheme) &&
      extension_misc::IsSystemUIApp(url.host_piece())) {
    return true;
  }
#endif

  return false;
}

#if BUILDFLAG(IS_CHROMEOS_LACROS)
// Returns the single main profile, or nullptr if none is found.
Profile* GetMainProfile() {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (!profile_manager)
    return nullptr;
  auto profiles = g_browser_process->profile_manager()->GetLoadedProfiles();
  const auto main_it = base::ranges::find_if(
      profiles, [](Profile* profile) { return profile->IsMainProfile(); });
  if (main_it == profiles.end())
    return nullptr;
  return *main_it;
}

#endif

}  // namespace

const base::Feature kEnterpriseConnectorsEnabled{
    "EnterpriseConnectorsEnabled", base::FEATURE_ENABLED_BY_DEFAULT};

const char kServiceProviderConfig[] = R"({
  "version": "1",
  "service_providers" : [
    {
      "name": "google",
      "display_name": "Google Cloud",
      "version": {
        "1": {
          "analysis": {
            "url": "https://safebrowsing.google.com/safebrowsing/uploads/scan",
            "supported_tags": [
              {
                "name": "malware",
                "display_name": "Threat protection",
                "mime_types": [
                  "application/vnd.microsoft.portable-executable",
                  "application/vnd.rar",
                  "application/x-msdos-program",
                  "application/zip"
                ],
                "max_file_size": 52428800
              },
              {
                "name": "dlp",
                "display_name": "Sensitive data protection",
                "mime_types": [
                  "application/gzip",
                  "application/msword",
                  "application/pdf",
                  "application/postscript",
                  "application/rtf",
                  "application/vnd.google-apps.document.internal",
                  "application/vnd.google-apps.spreadsheet.internal",
                  "application/vnd.ms-cab-compressed",
                  "application/vnd.ms-excel",
                  "application/vnd.ms-powerpoint",
                  "application/vnd.ms-xpsdocument",
                  "application/vnd.oasis.opendocument.text",
                  "application/vnd.openxmlformats-officedocument.presentationml.presentation",
                  "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
                  "application/vnd.openxmlformats-officedocument.spreadsheetml.template",
                  "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
                  "application/vnd.openxmlformats-officedocument.wordprocessingml.template",
                  "application/vnd.ms-excel.sheet.macroenabled.12",
                  "application/vnd.ms-excel.template.macroenabled.12",
                  "application/vnd.ms-word.document.macroenabled.12",
                  "application/vnd.ms-word.template.macroenabled.12",
                  "application/vnd.rar",
                  "application/vnd.wordperfect",
                  "application/x-7z-compressed",
                  "application/x-bzip",
                  "application/x-bzip2",
                  "application/x-tar",
                  "application/zip",
                  "text/csv",
                  "text/plain"
                ],
                "max_file_size": 52428800
              }
            ]
          },
          "reporting": {
            "url": "https://chromereporting-pa.googleapis.com/v1/events"
          }
        }
      }
    },
    {
      "name": "box",
      "display_name": "Box",
      "version":  {
        "1": {
          "file_system": {
            "home": "https://box.com",
            "authorization_endpoint": "https://account.box.com/api/oauth2/authorize",
            "token_endpoint": "https://api.box.com/oauth2/token",
            "max_direct_size": 20971520,
            "scopes": [],
            "disable": [ "box.com", "boxcloud.com" ]
          }
        }
      }
    }
  ]
})";

ServiceProviderConfig* GetServiceProviderConfig() {
  static base::NoDestructor<ServiceProviderConfig> config(
      kServiceProviderConfig);
  return config.get();
}

// --------------------------------
// ConnectorsService implementation
// --------------------------------

ConnectorsService::ConnectorsService(content::BrowserContext* context,
                                     std::unique_ptr<ConnectorsManager> manager)
    : context_(context), connectors_manager_(std::move(manager)) {
  DCHECK(context_);
  DCHECK(connectors_manager_);
}

ConnectorsService::~ConnectorsService() = default;

absl::optional<ReportingSettings> ConnectorsService::GetReportingSettings(
    ReportingConnector connector) {
  if (!ConnectorsEnabled())
    return absl::nullopt;

  absl::optional<ReportingSettings> settings =
      connectors_manager_->GetReportingSettings(connector);
  if (!settings.has_value())
    return absl::nullopt;

  absl::optional<DmToken> dm_token = GetDmToken(ConnectorScopePref(connector));
  if (!dm_token.has_value())
    return absl::nullopt;

  settings.value().dm_token = dm_token.value().value;
  settings.value().per_profile =
      dm_token.value().scope == policy::POLICY_SCOPE_USER;

  return settings;
}

absl::optional<AnalysisSettings> ConnectorsService::GetAnalysisSettings(
    const GURL& url,
    AnalysisConnector connector) {
  if (!ConnectorsEnabled())
    return absl::nullopt;

  if (IsURLExemptFromAnalysis(url))
    return absl::nullopt;

  absl::optional<AnalysisSettings> settings =
      connectors_manager_->GetAnalysisSettings(url, connector);
  if (!settings.has_value())
    return absl::nullopt;

  absl::optional<DmToken> dm_token = GetDmToken(ConnectorScopePref(connector));
  if (!dm_token.has_value())
    return absl::nullopt;

  settings.value().dm_token = dm_token.value().value;
  settings.value().per_profile =
      dm_token.value().scope == policy::POLICY_SCOPE_USER;
  settings.value().client_metadata = BuildClientMetadata();

  return settings;
}

absl::optional<FileSystemSettings>
ConnectorsService::GetFileSystemGlobalSettings(FileSystemConnector connector) {
  if (!ConnectorsEnabled())
    return absl::nullopt;

  absl::optional<FileSystemSettings> settings =
      connectors_manager_->GetFileSystemGlobalSettings(connector);

  if (!settings.has_value())
    return absl::nullopt;

  return settings;
}

absl::optional<FileSystemSettings> ConnectorsService::GetFileSystemSettings(
    const GURL& url,
    FileSystemConnector connector) {
  if (!ConnectorsEnabled())
    return absl::nullopt;

  absl::optional<FileSystemSettings> settings =
      connectors_manager_->GetFileSystemSettings(url, connector);
  if (!settings.has_value())
    return absl::nullopt;

  return settings;
}

bool ConnectorsService::IsConnectorEnabled(AnalysisConnector connector) const {
  if (!ConnectorsEnabled())
    return false;

  return connectors_manager_->IsConnectorEnabled(connector);
}

bool ConnectorsService::IsConnectorEnabled(ReportingConnector connector) const {
  if (!ConnectorsEnabled())
    return false;

  return connectors_manager_->IsConnectorEnabled(connector);
}

bool ConnectorsService::IsConnectorEnabled(
    FileSystemConnector connector) const {
  if (!ConnectorsEnabled())
    return false;

  return connectors_manager_->IsConnectorEnabled(connector);
}

std::vector<std::string> ConnectorsService::GetReportingServiceProviderNames(
    ReportingConnector connector) {
  if (!ConnectorsEnabled())
    return {};

  if (!GetDmToken(ConnectorScopePref(connector)).has_value())
    return {};

  return connectors_manager_->GetReportingServiceProviderNames(connector);
}

bool ConnectorsService::DelayUntilVerdict(AnalysisConnector connector) {
  if (!ConnectorsEnabled())
    return false;

  return connectors_manager_->DelayUntilVerdict(connector);
}

absl::optional<std::u16string> ConnectorsService::GetCustomMessage(
    AnalysisConnector connector,
    const std::string& tag) {
  if (!ConnectorsEnabled())
    return absl::nullopt;

  return connectors_manager_->GetCustomMessage(connector, tag);
}

absl::optional<GURL> ConnectorsService::GetLearnMoreUrl(
    AnalysisConnector connector,
    const std::string& tag) {
  if (!ConnectorsEnabled())
    return absl::nullopt;

  return connectors_manager_->GetLearnMoreUrl(connector, tag);
}

bool ConnectorsService::HasCustomInfoToDisplay(AnalysisConnector connector,
                                               const std::string& tag) {
  return GetCustomMessage(connector, tag) || GetLearnMoreUrl(connector, tag);
}

std::vector<std::string> ConnectorsService::GetAnalysisServiceProviderNames(
    AnalysisConnector connector) {
  if (!ConnectorsEnabled())
    return {};

  if (!GetDmToken(ConnectorScopePref(connector)).has_value())
    return {};

  return connectors_manager_->GetAnalysisServiceProviderNames(connector);
}

std::string ConnectorsService::GetManagementDomain() {
  if (!ConnectorsEnabled())
    return std::string();

  absl::optional<policy::PolicyScope> scope = absl::nullopt;
  for (const char* scope_pref :
       {prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckScope,
        ConnectorScopePref(AnalysisConnector::FILE_ATTACHED),
        ConnectorScopePref(AnalysisConnector::FILE_DOWNLOADED),
        ConnectorScopePref(AnalysisConnector::BULK_DATA_ENTRY),
        ConnectorScopePref(AnalysisConnector::PRINT),
        ConnectorScopePref(ReportingConnector::SECURITY_EVENT)}) {
    absl::optional<DmToken> dm_token = GetDmToken(scope_pref);
    if (dm_token.has_value()) {
      scope = dm_token.value().scope;

      // Having one CBCM Connector policy set implies that profile ones will be
      // ignored for another domain, so the loop can stop immediately.
      if (scope == policy::PolicyScope::POLICY_SCOPE_MACHINE)
        break;
    }
  }

  if (!scope.has_value())
    return std::string();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  return chrome::GetAccountManagerIdentity(
             Profile::FromBrowserContext(context_))
      .value_or(std::string());
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // In LaCros it's always managed by main profile policy.
  const enterprise_management::PolicyData* policy =
      policy::PolicyLoaderLacros::main_user_policy_data();
  if (policy && policy->has_managed_by())
    return policy->managed_by();
  return std::string();
#else
  if (scope.value() == policy::PolicyScope::POLICY_SCOPE_USER) {
    return chrome::GetAccountManagerIdentity(
               Profile::FromBrowserContext(context_))
        .value_or(std::string());
  }

  policy::MachineLevelUserCloudPolicyManager* manager =
      g_browser_process->browser_policy_connector()
          ->machine_level_user_cloud_policy_manager();
  if (!manager)
    return std::string();

  policy::CloudPolicyStore* store = manager->store();
  return (store && store->has_policy())
             ? gaia::ExtractDomainName(store->policy()->username())
             : std::string();
#endif
}

absl::optional<std::string> ConnectorsService::GetDMTokenForRealTimeUrlCheck()
    const {
  if (!ConnectorsEnabled())
    return absl::nullopt;

  if (Profile::FromBrowserContext(context_)->GetPrefs()->GetInteger(
          prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckMode) ==
      safe_browsing::REAL_TIME_CHECK_DISABLED) {
    return absl::nullopt;
  }

  absl::optional<DmToken> dm_token =
      GetDmToken(prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckScope);

  if (dm_token.has_value())
    return dm_token.value().value;
  return absl::nullopt;
}

safe_browsing::EnterpriseRealTimeUrlCheckMode
ConnectorsService::GetAppliedRealTimeUrlCheck() const {
  if (!ConnectorsEnabled() ||
      !GetDmToken(prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckScope)
           .has_value()) {
    return safe_browsing::REAL_TIME_CHECK_DISABLED;
  }

  return static_cast<safe_browsing::EnterpriseRealTimeUrlCheckMode>(
      Profile::FromBrowserContext(context_)->GetPrefs()->GetInteger(
          prefs::kSafeBrowsingEnterpriseRealTimeUrlCheckMode));
}

ConnectorsManager* ConnectorsService::ConnectorsManagerForTesting() {
  return connectors_manager_.get();
}

ConnectorsService::DmToken::DmToken(const std::string& value,
                                    policy::PolicyScope scope)
    : value(value), scope(scope) {}
ConnectorsService::DmToken::DmToken(DmToken&&) = default;
ConnectorsService::DmToken& ConnectorsService::DmToken::operator=(DmToken&&) =
    default;
ConnectorsService::DmToken::~DmToken() = default;

absl::optional<ConnectorsService::DmToken> ConnectorsService::GetDmToken(
    const char* scope_pref) const {
#if defined(OS_CHROMEOS)
  // On CrOS the settings from primary profile applies to all profiles.
  return GetBrowserDmToken();
#else
  return GetPolicyScope(scope_pref) == policy::POLICY_SCOPE_USER
             ? GetProfileDmToken()
             : GetBrowserDmToken();
#endif
}

absl::optional<ConnectorsService::DmToken>
ConnectorsService::GetBrowserDmToken() const {
  policy::DMToken dm_token =
      policy::GetDMToken(Profile::FromBrowserContext(context_));

  if (!dm_token.is_valid())
    return absl::nullopt;

  return DmToken(dm_token.value(), policy::POLICY_SCOPE_MACHINE);
}

#if !defined(OS_CHROMEOS)
absl::optional<ConnectorsService::DmToken>
ConnectorsService::GetProfileDmToken() const {
  if (!CanUseProfileDmToken())
    return absl::nullopt;

  Profile* profile = Profile::FromBrowserContext(context_);

  policy::UserCloudPolicyManager* policy_manager =
      profile->GetUserCloudPolicyManager();
  if (!policy_manager || !policy_manager->IsClientRegistered())
    return absl::nullopt;

  return DmToken(policy_manager->core()->client()->dm_token(),
                 policy::POLICY_SCOPE_USER);
}

bool ConnectorsService::CanUseProfileDmToken() const {
  Profile* profile = Profile::FromBrowserContext(context_);

  // If the browser isn't managed by CBCM, then the profile DM token can be
  // used.
  if (!policy::BrowserDMTokenStorage::Get()->RetrieveDMToken().is_valid())
    return true;

  return chrome::enterprise_util::IsProfileAffiliated(profile);
}
#endif  // !defined(OS_CHROMEOS)

policy::PolicyScope ConnectorsService::GetPolicyScope(
    const char* scope_pref) const {
  return static_cast<policy::PolicyScope>(
      Profile::FromBrowserContext(context_)->GetPrefs()->GetInteger(
          scope_pref));
}

bool ConnectorsService::ConnectorsEnabled() const {
  if (!base::FeatureList::IsEnabled(kEnterpriseConnectorsEnabled))
    return false;

  return !Profile::FromBrowserContext(context_)->IsOffTheRecord();
}

std::unique_ptr<ClientMetadata> ConnectorsService::BuildClientMetadata() {
  // Check the reporting policy value to check if the analysis should include
  // browser/device/profile information.
  auto reporting_settings =
      GetReportingSettings(ReportingConnector::SECURITY_EVENT);
  if (!reporting_settings.has_value())
    return nullptr;

  Profile* profile = Profile::FromBrowserContext(context_);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const user_manager::User* user =
      chromeos::ProfileHelper::Get()->GetUserByProfile(profile);
  const bool include_device_info = user && user->IsAffiliated();
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  const bool include_device_info =
      policy::PolicyLoaderLacros::IsMainUserAffiliated();
#else
  const bool include_device_info = !reporting_settings.value().per_profile;
#endif

  auto metadata = std::make_unique<ClientMetadata>(
      reporting::GetContextAsClientMetadata(profile));
  PopulateBrowserMetadata(include_device_info, metadata->mutable_browser());
  if (include_device_info) {
    PopulateDeviceMetadata(reporting_settings.value(), profile,
                           metadata->mutable_device());
  }

  return metadata;
}

// ---------------------------------------
// ConnectorsServiceFactory implementation
// ---------------------------------------

// static
ConnectorsServiceFactory* ConnectorsServiceFactory::GetInstance() {
  return base::Singleton<ConnectorsServiceFactory>::get();
}

ConnectorsService* ConnectorsServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<ConnectorsService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

ConnectorsServiceFactory::ConnectorsServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "ConnectorsService",
          BrowserContextDependencyManager::GetInstance()) {}

ConnectorsServiceFactory::~ConnectorsServiceFactory() = default;

KeyedService* ConnectorsServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new ConnectorsService(
      context,
      std::make_unique<ConnectorsManager>(
          user_prefs::UserPrefs::Get(context), GetServiceProviderConfig(),
          base::FeatureList::IsEnabled(kEnterpriseConnectorsEnabled)));
}

content::BrowserContext* ConnectorsServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // On Chrome OS, settings from the primary/main profile apply to all
  // profiles, besides incognito.
  // However, the primary/main profile might not exist in tests - then the
  // provided |context| is still used.
  if (context && !context->IsOffTheRecord() &&
      !Profile::FromBrowserContext(context)->AsTestingProfile()) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    Profile* primary_profile = ProfileManager::GetPrimaryUserProfile();
    if (primary_profile)
      return primary_profile;
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
    Profile* main_profile = GetMainProfile();
    if (main_profile)
      return main_profile;
#endif
  }
  return context;
}

}  // namespace enterprise_connectors
