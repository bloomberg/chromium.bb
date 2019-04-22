// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/extensions/bookmark_app_uninstaller.h"

#include "base/optional.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/uninstall_reason.h"

namespace extensions {

BookmarkAppUninstaller::BookmarkAppUninstaller(Profile* profile,
                                               web_app::AppRegistrar* registrar)
    : profile_(profile),
      registrar_(registrar),
      extension_ids_map_(profile->GetPrefs()) {}

BookmarkAppUninstaller::~BookmarkAppUninstaller() = default;

bool BookmarkAppUninstaller::UninstallApp(const GURL& app_url) {
  base::Optional<std::string> extension_id =
      extension_ids_map_.LookupExtensionId(app_url);
  if (!extension_id.has_value()) {
    LOG(WARNING) << "Couldn't uninstall app with url " << app_url
                 << "; No corresponding extension for url.";
    return false;
  }

  if (!registrar_->IsInstalled(extension_id.value())) {
    LOG(WARNING) << "Couldn't uninstall app with url " << app_url
                 << "; Extension not installed.";
    return false;
  }

  base::string16 error;
  bool uninstalled =
      ExtensionSystem::Get(profile_)->extension_service()->UninstallExtension(
          extension_id.value(), UNINSTALL_REASON_ORPHANED_EXTERNAL_EXTENSION,
          &error);

  if (!uninstalled) {
    LOG(WARNING) << "Couldn't uninstall app with url " << app_url << ". "
                 << error;
  }
  return uninstalled;
}

}  // namespace extensions
