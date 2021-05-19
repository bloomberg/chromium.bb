// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libcef/browser/alloy/alloy_browser_context.h"

#include <map>
#include <utility>

#include "libcef/browser/download_manager_delegate.h"
#include "libcef/browser/extensions/extension_system.h"
#include "libcef/browser/prefs/browser_prefs.h"
#include "libcef/browser/ssl_host_state_delegate.h"
#include "libcef/browser/thread_util.h"
#include "libcef/common/cef_switches.h"
#include "libcef/common/extensions/extensions_util.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "chrome/browser/font_family_cache.h"
#include "chrome/browser/media/media_device_id_salt.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/ui/zoom/chrome_zoom_level_prefs.h"
#include "chrome/common/pref_names.h"
#include "components/guest_view/browser/guest_view_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/simple_dependency_manager.h"
#include "components/keyed_service/core/simple_key_map.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/pref_proxy_config_tracker_impl.h"
#include "components/user_prefs/user_prefs.h"
#include "components/visitedlink/browser/visitedlink_event_listener.h"
#include "components/visitedlink/browser/visitedlink_writer.h"
#include "components/zoom/zoom_event_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/process_manager.h"
#include "extensions/common/constants.h"
#include "net/proxy_resolution/proxy_config_service.h"
#include "services/network/public/mojom/cors_origin_pattern.mojom.h"

using content::BrowserThread;

// Creates and manages VisitedLinkEventListener objects for each
// AlloyBrowserContext sharing the same VisitedLinkWriter.
class CefVisitedLinkListener : public visitedlink::VisitedLinkWriter::Listener {
 public:
  CefVisitedLinkListener() { DCHECK(listener_map_.empty()); }

  void CreateListenerForContext(content::BrowserContext* context) {
    CEF_REQUIRE_UIT();
    auto listener =
        std::make_unique<visitedlink::VisitedLinkEventListener>(context);
    listener_map_.insert(std::make_pair(context, std::move(listener)));
  }

  void RemoveListenerForContext(content::BrowserContext* context) {
    CEF_REQUIRE_UIT();
    ListenerMap::iterator it = listener_map_.find(context);
    DCHECK(it != listener_map_.end());
    listener_map_.erase(it);
  }

  // visitedlink::VisitedLinkWriter::Listener methods.

  void NewTable(base::ReadOnlySharedMemoryRegion* table_region) override {
    CEF_REQUIRE_UIT();
    ListenerMap::iterator it = listener_map_.begin();
    for (; it != listener_map_.end(); ++it)
      it->second->NewTable(table_region);
  }

  void Add(visitedlink::VisitedLinkCommon::Fingerprint fingerprint) override {
    CEF_REQUIRE_UIT();
    ListenerMap::iterator it = listener_map_.begin();
    for (; it != listener_map_.end(); ++it)
      it->second->Add(fingerprint);
  }

  void Reset(bool invalidate_hashes) override {
    CEF_REQUIRE_UIT();
    ListenerMap::iterator it = listener_map_.begin();
    for (; it != listener_map_.end(); ++it)
      it->second->Reset(invalidate_hashes);
  }

 private:
  // Map of AlloyBrowserContext to the associated VisitedLinkEventListener.
  typedef std::map<const content::BrowserContext*,
                   std::unique_ptr<visitedlink::VisitedLinkEventListener>>
      ListenerMap;
  ListenerMap listener_map_;

  DISALLOW_COPY_AND_ASSIGN(CefVisitedLinkListener);
};

AlloyBrowserContext::AlloyBrowserContext(
    const CefRequestContextSettings& settings)
    : CefBrowserContext(settings) {}

AlloyBrowserContext::~AlloyBrowserContext() {
  if (resource_context_) {
    content::BrowserThread::DeleteSoon(content::BrowserThread::IO, FROM_HERE,
                                       resource_context_.release());
  }
}

bool AlloyBrowserContext::IsInitialized() const {
  CEF_REQUIRE_UIT();
  return !!key_;
}

void AlloyBrowserContext::StoreOrTriggerInitCallback(
    base::OnceClosure callback) {
  CEF_REQUIRE_UIT();
  // Initialization is always synchronous.
  std::move(callback).Run();
}

void AlloyBrowserContext::Initialize() {
  CefBrowserContext::Initialize();

  key_ = std::make_unique<ProfileKey>(cache_path_);
  SimpleKeyMap::GetInstance()->Associate(this, key_.get());

  // Initialize the PrefService object.
  pref_service_ = browser_prefs::CreatePrefService(
      this, cache_path_, !!settings_.persist_user_preferences);

  // This must be called before creating any services to avoid hitting
  // DependencyManager::AssertContextWasntDestroyed when creating/destroying
  // multiple browser contexts (due to pointer address reuse).
  BrowserContextDependencyManager::GetInstance()->CreateBrowserContextServices(
      this);

  const bool extensions_enabled = extensions::ExtensionsEnabled();
  if (extensions_enabled) {
    // Create the custom ExtensionSystem first because other KeyedServices
    // depend on it.
    extension_system_ = static_cast<extensions::CefExtensionSystem*>(
        extensions::ExtensionSystem::Get(this));
    extension_system_->InitForRegularProfile(true);

    // Make sure the ProcessManager is created so that it receives extension
    // load notifications. This is necessary for the proper initialization of
    // background/event pages.
    extensions::ProcessManager::Get(this);
  }

  // Initialize visited links management.
  base::FilePath visited_link_path;
  if (!cache_path_.empty())
    visited_link_path = cache_path_.Append(FILE_PATH_LITERAL("Visited Links"));
  visitedlink_listener_ = new CefVisitedLinkListener;
  visitedlink_master_.reset(new visitedlink::VisitedLinkWriter(
      visitedlink_listener_, this, !visited_link_path.empty(), false,
      visited_link_path, 0));
  visitedlink_listener_->CreateListenerForContext(this);
  visitedlink_master_->Init();

  // Initialize proxy configuration tracker.
  pref_proxy_config_tracker_.reset(new PrefProxyConfigTrackerImpl(
      GetPrefs(), content::GetIOThreadTaskRunner({})));

  // Spell checking support and possibly other subsystems retrieve the
  // PrefService associated with a BrowserContext via UserPrefs::Get().
  PrefService* pref_service = GetPrefs();
  DCHECK(pref_service);
  user_prefs::UserPrefs::Set(this, pref_service);
  key_->SetPrefs(pref_service);

  if (extensions_enabled)
    extension_system_->Init();

  ChromePluginServiceFilter::GetInstance()->RegisterProfile(this);

  media_device_id_salt_ = new MediaDeviceIDSalt(pref_service);
}

void AlloyBrowserContext::Shutdown() {
  CefBrowserContext::Shutdown();

  // Send notifications to clean up objects associated with this Profile.
  MaybeSendDestroyedNotification();

  ChromePluginServiceFilter::GetInstance()->UnregisterProfile(this);

  // Remove any BrowserContextKeyedServiceFactory associations. This must be
  // called before the ProxyService owned by AlloyBrowserContext is destroyed.
  // The SimpleDependencyManager should always be passed after the
  // BrowserContextDependencyManager. This is because the KeyedService instances
  // in the BrowserContextDependencyManager's dependency graph can depend on the
  // ones in the SimpleDependencyManager's graph.
  DependencyManager::PerformInterlockedTwoPhaseShutdown(
      BrowserContextDependencyManager::GetInstance(), this,
      SimpleDependencyManager::GetInstance(), key_.get());

  key_.reset();
  SimpleKeyMap::GetInstance()->Dissociate(this);

  // Shuts down the storage partitions associated with this browser context.
  // This must be called before the browser context is actually destroyed
  // and before a clean-up task for its corresponding IO thread residents
  // (e.g. ResourceContext) is posted, so that the classes that hung on
  // StoragePartition can have time to do necessary cleanups on IO thread.
  ShutdownStoragePartitions();

  visitedlink_listener_->RemoveListenerForContext(this);

  // The FontFamilyCache references the ProxyService so delete it before the
  // ProxyService is deleted.
  SetUserData(&kFontFamilyCacheKey, nullptr);

  pref_proxy_config_tracker_->DetachFromPrefService();

  // Delete the download manager delegate here because otherwise we'll crash
  // when it's accessed from the content::BrowserContext destructor.
  if (download_manager_delegate_)
    download_manager_delegate_.reset(nullptr);
}

void AlloyBrowserContext::RemoveCefRequestContext(
    CefRequestContextImpl* context) {
  CEF_REQUIRE_UIT();

  if (extensions::ExtensionsEnabled()) {
    extension_system()->OnRequestContextDeleted(context);
  }

  // May result in |this| being deleted.
  CefBrowserContext::RemoveCefRequestContext(context);
}

void AlloyBrowserContext::LoadExtension(
    const CefString& root_directory,
    CefRefPtr<CefDictionaryValue> manifest,
    CefRefPtr<CefExtensionHandler> handler,
    CefRefPtr<CefRequestContext> loader_context) {
  if (!extensions::ExtensionsEnabled()) {
    if (handler)
      handler->OnExtensionLoadFailed(ERR_ABORTED);
    return;
  }

  if (manifest && manifest->GetSize() > 0) {
    CefDictionaryValueImpl* value_impl =
        static_cast<CefDictionaryValueImpl*>(manifest.get());
    extension_system()->LoadExtension(base::WrapUnique(value_impl->CopyValue()),
                                      root_directory, false /* builtin */,
                                      loader_context, handler);
  } else {
    extension_system()->LoadExtension(root_directory, false /* builtin */,
                                      loader_context, handler);
  }
}

bool AlloyBrowserContext::GetExtensions(std::vector<CefString>& extension_ids) {
  if (!extensions::ExtensionsEnabled())
    return false;

  extensions::CefExtensionSystem::ExtensionMap extension_map =
      extension_system()->GetExtensions();
  extensions::CefExtensionSystem::ExtensionMap::const_iterator it =
      extension_map.begin();
  for (; it != extension_map.end(); ++it)
    extension_ids.push_back(it->second->GetIdentifier());

  return true;
}

CefRefPtr<CefExtension> AlloyBrowserContext::GetExtension(
    const CefString& extension_id) {
  if (!extensions::ExtensionsEnabled())
    return nullptr;

  return extension_system()->GetExtension(extension_id);
}

bool AlloyBrowserContext::UnloadExtension(const CefString& extension_id) {
  DCHECK(extensions::ExtensionsEnabled());
  return extension_system()->UnloadExtension(extension_id);
}

bool AlloyBrowserContext::IsPrintPreviewSupported() const {
  CEF_REQUIRE_UIT();
  if (!extensions::PrintPreviewEnabled())
    return false;

  return !GetPrefs()->GetBoolean(prefs::kPrintPreviewDisabled);
}

content::ResourceContext* AlloyBrowserContext::GetResourceContext() {
  if (!resource_context_) {
    resource_context_ = std::make_unique<content::ResourceContext>();
  }
  return resource_context_.get();
}

content::ClientHintsControllerDelegate*
AlloyBrowserContext::GetClientHintsControllerDelegate() {
  return nullptr;
}

void AlloyBrowserContext::SetCorsOriginAccessListForOrigin(
    TargetBrowserContexts target_mode,
    const url::Origin& source_origin,
    std::vector<network::mojom::CorsOriginPatternPtr> allow_patterns,
    std::vector<network::mojom::CorsOriginPatternPtr> block_patterns,
    base::OnceClosure closure) {
  // This method is called for Extension support.
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, std::move(closure));
}

ChromeZoomLevelPrefs* AlloyBrowserContext::GetZoomLevelPrefs() {
  return static_cast<ChromeZoomLevelPrefs*>(
      GetStoragePartition(this, nullptr)->GetZoomLevelDelegate());
}

scoped_refptr<network::SharedURLLoaderFactory>
AlloyBrowserContext::GetURLLoaderFactory() {
  return GetDefaultStoragePartition(this)
      ->GetURLLoaderFactoryForBrowserProcess();
}

base::FilePath AlloyBrowserContext::GetPath() {
  return cache_path_;
}

base::FilePath AlloyBrowserContext::GetPath() const {
  return cache_path_;
}

std::unique_ptr<content::ZoomLevelDelegate>
AlloyBrowserContext::CreateZoomLevelDelegate(
    const base::FilePath& partition_path) {
  if (cache_path_.empty())
    return std::unique_ptr<content::ZoomLevelDelegate>();

  return base::WrapUnique(new ChromeZoomLevelPrefs(
      GetPrefs(), cache_path_, partition_path,
      zoom::ZoomEventManager::GetForBrowserContext(this)->GetWeakPtr()));
}

bool AlloyBrowserContext::IsOffTheRecord() const {
  // Alloy contexts are never flagged as off-the-record. It causes problems
  // for the extension system.
  return false;
}

content::DownloadManagerDelegate*
AlloyBrowserContext::GetDownloadManagerDelegate() {
  if (!download_manager_delegate_) {
    content::DownloadManager* manager =
        BrowserContext::GetDownloadManager(this);
    download_manager_delegate_.reset(new CefDownloadManagerDelegate(manager));
  }
  return download_manager_delegate_.get();
}

content::BrowserPluginGuestManager* AlloyBrowserContext::GetGuestManager() {
  DCHECK(extensions::ExtensionsEnabled());
  return guest_view::GuestViewManager::FromBrowserContext(this);
}

storage::SpecialStoragePolicy* AlloyBrowserContext::GetSpecialStoragePolicy() {
  return nullptr;
}

content::PushMessagingService* AlloyBrowserContext::GetPushMessagingService() {
  return nullptr;
}

content::StorageNotificationService*
AlloyBrowserContext::GetStorageNotificationService() {
  return nullptr;
}

content::SSLHostStateDelegate* AlloyBrowserContext::GetSSLHostStateDelegate() {
  if (!ssl_host_state_delegate_)
    ssl_host_state_delegate_.reset(new CefSSLHostStateDelegate());
  return ssl_host_state_delegate_.get();
}

content::PermissionControllerDelegate*
AlloyBrowserContext::GetPermissionControllerDelegate() {
  return nullptr;
}

content::BackgroundFetchDelegate*
AlloyBrowserContext::GetBackgroundFetchDelegate() {
  return nullptr;
}

content::BackgroundSyncController*
AlloyBrowserContext::GetBackgroundSyncController() {
  return nullptr;
}

content::BrowsingDataRemoverDelegate*
AlloyBrowserContext::GetBrowsingDataRemoverDelegate() {
  return nullptr;
}

std::string AlloyBrowserContext::GetMediaDeviceIDSalt() {
  return media_device_id_salt_->GetSalt();
}

PrefService* AlloyBrowserContext::GetPrefs() {
  return pref_service_.get();
}

const PrefService* AlloyBrowserContext::GetPrefs() const {
  return pref_service_.get();
}

ProfileKey* AlloyBrowserContext::GetProfileKey() const {
  DCHECK(key_);
  return key_.get();
}

policy::SchemaRegistryService*
AlloyBrowserContext::GetPolicySchemaRegistryService() {
  NOTREACHED();
  return nullptr;
}

policy::UserCloudPolicyManager*
AlloyBrowserContext::GetUserCloudPolicyManager() {
  NOTREACHED();
  return nullptr;
}

policy::ProfilePolicyConnector*
AlloyBrowserContext::GetProfilePolicyConnector() {
  NOTREACHED();
  return nullptr;
}

const policy::ProfilePolicyConnector*
AlloyBrowserContext::GetProfilePolicyConnector() const {
  NOTREACHED();
  return nullptr;
}

bool AlloyBrowserContext::IsNewProfile() const {
  NOTREACHED();
  return false;
}

void AlloyBrowserContext::RebuildTable(
    const scoped_refptr<URLEnumerator>& enumerator) {
  // Called when visited links will not or cannot be loaded from disk.
  enumerator->OnComplete(true);
}

DownloadPrefs* AlloyBrowserContext::GetDownloadPrefs() {
  CEF_REQUIRE_UIT();
  if (!download_prefs_) {
    download_prefs_.reset(new DownloadPrefs(this));
  }
  return download_prefs_.get();
}

void AlloyBrowserContext::AddVisitedURLs(const std::vector<GURL>& urls) {
  visitedlink_master_->AddURLs(urls);
}
