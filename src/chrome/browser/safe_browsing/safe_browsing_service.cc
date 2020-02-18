// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_service.h"

#include <stddef.h>

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/threading/thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager.h"
#include "chrome/browser/safe_browsing/ui_manager.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/safe_browsing/file_type_policies.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/browser/safe_browsing_network_context.h"
#include "components/safe_browsing/browser/safe_browsing_url_request_context_getter.h"
#include "components/safe_browsing/common/safebrowsing_constants.h"
#include "components/safe_browsing/db/database_manager.h"
#include "components/safe_browsing/ping_manager.h"
#include "components/safe_browsing/triggers/trigger_manager.h"
#include "components/safe_browsing/verdict_cache_manager.h"
#include "components/safe_browsing/web_ui/safe_browsing_ui.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/resource_request_info.h"
#include "services/network/public/cpp/cross_thread_shared_url_loader_factory_info.h"
#include "services/network/public/cpp/features.h"
#include "services/preferences/public/mojom/tracked_preference_validation_delegate.mojom.h"

#if defined(OS_WIN)
#include "chrome/install_static/install_util.h"
#endif

#if defined(FULL_SAFE_BROWSING)
#include "chrome/browser/safe_browsing/client_side_detection_service.h"
#include "chrome/browser/safe_browsing/download_protection/download_protection_service.h"
#include "chrome/browser/safe_browsing/incident_reporting/binary_integrity_analyzer.h"
#include "chrome/browser/safe_browsing/incident_reporting/incident_reporting_service.h"
#include "chrome/browser/safe_browsing/incident_reporting/resource_request_detector.h"
#include "components/safe_browsing/password_protection/password_protection_service.h"
#endif

using content::BrowserThread;
using content::NonNestable;

namespace safe_browsing {

#if defined(FULL_SAFE_BROWSING)
namespace {

// 50 was chosen as an arbitrary upper bound on the likely font sizes. For
// reference, the "Font size" dropdown in settings lets you select between 9,
// 12, 16, 20, or 24.
const int kMaximumTrackedFontSize = 50;

void RecordFontSizeMetrics(const PrefService& pref_service) {
  UMA_HISTOGRAM_EXACT_LINEAR(
      "SafeBrowsing.FontSize.Default",
      pref_service.GetInteger(prefs::kWebKitDefaultFontSize),
      kMaximumTrackedFontSize);
  UMA_HISTOGRAM_EXACT_LINEAR(
      "SafeBrowsing.FontSize.DefaultFixed",
      pref_service.GetInteger(prefs::kWebKitDefaultFixedFontSize),
      kMaximumTrackedFontSize);
  UMA_HISTOGRAM_EXACT_LINEAR(
      "SafeBrowsing.FontSize.Minimum",
      pref_service.GetInteger(prefs::kWebKitMinimumFontSize),
      kMaximumTrackedFontSize);
  UMA_HISTOGRAM_EXACT_LINEAR(
      "SafeBrowsing.FontSize.MinimumLogical",
      pref_service.GetInteger(prefs::kWebKitMinimumLogicalFontSize),
      kMaximumTrackedFontSize);
}

}  // namespace
#endif

// static
base::FilePath SafeBrowsingService::GetCookieFilePathForTesting() {
  return base::FilePath(SafeBrowsingService::GetBaseFilename().value() +
                        safe_browsing::kCookiesFile);
}

// static
base::FilePath SafeBrowsingService::GetBaseFilename() {
  base::FilePath path;
  bool result = base::PathService::Get(chrome::DIR_USER_DATA, &path);
  DCHECK(result);
  return path.Append(safe_browsing::kSafeBrowsingBaseFilename);
}

SafeBrowsingService::SafeBrowsingService()
    : services_delegate_(ServicesDelegate::Create(this)),
      estimated_extended_reporting_by_prefs_(SBER_LEVEL_OFF),
      shutdown_(false),
      enabled_(false),
      enabled_by_prefs_(false) {}

SafeBrowsingService::~SafeBrowsingService() {
  // We should have already been shut down. If we're still enabled, then the
  // database isn't going to be closed properly, which could lead to corruption.
  DCHECK(!enabled_);
}

void SafeBrowsingService::Initialize() {
  // Ensure FileTypePolicies's Singleton is instantiated during startup.
  // This guarantees we'll log UMA metrics about its state.
  FileTypePolicies::GetInstance();

  base::FilePath user_data_dir;
  bool result = base::PathService::Get(chrome::DIR_USER_DATA, &user_data_dir);
  DCHECK(result);

  network_context_ =
      std::make_unique<safe_browsing::SafeBrowsingNetworkContext>(
          nullptr, user_data_dir,
          base::BindRepeating(&SafeBrowsingService::CreateNetworkContextParams,
                              base::Unretained(this)));

  WebUIInfoSingleton::GetInstance()->set_network_context(
      network_context_.get());

  ui_manager_ = CreateUIManager();

  navigation_observer_manager_ = new SafeBrowsingNavigationObserverManager();

  services_delegate_->Initialize();
  services_delegate_->InitializeCsdService(GetURLLoaderFactory());

  // Needs to happen after |ui_manager_| is created.
  CreateTriggerManager();

  // Track profile creation and destruction.
  profiles_registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CREATED,
                          content::NotificationService::AllSources());
  profiles_registrar_.Add(this, chrome::NOTIFICATION_PROFILE_DESTROYED,
                          content::NotificationService::AllSources());

  // Register all the delayed analysis to the incident reporting service.
  RegisterAllDelayedAnalysis();
}

void SafeBrowsingService::ShutDown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  shutdown_ = true;

  // Remove Profile creation/destruction observers.
  profiles_registrar_.RemoveAll();

  // Delete the PrefChangeRegistrars, whose dtors also unregister |this| as an
  // observer of the preferences.
  prefs_map_.clear();

  Stop(true);

  services_delegate_->ShutdownServices();

  network_context_->ServiceShuttingDown();
  proxy_config_monitor_.reset();
}

network::mojom::NetworkContext* SafeBrowsingService::GetNetworkContext() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return network_context_->GetNetworkContext();
}

scoped_refptr<network::SharedURLLoaderFactory>
SafeBrowsingService::GetURLLoaderFactory() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!network_context_)
    return nullptr;
  return network_context_->GetURLLoaderFactory();
}

void SafeBrowsingService::FlushNetworkInterfaceForTesting() {
  if (network_context_)
    network_context_->FlushForTesting();
}

const scoped_refptr<SafeBrowsingUIManager>& SafeBrowsingService::ui_manager()
    const {
  return ui_manager_;
}

const scoped_refptr<SafeBrowsingDatabaseManager>&
SafeBrowsingService::database_manager() const {
  return services_delegate_->database_manager();
}

scoped_refptr<SafeBrowsingNavigationObserverManager>
SafeBrowsingService::navigation_observer_manager() {
  return navigation_observer_manager_;
}

PingManager* SafeBrowsingService::ping_manager() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return ping_manager_.get();
}

TriggerManager* SafeBrowsingService::trigger_manager() const {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return trigger_manager_.get();
}

PasswordProtectionService* SafeBrowsingService::GetPasswordProtectionService(
    Profile* profile) const {
  if (profile->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled))
    return services_delegate_->GetPasswordProtectionService(profile);
  return nullptr;
}

std::unique_ptr<prefs::mojom::TrackedPreferenceValidationDelegate>
SafeBrowsingService::CreatePreferenceValidationDelegate(
    Profile* profile) const {
  return services_delegate_->CreatePreferenceValidationDelegate(profile);
}

void SafeBrowsingService::RegisterDelayedAnalysisCallback(
    const DelayedAnalysisCallback& callback) {
  services_delegate_->RegisterDelayedAnalysisCallback(callback);
}

void SafeBrowsingService::AddDownloadManager(
    content::DownloadManager* download_manager) {
  services_delegate_->AddDownloadManager(download_manager);
}

SafeBrowsingUIManager* SafeBrowsingService::CreateUIManager() {
  return new SafeBrowsingUIManager(this);
}

void SafeBrowsingService::RegisterAllDelayedAnalysis() {
#if defined(FULL_SAFE_BROWSING)
  RegisterBinaryIntegrityAnalysis();
#endif
}

V4ProtocolConfig SafeBrowsingService::GetV4ProtocolConfig() const {
  base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  return ::safe_browsing::GetV4ProtocolConfig(
      GetProtocolConfigClientName(),
      cmdline->HasSwitch(::switches::kDisableBackgroundNetworking));
}

VerdictCacheManager* SafeBrowsingService::GetVerdictCacheManager(
    Profile* profile) const {
  if (profile->GetPrefs()->GetBoolean(prefs::kSafeBrowsingEnabled))
    return services_delegate_->GetVerdictCacheManager(profile);
  return nullptr;
}

std::string SafeBrowsingService::GetProtocolConfigClientName() const {
  std::string client_name;
  // On Windows, get the safe browsing client name from the browser
  // distribution classes in installer util. These classes don't yet have
  // an analog on non-Windows builds so just keep the name specified here.
#if defined(OS_WIN)
  client_name = install_static::GetSafeBrowsingName();
#else
#if defined(GOOGLE_CHROME_BUILD)
  client_name = "googlechrome";
#else
  client_name = "chromium";
#endif

  // Mark client string to allow server to differentiate mobile.
#if defined(OS_ANDROID)
  client_name.append("-a");
#endif

#endif  // defined(OS_WIN)

  return client_name;
}

void SafeBrowsingService::SetDatabaseManagerForTest(
    SafeBrowsingDatabaseManager* database_manager) {
  services_delegate_->SetDatabaseManagerForTest(database_manager);
}

void SafeBrowsingService::StartOnIOThread(
    std::unique_ptr<network::SharedURLLoaderFactoryInfo> url_loader_factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (enabled_)
    return;
  enabled_ = true;

  V4ProtocolConfig v4_config = GetV4ProtocolConfig();

  services_delegate_->StartOnIOThread(
      network::SharedURLLoaderFactory::Create(std::move(url_loader_factory)),
      v4_config);
}

void SafeBrowsingService::StopOnIOThread(bool shutdown) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  services_delegate_->StopOnIOThread(shutdown);

  if (enabled_) {
    enabled_ = false;
  }
}

void SafeBrowsingService::Start() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!ping_manager_) {
    ping_manager_ =
        PingManager::Create(GetURLLoaderFactory(), GetV4ProtocolConfig());
  }

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(
          &SafeBrowsingService::StartOnIOThread, this,
          std::make_unique<network::CrossThreadSharedURLLoaderFactoryInfo>(
              GetURLLoaderFactory())));
}

void SafeBrowsingService::Stop(bool shutdown) {
  ping_manager_.reset();
  ui_manager_->Stop(shutdown);
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&SafeBrowsingService::StopOnIOThread, this, shutdown));
}

void SafeBrowsingService::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_CREATED: {
      DCHECK_CURRENTLY_ON(BrowserThread::UI);
      Profile* profile = content::Source<Profile>(source).ptr();
      services_delegate_->CreateVerdictCacheManager(profile);
      services_delegate_->CreatePasswordProtectionService(profile);
      services_delegate_->CreateTelemetryService(profile);
      if (!profile->IsOffTheRecord())
        AddPrefService(profile->GetPrefs());
      break;
    }
    case chrome::NOTIFICATION_PROFILE_DESTROYED: {
      DCHECK_CURRENTLY_ON(BrowserThread::UI);
      Profile* profile = content::Source<Profile>(source).ptr();
      services_delegate_->RemoveVerdictCacheManager(profile);
      services_delegate_->RemovePasswordProtectionService(profile);
      services_delegate_->RemoveTelemetryService();
      if (!profile->IsOffTheRecord())
        RemovePrefService(profile->GetPrefs());
      break;
    }
    default:
      NOTREACHED();
  }
}

void SafeBrowsingService::AddPrefService(PrefService* pref_service) {
  DCHECK(prefs_map_.find(pref_service) == prefs_map_.end());
  std::unique_ptr<PrefChangeRegistrar> registrar =
      std::make_unique<PrefChangeRegistrar>();
  registrar->Init(pref_service);
  registrar->Add(
      prefs::kSafeBrowsingEnabled,
      base::Bind(&SafeBrowsingService::RefreshState, base::Unretained(this)));
  // ClientSideDetectionService will need to be refresh the models
  // renderers have if extended-reporting changes.
  registrar->Add(
      prefs::kSafeBrowsingScoutReportingEnabled,
      base::Bind(&SafeBrowsingService::RefreshState, base::Unretained(this)));
  prefs_map_[pref_service] = std::move(registrar);
  RefreshState();

  // Record the current pref state.
  UMA_HISTOGRAM_BOOLEAN("SafeBrowsing.Pref.General",
                        pref_service->GetBoolean(prefs::kSafeBrowsingEnabled));
  // Extended Reporting metrics are handled together elsewhere.
  RecordExtendedReportingMetrics(*pref_service);

#if defined(FULL_SAFE_BROWSING)
  RecordFontSizeMetrics(*pref_service);
#endif
}

void SafeBrowsingService::RemovePrefService(PrefService* pref_service) {
  if (prefs_map_.find(pref_service) != prefs_map_.end()) {
    prefs_map_.erase(pref_service);
    RefreshState();
  } else {
    NOTREACHED();
  }
}

std::unique_ptr<SafeBrowsingService::StateSubscription>
SafeBrowsingService::RegisterStateCallback(
    const base::Callback<void(void)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return state_callback_list_.Add(callback);
}

void SafeBrowsingService::RefreshState() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // Check if any profile requires the service to be active.
  enabled_by_prefs_ = false;
  estimated_extended_reporting_by_prefs_ = SBER_LEVEL_OFF;
  for (const auto& pref : prefs_map_) {
    if (pref.first->GetBoolean(prefs::kSafeBrowsingEnabled)) {
      enabled_by_prefs_ = true;
      ExtendedReportingLevel erl =
          safe_browsing::GetExtendedReportingLevel(*pref.first);
      if (erl != SBER_LEVEL_OFF) {
        estimated_extended_reporting_by_prefs_ = erl;
        break;
      }
    }
  }

  if (enabled_by_prefs_)
    Start();
  else
    Stop(false);

  state_callback_list_.Notify();

  services_delegate_->RefreshState(enabled_by_prefs_);
}

void SafeBrowsingService::SendSerializedDownloadReport(
    const std::string& report) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (ping_manager())
    ping_manager()->ReportThreatDetails(report);
}

void SafeBrowsingService::CreateTriggerManager() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  trigger_manager_ = std::make_unique<TriggerManager>(
      ui_manager_.get(), navigation_observer_manager_.get(),
      g_browser_process->local_state());
}

network::mojom::NetworkContextParamsPtr
SafeBrowsingService::CreateNetworkContextParams() {
  auto params = SystemNetworkContextManager::GetInstance()
                    ->CreateDefaultNetworkContextParams();
  if (!proxy_config_monitor_) {
    proxy_config_monitor_ =
        std::make_unique<ProxyConfigMonitor>(g_browser_process->local_state());
  }
  proxy_config_monitor_->AddToNetworkContextParams(params.get());
  return params;
}

// The default SafeBrowsingServiceFactory.  Global, made a singleton so we
// don't leak it.
class SafeBrowsingServiceFactoryImpl : public SafeBrowsingServiceFactory {
 public:
  // TODO(crbug/925153): Once callers of this function are no longer downcasting
  // it to the SafeBrowsingService, we can make this a scoped_refptr.
  SafeBrowsingServiceInterface* CreateSafeBrowsingService() override {
    return new SafeBrowsingService();
  }

 private:
  friend class base::NoDestructor<SafeBrowsingServiceFactoryImpl>;

  SafeBrowsingServiceFactoryImpl() {}

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingServiceFactoryImpl);
};

SafeBrowsingServiceFactory* GetSafeBrowsingServiceFactory() {
  static base::NoDestructor<SafeBrowsingServiceFactoryImpl> factory;
  return factory.get();
}

}  // namespace safe_browsing
