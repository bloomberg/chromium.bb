// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_INSTALL_FINALIZER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_INSTALL_FINALIZER_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/web_applications/components/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/components/install_finalizer.h"

class Profile;

namespace extensions {

class CrxInstaller;

// Class used to actually install the Bookmark App in the system.
// TODO(loyso): Erase this subclass once crbug.com/877898 fixed.
class BookmarkAppInstallFinalizer : public web_app::InstallFinalizer {
 public:
  // Constructs a BookmarkAppInstallFinalizer that will install the Bookmark App
  // in |profile|.
  explicit BookmarkAppInstallFinalizer(Profile* profile);
  ~BookmarkAppInstallFinalizer() override;

  // InstallFinalizer:
  void FinalizeInstall(const WebApplicationInfo& web_app_info,
                       const FinalizeOptions& options,
                       InstallFinalizedCallback callback) override;
  void UninstallExternalWebApp(
      const GURL& app_url,
      UninstallExternalWebAppCallback callback) override;
  bool CanCreateOsShortcuts() const override;
  void CreateOsShortcuts(const web_app::AppId& app_id,
                         bool add_to_desktop,
                         CreateOsShortcutsCallback callback) override;
  bool CanPinAppToShelf() const override;
  void PinAppToShelf(const web_app::AppId& app_id) override;
  bool CanReparentTab(const web_app::AppId& app_id,
                      bool shortcut_created) const override;
  void ReparentTab(const web_app::AppId& app_id,
                   content::WebContents* web_contents) override;
  bool CanRevealAppShim() const override;
  void RevealAppShim(const web_app::AppId& app_id) override;
  bool CanSkipAppUpdateForSync(
      const web_app::AppId& app_id,
      const WebApplicationInfo& web_app_info) const override;

  using CrxInstallerFactory =
      base::RepeatingCallback<scoped_refptr<CrxInstaller>(Profile*)>;
  void SetCrxInstallerFactoryForTesting(
      CrxInstallerFactory crx_installer_factory);

 private:
  CrxInstallerFactory crx_installer_factory_;
  Profile* profile_;
  web_app::ExternallyInstalledWebAppPrefs externally_installed_app_prefs_;

  DISALLOW_COPY_AND_ASSIGN(BookmarkAppInstallFinalizer);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_BOOKMARK_APP_INSTALL_FINALIZER_H_
