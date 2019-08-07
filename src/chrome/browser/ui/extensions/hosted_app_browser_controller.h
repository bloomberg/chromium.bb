// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_HOSTED_APP_BROWSER_CONTROLLER_H_
#define CHROME_BROWSER_UI_EXTENSIONS_HOSTED_APP_BROWSER_CONTROLLER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "chrome/browser/extensions/extension_uninstall_dialog.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/web_app_browser_controller.h"
#include "third_party/skia/include/core/SkColor.h"

class Browser;

namespace gfx {
class ImageSkia;
}

namespace extensions {

// Returns true if |page_url| is in the scope of the app for |app_url|. If the
// app has no scope defined (as in a bookmark app), we fall back to checking
// |page_url| has the same origin as |app_url|.
bool IsSameScope(const GURL& app_url,
                 const GURL& page_url,
                 content::BrowserContext* profile);

class Extension;

// Class to encapsulate logic to control the browser UI for extension based web
// apps.
class HostedAppBrowserController : public TabStripModelObserver,
                                   public ExtensionUninstallDialog::Delegate,
                                   public WebAppBrowserController {
 public:
  // Functions to set preferences that are unique to app windows.
  static void SetAppPrefsForWebContents(WebAppBrowserController* controller,
                                        content::WebContents* web_contents);

  explicit HostedAppBrowserController(Browser* browser);
  ~HostedAppBrowserController() override;

  base::Optional<std::string> GetAppId() const override;

  // Returns true if the associated Hosted App is for a PWA.
  bool CreatedForInstalledPwa() const override;

  // Returns true if this controller is for a System Web App.
  bool IsForSystemWebApp() const;

  // Whether the browser being controlled should be currently showing the
  // toolbar.
  bool ShouldShowToolbar() const override;

  // Returns true if the hosted app buttons should be shown in the frame for
  // this BrowserView.
  bool ShouldShowHostedAppButtonContainer() const override;

  // Returns the app icon for the window to use in the task list.
  gfx::ImageSkia GetWindowAppIcon() const override;

  // Returns the icon to be displayed in the window title bar.
  gfx::ImageSkia GetWindowIcon() const override;

  // Returns the color of the title bar.
  base::Optional<SkColor> GetThemeColor() const override;

  // Returns the title to be displayed in the window title bar.
  base::string16 GetTitle() const override;

  // Gets the short name of the app.
  std::string GetAppShortName() const override;

  // Gets the origin of the app start url suitable for display (e.g
  // example.com.au).
  base::string16 GetFormattedUrlOrigin() const override;

  // Gets the launch url for the app.
  GURL GetAppLaunchURL() const override;

  // Gets the extension for this controller.
  const Extension* GetExtensionForTesting() const;

  bool CanUninstall() const override;

  void Uninstall(UninstallReason reason, UninstallSource source) override;

  // Returns whether the app is installed (uninstallation may complete within
  // the lifetime of HostedAppBrowserController).
  bool IsInstalled() const override;

  // TabStripModelObserver overrides.
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  bool IsHostedApp() const override;

 private:
  // Called by OnTabstripModelChanged().
  void OnTabInserted(content::WebContents* contents);
  void OnTabRemoved(content::WebContents* contents);

  // Will return nullptr if the extension has been uninstalled.
  const Extension* GetExtension() const;

  const std::string extension_id_;
  const bool created_for_installed_pwa_;
  std::unique_ptr<ExtensionUninstallDialog> uninstall_dialog_;

  DISALLOW_COPY_AND_ASSIGN(HostedAppBrowserController);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_HOSTED_APP_BROWSER_CONTROLLER_H_
