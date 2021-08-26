// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/signals/context_info_fetcher.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/string_split.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/enterprise/util/affiliation.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/component_updater/pref_names.h"
#include "components/policy/content/policy_blocklist_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/site_isolation_policy.h"
#include "device_management_backend.pb.h"

#if defined(OS_WIN)
#include <netfw.h>
#include <windows.h>
#include <wrl/client.h>
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/dbus/constants/dbus_switches.h"
#endif

namespace enterprise_signals {

namespace {

bool IsURLBlocked(const GURL& url, content::BrowserContext* browser_context_) {
  PolicyBlocklistService* service =
      PolicyBlocklistFactory::GetForBrowserContext(browser_context_);

  if (!service)
    return false;

  policy::URLBlocklist::URLBlocklistState state =
      service->GetURLBlocklistState(url);

  return state == policy::URLBlocklist::URLBlocklistState::URL_IN_BLOCKLIST;
}

#if defined(OS_LINUX)
const char** GetUfwConfigPath() {
  static const char* path = "/etc/ufw/ufw.conf";
  return &path;
}

SettingValue GetUfwStatus() {
  base::FilePath path(*GetUfwConfigPath());
  std::string file_content;
  base::StringPairs values;

  if (!base::PathExists(path) || !base::PathIsReadable(path) ||
      !base::ReadFileToString(path, &file_content)) {
    return SettingValue::UNKNOWN;
  }
  base::SplitStringIntoKeyValuePairs(file_content, '=', '\n', &values);
  auto is_ufw_enabled = std::find_if(values.begin(), values.end(), [](auto v) {
    return v.first == "ENABLED";
  });
  if (is_ufw_enabled == values.end())
    return SettingValue::UNKNOWN;

  if (is_ufw_enabled->second == "yes")
    return SettingValue::ENABLED;
  else if (is_ufw_enabled->second == "no")
    return SettingValue::DISABLED;
  else
    return SettingValue::UNKNOWN;
}
#endif  // defined(OS_LINUX)

#if defined(OS_WIN)
SettingValue GetWinOSFirewall() {
  Microsoft::WRL::ComPtr<INetFwPolicy2> firewall_policy;
  HRESULT hr = CoCreateInstance(CLSID_NetFwPolicy2, nullptr, CLSCTX_ALL,
                                IID_PPV_ARGS(&firewall_policy));
  if (FAILED(hr)) {
    DLOG(ERROR) << logging::SystemErrorCodeToString(hr);
    return SettingValue::UNKNOWN;
  }

  long profile_types = 0;
  hr = firewall_policy->get_CurrentProfileTypes(&profile_types);
  if (FAILED(hr))
    return SettingValue::UNKNOWN;

  // The most restrictive active profile takes precedence.
  constexpr NET_FW_PROFILE_TYPE2 kProfileTypes[] = {
      NET_FW_PROFILE2_PUBLIC, NET_FW_PROFILE2_PRIVATE, NET_FW_PROFILE2_DOMAIN};
  for (size_t i = 0; i < base::size(kProfileTypes); ++i) {
    if ((profile_types & kProfileTypes[i]) != 0) {
      VARIANT_BOOL enabled = VARIANT_TRUE;
      hr = firewall_policy->get_FirewallEnabled(kProfileTypes[i], &enabled);
      if (FAILED(hr))
        return SettingValue::UNKNOWN;
      if (enabled == VARIANT_TRUE)
        return SettingValue::ENABLED;
      else if (enabled == VARIANT_FALSE)
        return SettingValue::DISABLED;
      else
        return SettingValue::UNKNOWN;
    }
  }
  return SettingValue::UNKNOWN;
}
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
SettingValue GetChromeosFirewall() {
  // The firewall is always enabled and can only be disabled in dev mode on
  // ChromeOS. If the device isn't in dev mode, the firewall is guaranteed to be
  // enabled whereas if it's in dev mode, the firewall could be enabled or not.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(chromeos::switches::kSystemDevMode)
             ? SettingValue::UNKNOWN
             : SettingValue::ENABLED;
}
#endif

}  // namespace

ContextInfo::ContextInfo() = default;
ContextInfo::ContextInfo(ContextInfo&&) = default;
ContextInfo::~ContextInfo() = default;

ContextInfoFetcher::ContextInfoFetcher(
    content::BrowserContext* browser_context,
    enterprise_connectors::ConnectorsService* connectors_service)
    : browser_context_(browser_context),
      connectors_service_(connectors_service) {
  DCHECK(connectors_service_);
}

ContextInfoFetcher::~ContextInfoFetcher() = default;

// static
std::unique_ptr<ContextInfoFetcher> ContextInfoFetcher::CreateInstance(
    content::BrowserContext* browser_context,
    enterprise_connectors::ConnectorsService* connectors_service) {
  // TODO(domfc): Add platform overrides of the class once they are needed for
  // an attribute.
  return std::make_unique<ContextInfoFetcher>(browser_context,
                                              connectors_service);
}

ContextInfo ContextInfoFetcher::FetchAsyncSignals(ContextInfo info) {
  // Add other async signals here
  info.os_firewall = GetOSFirewall();
  return info;
}

void ContextInfoFetcher::Fetch(ContextInfoCallback callback) {
  ContextInfo info;

  info.browser_affiliation_ids = GetBrowserAffiliationIDs();
  info.profile_affiliation_ids = GetProfileAffiliationIDs();
  info.on_file_attached_providers =
      GetAnalysisConnectorProviders(enterprise_connectors::FILE_ATTACHED);
  info.on_file_downloaded_providers =
      GetAnalysisConnectorProviders(enterprise_connectors::FILE_DOWNLOADED);
  info.on_bulk_data_entry_providers =
      GetAnalysisConnectorProviders(enterprise_connectors::BULK_DATA_ENTRY);
  info.realtime_url_check_mode = GetRealtimeUrlCheckMode();
  info.on_security_event_providers = GetOnSecurityEventProviders();
  info.browser_version = version_info::GetVersionNumber();
  info.safe_browsing_protection_level = GetSafeBrowsingProtectionLevel();
  info.site_isolation_enabled =
      content::SiteIsolationPolicy::UseDedicatedProcessesForAllSites();
  info.built_in_dns_client_enabled = GetBuiltInDnsClientEnabled();
  info.password_protection_warning_trigger =
      GetPasswordProtectionWarningTrigger();
  info.chrome_cleanup_enabled = GetChromeCleanupEnabled();
  info.chrome_remote_desktop_app_blocked = GetChromeRemoteDesktopAppBlocked();
  info.third_party_blocking_enabled = GetThirdPartyBLockingEnabled();
#if defined(OS_WIN)
  base::ThreadPool::CreateCOMSTATaskRunner({})
      .get()
      ->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&ContextInfoFetcher::FetchAsyncSignals,
                         base::Unretained(this), std::move(info)),
          std::move(callback));
#else
  base::ThreadPool::CreateTaskRunner({base::MayBlock()})
      .get()
      ->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&ContextInfoFetcher::FetchAsyncSignals,
                         base::Unretained(this), std::move(info)),
          std::move(callback));
#endif
}

std::vector<std::string> ContextInfoFetcher::GetBrowserAffiliationIDs() {
  auto ids =
      g_browser_process->browser_policy_connector()->device_affiliation_ids();
  return {ids.begin(), ids.end()};
}

std::vector<std::string> ContextInfoFetcher::GetProfileAffiliationIDs() {
  auto ids = Profile::FromBrowserContext(browser_context_)
                 ->GetProfilePolicyConnector()
                 ->user_affiliation_ids();
  return {ids.begin(), ids.end()};
}

std::vector<std::string> ContextInfoFetcher::GetAnalysisConnectorProviders(
    enterprise_connectors::AnalysisConnector connector) {
  return connectors_service_->GetAnalysisServiceProviderNames(connector);
}

safe_browsing::EnterpriseRealTimeUrlCheckMode
ContextInfoFetcher::GetRealtimeUrlCheckMode() {
  return connectors_service_->GetAppliedRealTimeUrlCheck();
}

std::vector<std::string> ContextInfoFetcher::GetOnSecurityEventProviders() {
  return connectors_service_->GetReportingServiceProviderNames(
      enterprise_connectors::ReportingConnector::SECURITY_EVENT);
}

safe_browsing::SafeBrowsingState
ContextInfoFetcher::GetSafeBrowsingProtectionLevel() {
  Profile* profile = Profile::FromBrowserContext(browser_context_);

  bool safe_browsing_enabled =
      profile->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled);
  bool safe_browsing_enhanced_enabled =
      profile->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnhanced);

  if (safe_browsing_enabled) {
    if (safe_browsing_enhanced_enabled)
      return safe_browsing::ENHANCED_PROTECTION;
    else
      return safe_browsing::STANDARD_PROTECTION;
  } else {
    return safe_browsing::NO_SAFE_BROWSING;
  }
}

absl::optional<bool> ContextInfoFetcher::GetThirdPartyBLockingEnabled() {
#if defined(OS_WIN) && BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return g_browser_process->local_state()->GetBoolean(
      prefs::kThirdPartyBlockingEnabled);
#else
  return absl::nullopt;
#endif
}

bool ContextInfoFetcher::GetBuiltInDnsClientEnabled() {
  return g_browser_process->local_state()->GetBoolean(
      prefs::kBuiltInDnsClientEnabled);
}

absl::optional<safe_browsing::PasswordProtectionTrigger>
ContextInfoFetcher::GetPasswordProtectionWarningTrigger() {
  Profile* profile = Profile::FromBrowserContext(browser_context_);
  if (!profile->GetPrefs()->HasPrefPath(
          prefs::kPasswordProtectionWarningTrigger))
    return absl::nullopt;
  return static_cast<safe_browsing::PasswordProtectionTrigger>(
      profile->GetPrefs()->GetInteger(
          prefs::kPasswordProtectionWarningTrigger));
}

absl::optional<bool> ContextInfoFetcher::GetChromeCleanupEnabled() {
#if defined(OS_WIN)
  return g_browser_process->local_state()->GetBoolean(
      prefs::kSwReporterEnabled);
#else
  return absl::nullopt;
#endif
}

bool ContextInfoFetcher::GetChromeRemoteDesktopAppBlocked() {
  return IsURLBlocked(GURL("https://remotedesktop.google.com"),
                      browser_context_) ||
         IsURLBlocked(GURL("https://remotedesktop.corp.google.com"),
                      browser_context_);
}

SettingValue ContextInfoFetcher::GetOSFirewall() {
#if defined(OS_LINUX)
  return GetUfwStatus();
#elif defined(OS_WIN)
  return GetWinOSFirewall();
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  return GetChromeosFirewall();
#else
  return SettingValue::UNKNOWN;
#endif
}

#if defined(OS_LINUX)
ScopedUfwConfigPathForTesting::ScopedUfwConfigPathForTesting(const char* path)
    : initial_path_(*GetUfwConfigPath()) {
  *GetUfwConfigPath() = path;
}

ScopedUfwConfigPathForTesting::~ScopedUfwConfigPathForTesting() {
  *GetUfwConfigPath() = initial_path_;
}
#endif  // defined(OS_LINUX)

}  // namespace enterprise_signals
