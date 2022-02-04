// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/extension_apps_util.h"

namespace apps {

extensions::UninstallReason GetExtensionUninstallReason(
    apps::mojom::UninstallSource uninstall_source) {
  switch (uninstall_source) {
    case apps::mojom::UninstallSource::kUnknown:
      // We assume if the reason is unknown that it's user inititated.
      return extensions::UNINSTALL_REASON_USER_INITIATED;
    case apps::mojom::UninstallSource::kAppList:
    case apps::mojom::UninstallSource::kAppManagement:
    case apps::mojom::UninstallSource::kShelf:
      return extensions::UNINSTALL_REASON_USER_INITIATED;
    case apps::mojom::UninstallSource::kMigration:
      return extensions::UNINSTALL_REASON_MIGRATED;
  }
}

}  // namespace apps
