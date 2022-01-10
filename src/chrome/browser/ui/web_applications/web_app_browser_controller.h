// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_BROWSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_BROWSER_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/web_applications/app_registrar_observer.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/services/app_service/public/cpp/icon_types.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/models/image_model.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "components/digital_asset_links/digital_asset_links_handler.h"  // nogncheck
#endif

class Browser;
class SkBitmap;

namespace digital_asset_links {
class DigitalAssetLinksHandler;
}

namespace web_app {

class SystemWebAppDelegate;
class WebAppRegistrar;
class WebAppProvider;

// Class to encapsulate logic to control the browser UI for
// web apps.
// App information is obtained from the WebAppRegistrar.
// Icon information is obtained from the WebAppIconManager.
// Note: Much of the functionality in HostedAppBrowserController
// will move to this class.
class WebAppBrowserController : public AppBrowserController,
                                public AppRegistrarObserver {
 public:
  WebAppBrowserController(WebAppProvider& provider,
                          Browser* browser,
                          AppId app_id,
                          const SystemWebAppDelegate* system_app,
                          bool has_tab_strip);
  WebAppBrowserController(const WebAppBrowserController&) = delete;
  WebAppBrowserController& operator=(const WebAppBrowserController&) = delete;
  ~WebAppBrowserController() override;

  // AppBrowserController:
  bool HasMinimalUiButtons() const override;
  ui::ImageModel GetWindowAppIcon() const override;
  ui::ImageModel GetWindowIcon() const override;
  absl::optional<SkColor> GetThemeColor() const override;
  absl::optional<SkColor> GetBackgroundColor() const override;
  std::u16string GetTitle() const override;
  std::u16string GetAppShortName() const override;
  std::u16string GetFormattedUrlOrigin() const override;
  GURL GetAppStartUrl() const override;
  bool IsUrlInAppScope(const GURL& url) const override;
  WebAppBrowserController* AsWebAppBrowserController() override;
  bool CanUserUninstall() const override;
  void Uninstall(
      webapps::WebappUninstallSource webapp_uninstall_source) override;
  bool IsInstalled() const override;
  bool IsHostedApp() const override;
  std::unique_ptr<TabMenuModelFactory> GetTabMenuModelFactory() const override;
  bool AppUsesWindowControlsOverlay() const override;
  bool IsWindowControlsOverlayEnabled() const override;
  void ToggleWindowControlsOverlayEnabled() override;
  gfx::Rect GetDefaultBounds() const override;
  bool HasReloadButton() const override;
  const SystemWebAppDelegate* system_app() const override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool ShouldShowCustomTabBar() const override;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // AppRegistrarObserver:
  void OnWebAppUninstalled(const AppId& app_id) override;
  void OnAppRegistrarDestroyed() override;

  void SetReadIconCallbackForTesting(base::OnceClosure callback);

 protected:
  // AppBrowserController:
  void OnTabInserted(content::WebContents* contents) override;
  void OnTabRemoved(content::WebContents* contents) override;

 private:
  const WebAppRegistrar& registrar() const;

  // Helper function to call AppServiceProxy to load icon.
  void LoadAppIcon(bool allow_placeholder_icon) const;
  // Invoked when the icon is loaded.
  void OnLoadIcon(apps::IconValuePtr icon_value);

  void OnReadIcon(SkBitmap bitmap);
  void PerformDigitalAssetLinkVerification(Browser* browser);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void OnRelationshipCheckComplete(
      digital_asset_links::RelationshipCheckResult result);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  WebAppProvider& provider_;
  raw_ptr<const SystemWebAppDelegate> system_app_;
  mutable absl::optional<ui::ImageModel> app_icon_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // The result of digital asset link verification of the web app.
  // Only used for web-only TWAs installed through the Play Store.
  absl::optional<bool> is_verified_;

  std::unique_ptr<digital_asset_links::DigitalAssetLinksHandler>
      asset_link_handler_;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  base::ScopedObservation<WebAppRegistrar, AppRegistrarObserver>
      registrar_observation_{this};

  base::OnceClosure callback_for_testing_;
  mutable base::WeakPtrFactory<WebAppBrowserController> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_WEB_APP_BROWSER_CONTROLLER_H_
