// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_installer.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "chrome/browser/extensions/crx_installer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/manifest_handlers/app_launch_info.h"
#include "chrome/common/web_application_info.h"
#include "extensions/common/extension.h"
#include "url/gurl.h"

namespace extensions {

BookmarkAppInstaller::BookmarkAppInstaller(Profile* profile)
    : crx_installer_(CrxInstaller::CreateSilent(
          ExtensionSystem::Get(profile)->extension_service())) {}

BookmarkAppInstaller::~BookmarkAppInstaller() = default;

void BookmarkAppInstaller::Install(const WebApplicationInfo& web_app_info,
                                   ResultCallback callback) {
  crx_installer_->set_installer_callback(base::BindOnce(
      &BookmarkAppInstaller::OnInstall, weak_ptr_factory_.GetWeakPtr(),
      std::move(callback), web_app_info.app_url));
  crx_installer_->InstallWebApp(web_app_info);
}

void BookmarkAppInstaller::SetCrxInstallerForTesting(
    scoped_refptr<CrxInstaller> crx_installer) {
  crx_installer_ = crx_installer;
}

void BookmarkAppInstaller::OnInstall(
    ResultCallback callback,
    const GURL& app_url,
    const base::Optional<CrxInstallError>& error) {
  if (error) {
    std::move(callback).Run(std::string());
    return;
  }

  auto* installed_extension = crx_installer_->extension();
  DCHECK(installed_extension);
  DCHECK_EQ(AppLaunchInfo::GetLaunchWebURL(installed_extension), app_url);

  std::move(callback).Run(installed_extension->id());
}

}  // namespace extensions
