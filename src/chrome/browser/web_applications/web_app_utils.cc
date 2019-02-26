// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_app_utils.h"

#include "base/files/file_path.h"
#include "chrome/browser/profiles/profile.h"

namespace web_app {

constexpr base::FilePath::CharType kWebAppsDirectoryName[] =
    FILE_PATH_LITERAL("WebApps");

bool AllowWebAppInstallation(Profile* profile) {
  return !profile->IsGuestSession() && !profile->IsOffTheRecord() &&
         !profile->IsSystemProfile();
}

base::FilePath GetWebAppsDirectory(Profile* profile) {
  return profile->GetPath().Append(base::FilePath(kWebAppsDirectoryName));
}

}  // namespace web_app
