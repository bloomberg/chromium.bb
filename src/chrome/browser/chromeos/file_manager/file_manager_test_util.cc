// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_manager/file_manager_test_util.h"

#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/extensions/component_loader.h"
#include "chrome/browser/extensions/extension_service.h"
#include "extensions/browser/extension_system.h"

namespace file_manager {
namespace test {

void AddDefaultComponentExtensionsOnMainThread(Profile* profile) {
  CHECK(profile);

  extensions::ComponentLoader::EnableBackgroundExtensionsForTesting();
  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(profile)->extension_service();
  service->component_loader()->AddDefaultComponentExtensions(false);

  // The File Manager component extension should have been added for loading
  // into the user profile, but not into the sign-in profile.
  CHECK(extensions::ExtensionSystem::Get(profile)
            ->extension_service()
            ->component_loader()
            ->Exists(kFileManagerAppId));
  CHECK(!extensions::ExtensionSystem::Get(
             chromeos::ProfileHelper::GetSigninProfile())
             ->extension_service()
             ->component_loader()
             ->Exists(kFileManagerAppId));
}

}  // namespace test
}  // namespace file_manager
