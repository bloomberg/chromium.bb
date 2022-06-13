// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_shortcut.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/task/task_runner.h"

namespace web_app {

namespace internals {

// On Chrome OS, we do not have platform shortcuts, so these operation are
// no-ops. We instead integrate with the Launcher and the Shelf through the App
// Service.

bool CreatePlatformShortcuts(const base::FilePath& web_app_path,
                             const ShortcutLocations& creation_locations,
                             ShortcutCreationReason creation_reason,
                             const ShortcutInfo& shortcut_info) {
  return true;
}

void DeletePlatformShortcuts(const base::FilePath& web_app_path,
                             const ShortcutInfo& shortcut_info,
                             scoped_refptr<base::TaskRunner> result_runner,
                             web_app::DeleteShortcutsCallback callback) {
  result_runner->PostTask(FROM_HERE, base::BindOnce(std::move(callback),
                                                    /*shortcut_deleted=*/true));
}

void UpdatePlatformShortcuts(const base::FilePath& web_app_path,
                             const std::u16string& old_app_title,
                             const ShortcutInfo& shortcut_info) {}

ShortcutLocations GetAppExistingShortCutLocationImpl(
    const ShortcutInfo& shortcut_info) {
  return ShortcutLocations();
}

void DeleteAllShortcutsForProfile(const base::FilePath& profile_path) {}

}  // namespace internals

}  // namespace web_app
