// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_REGISTRAR_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_REGISTRAR_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "chrome/browser/profiles/profile_manager_observer.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_id.h"
#include "chrome/browser/web_applications/components/web_application_info.h"
#include "components/services/app_service/public/cpp/file_handler.h"
#include "components/services/app_service/public/cpp/protocol_handler_info.h"
#include "components/services/app_service/public/cpp/url_handler_info.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace web_app {

class AppRegistrarObserver;
class WebApp;
class OsIntegrationManager;

using Registry = std::map<AppId, std::unique_ptr<WebApp>>;

// A registry model. This is a read-only container, which owns WebApp objects.
class WebAppRegistrar : public ProfileManagerObserver {
 public:
  explicit WebAppRegistrar(Profile* profile);
  WebAppRegistrar(const WebAppRegistrar&) = delete;
  WebAppRegistrar& operator=(const WebAppRegistrar&) = delete;
  ~WebAppRegistrar() override;

  bool is_empty() const { return registry_.empty(); }

  const WebApp* GetAppById(const AppId& app_id) const;

  // TODO(https://crbug.com/1182363): should be removed when id is introduced to
  // manifest.
  const WebApp* GetAppByStartUrl(const GURL& start_url) const;
  std::vector<AppId> GetAppsFromSyncAndPendingInstallation();

  // Returns true if the app was preinstalled and NOT installed via any other
  // mechanism.
  bool WasInstalledByDefaultOnly(const AppId& app_id) const;

  void Start();
  void Shutdown();

  // Returns whether the app with |app_id| is currently listed in the registry.
  // ie. we have data for web app manifest and icons, and this |app_id| can be
  // used in other registrar methods.
  bool IsInstalled(const AppId& app_id) const;

  // Returns whether the app is currently being uninstalled. This will be true
  // after uninstall has begun but before the OS integration hooks for uninstall
  // have completed. It will return false after uninstallation has completed.
  // Note that the underlying field this checks is not yet persisted to the
  // database; see https://crbug.com/1162477
  bool IsUninstalling(const AppId& app_id) const;

  // Returns whether the app with |app_id| is currently fully locally installed.
  // ie. app is not grey in chrome://apps UI surface and may have OS integration
  // like shortcuts. |IsLocallyInstalled| apps is a subset of |IsInstalled|
  // apps. On Chrome OS all apps are always locally installed.
  bool IsLocallyInstalled(const AppId& app_id) const;

  // Returns true if the app was installed by user, false if default installed.
  bool WasInstalledByUser(const AppId& app_id) const;

  // Returns true if the app was installed by the device OEM. Always false on
  // on non-Chrome OS.
  bool WasInstalledByOem(const AppId& app_id) const;

  // Returns the AppIds and URLs of apps externally installed from
  // |install_source|.
  std::map<AppId, GURL> GetExternallyInstalledApps(
      ExternalInstallSource install_source) const;

  // Returns the app id for |install_url| if the WebAppRegistrar is aware of an
  // externally installed app for it. Note that the |install_url| is the URL
  // that the app was installed from, which may not necessarily match the app's
  // current start URL.
  absl::optional<AppId> LookupExternalAppId(const GURL& install_url) const;

  // Returns whether the WebAppRegistrar has an externally installed app with
  // |app_id| from any |install_source|.
  bool HasExternalApp(const AppId& app_id) const;

  // Returns whether the WebAppRegistrar has an externally installed app with
  // |app_id| from |install_source|.
  bool HasExternalAppWithInstallSource(
      const AppId& app_id,
      ExternalInstallSource install_source) const;

  // Returns true if the web app with the |app_id| contains |protocol_scheme|
  // as one of its approved launch protocols.
  bool IsApprovedLaunchProtocol(const AppId& app_id,
                                std::string protocol_scheme) const;

  // Count a number of all apps which are installed by user (non-default).
  // Requires app registry to be in a ready state.
  int CountUserInstalledApps() const;

  // All names are UTF8 encoded.
  std::string GetAppShortName(const AppId& app_id) const;
  std::string GetAppDescription(const AppId& app_id) const;
  absl::optional<SkColor> GetAppThemeColor(const AppId& app_id) const;
  absl::optional<SkColor> GetAppBackgroundColor(const AppId& app_id) const;
  const GURL& GetAppStartUrl(const AppId& app_id) const;
  absl::optional<std::string> GetAppManifestId(const AppId& app_id) const;
  const std::string* GetAppLaunchQueryParams(const AppId& app_id) const;
  const apps::ShareTarget* GetAppShareTarget(const AppId& app_id) const;
  blink::mojom::CaptureLinks GetAppCaptureLinks(const AppId& app_id) const;
  const apps::FileHandlers* GetAppFileHandlers(const AppId& app_id) const;
  const apps::ProtocolHandlers* GetAppProtocolHandlers(
      const AppId& app_id) const;
  bool IsAppFileHandlerPermissionBlocked(const web_app::AppId& app_id) const;

  // Returns the start_url with launch_query_params appended to the end if any.
  GURL GetAppLaunchUrl(const AppId& app_id) const;

  // TODO(crbug.com/910016): Replace uses of this with GetAppScope().
  absl::optional<GURL> GetAppScopeInternal(const AppId& app_id) const;

  DisplayMode GetAppDisplayMode(const AppId& app_id) const;
  DisplayMode GetAppUserDisplayMode(const AppId& app_id) const;
  std::vector<DisplayMode> GetAppDisplayModeOverride(const AppId& app_id) const;

  // Returns the "url_handlers" field from the app manifest.
  apps::UrlHandlers GetAppUrlHandlers(const AppId& app_id) const;

  GURL GetAppManifestUrl(const AppId& app_id) const;

  base::Time GetAppLastBadgingTime(const AppId& app_id) const;
  base::Time GetAppLastLaunchTime(const AppId& app_id) const;
  base::Time GetAppInstallTime(const AppId& app_id) const;

  // Returns the "icons" field from the app manifest, use |WebAppIconManager| to
  // load icon bitmap data.
  std::vector<WebApplicationIconInfo> GetAppIconInfos(
      const AppId& app_id) const;

  // Represents which icon sizes we successfully downloaded from the IconInfos.
  SortedSizesPx GetAppDownloadedIconSizesAny(const AppId& app_id) const;

  // Returns the "shortcuts" field from the app manifest, use
  // |WebAppIconManager| to load shortcuts menu icons bitmaps data.
  std::vector<WebApplicationShortcutsMenuItemInfo> GetAppShortcutsMenuItemInfos(
      const AppId& app_id) const;

  // Represents which icon sizes we successfully downloaded from the
  // ShortcutsMenuItemInfos.
  std::vector<IconSizes> GetAppDownloadedShortcutsMenuIconsSizes(
      const AppId& app_id) const;

  // Returns the Run on OS Login mode.
  RunOnOsLoginMode GetAppRunOnOsLoginMode(const AppId& app_id) const;

  bool GetWindowControlsOverlayEnabled(const AppId& app_id) const;

  std::vector<AppId> GetAppIds() const;

  void SetSubsystems(OsIntegrationManager* os_integration_manager);

  // Returns the "scope" field from the app manifest, or infers a scope from the
  // "start_url" field if unavailable. Returns an invalid GURL iff the |app_id|
  // does not refer to an installed web app.
  GURL GetAppScope(const AppId& app_id) const;

  // Returns the app id of an app in the registry with the longest scope that is
  // a prefix of |url|, if any.
  absl::optional<AppId> FindAppWithUrlInScope(const GURL& url) const;

  // Returns true if there exists at least one app installed under |scope|.
  bool DoesScopeContainAnyApp(const GURL& scope) const;

  // Finds all apps that are installed under |scope|.
  std::vector<AppId> FindAppsInScope(const GURL& scope) const;

  // Returns the app id of an installed app in the registry with the longest
  // scope that is a prefix of |url|, if any. If |window_only| is specified,
  // only apps that open in app windows will be considered.
  absl::optional<AppId> FindInstalledAppWithUrlInScope(
      const GURL& url,
      bool window_only = false) const;

  // Returns whether the app is a shortcut app (as opposed to a PWA).
  bool IsShortcutApp(const AppId& app_id) const;

  // Returns true if the app with the specified |start_url| is currently fully
  // locally installed. The provided |start_url| must exactly match the launch
  // URL for the app; this method does not consult the app scope or match URLs
  // that fall within the scope.
  bool IsLocallyInstalled(const GURL& start_url) const;

  // Returns whether the app is pending successful navigation in order to
  // complete installation via the ExternallyManagedAppManager.
  bool IsPlaceholderApp(const AppId& app_id) const;

  // Computes and returns the DisplayMode, accounting for user preference
  // to launch in a browser window and entries in the web app manifest.
  DisplayMode GetAppEffectiveDisplayMode(const AppId& app_id) const;

  // Computes and returns the DisplayMode only accounting for
  // entries in the web app manifest.
  DisplayMode GetEffectiveDisplayModeFromManifest(const AppId& app_id) const;

  // Returns whether the app should be opened in tabbed window mode.
  bool IsTabbedWindowModeEnabled(const AppId& app_id) const;

  // TODO(crbug.com/897314): This can be removed once feature has launched.
  bool IsInExperimentalTabbedWindowMode(const AppId& app_id) const;

  void AddObserver(AppRegistrarObserver* observer);
  void RemoveObserver(AppRegistrarObserver* observer);

  void NotifyWebAppInstalled(const AppId& app_id);
  void NotifyWebAppManifestUpdated(const AppId& app_id,
                                   base::StringPiece old_name);
  void NotifyWebAppsWillBeUpdatedFromSync(
      const std::vector<const WebApp*>& new_apps_state);
  void NotifyWebAppUninstalled(const AppId& app_id);
  void NotifyWebAppWillBeUninstalled(const AppId& app_id);
  void NotifyWebAppLocallyInstalledStateChanged(const AppId& app_id,
                                                bool is_locally_installed);
  void NotifyWebAppDisabledStateChanged(const AppId& app_id, bool is_disabled);
  void NotifyWebAppsDisabledModeChanged();
  void NotifyWebAppLastLaunchTimeChanged(const AppId& app_id,
                                         const base::Time& time);
  void NotifyWebAppInstallTimeChanged(const AppId& app_id,
                                      const base::Time& time);

  // Notify when OS hooks installation is finished during Web App installation.
  void NotifyWebAppInstalledWithOsHooks(const AppId& app_id);
  void NotifyWebAppUserDisplayModeChanged(const AppId& app_id,
                                          DisplayMode user_display_mode);
  void NotifyWebAppExperimentalTabbedWindowModeChanged(const AppId& app_id,
                                                       bool enabled);

  // ProfileManagerObserver:
  void OnProfileMarkedForPermanentDeletion(
      Profile* profile_to_be_deleted) override;

  // A filter must return false to skip the |web_app|.
  using Filter = bool (*)(const WebApp& web_app);

  // Only range-based |for| loop supported. Don't use AppSet directly.
  // Doesn't support registration and unregistration of WebApp while iterating.
  class AppSet {
   public:
    // An iterator class that can be used to access the list of apps.
    template <typename WebAppType>
    class Iter {
     public:
      using InternalIter = Registry::const_iterator;

      Iter(InternalIter&& internal_iter,
           InternalIter&& internal_end,
           Filter filter)
          : internal_iter_(std::move(internal_iter)),
            internal_end_(std::move(internal_end)),
            filter_(filter) {
        FilterAndSkipApps();
      }
      Iter(Iter&&) = default;
      Iter(const Iter&) = delete;
      Iter& operator=(const Iter&) = delete;
      ~Iter() = default;

      void operator++() {
        ++internal_iter_;
        FilterAndSkipApps();
      }
      WebAppType& operator*() const { return *internal_iter_->second.get(); }
      bool operator!=(const Iter& iter) const {
        return internal_iter_ != iter.internal_iter_;
      }

     private:
      void FilterAndSkipApps() {
        if (!filter_)
          return;

        while (internal_iter_ != internal_end_ && !filter_(**this))
          ++internal_iter_;
      }

      InternalIter internal_iter_;
      InternalIter internal_end_;
      Filter filter_;
    };

    AppSet(const WebAppRegistrar* registrar, Filter filter);
    AppSet(AppSet&&) = default;
    AppSet(const AppSet&) = delete;
    AppSet& operator=(const AppSet&) = delete;
    ~AppSet();

    using iterator = Iter<WebApp>;
    using const_iterator = Iter<const WebApp>;

    iterator begin();
    iterator end();
    const_iterator begin() const;
    const_iterator end() const;

   private:
    const WebAppRegistrar* const registrar_;
    const Filter filter_;
#if DCHECK_IS_ON()
    const size_t mutations_count_;
#endif
  };

  // Returns all apps in the registry (a superset) including stubs.
  const AppSet GetAppsIncludingStubs() const;
  // Returns all apps excluding stubs for apps in sync install. Apps in sync
  // install are being installed and should be hidden for most subsystems. This
  // is a subset of GetAppsIncludingStubs().
  const AppSet GetApps() const;

 protected:
  Profile* profile() const { return profile_; }
  OsIntegrationManager& os_integration_manager() {
    return *os_integration_manager_;
  }

  void NotifyWebAppProfileWillBeDeleted(const AppId& app_id);
  void NotifyAppRegistrarShutdown();

  Registry& registry() { return registry_; }
  void SetRegistry(Registry&& registry);

  const AppSet FilterApps(Filter filter) const;

  void CountMutation();

 private:
  Profile* const profile_;

  base::ObserverList<AppRegistrarObserver, /*check_empty=*/true> observers_;
  OsIntegrationManager* os_integration_manager_ = nullptr;

  Registry registry_;
  bool registry_profile_being_deleted_ = false;
#if DCHECK_IS_ON()
  size_t mutations_count_ = 0;
#endif
};

// A writable API for the registry model. Mutable WebAppRegistrar must be used
// only by WebAppSyncBridge.
class WebAppRegistrarMutable : public WebAppRegistrar {
 public:
  explicit WebAppRegistrarMutable(Profile* profile);
  ~WebAppRegistrarMutable() override;

  void InitRegistry(Registry&& registry);

  WebApp* GetAppByIdMutable(const AppId& app_id);

  AppSet FilterAppsMutable(Filter filter);

  AppSet GetAppsIncludingStubsMutable();
  AppSet GetAppsMutable();

  using WebAppRegistrar::CountMutation;
  using WebAppRegistrar::registry;
};

// For testing and debug purposes.
bool IsRegistryEqual(const Registry& registry, const Registry& registry2);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_APP_REGISTRAR_H_
