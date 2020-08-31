// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_PUBLIC_PROFILE_H_
#define WEBLAYER_PUBLIC_PROFILE_H_

#include <algorithm>
#include <string>

namespace base {
class FilePath;
}

namespace weblayer {
class CookieManager;
class DownloadDelegate;

// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.weblayer_private
// GENERATED_JAVA_CLASS_NAME_OVERRIDE: ImplBrowsingDataType
enum class BrowsingDataType {
  COOKIES_AND_SITE_DATA = 0,
  CACHE = 1,
};

enum class SettingType {
  BASIC_SAFE_BROWSING_ENABLED = 0,
};

class Profile {
 public:
  // Pass an empty |name| for an in-memory profile.
  // Otherwise, |name| should contain only alphanumeric characters and
  // underscore.
  static std::unique_ptr<Profile> Create(const std::string& name);

  // Delete all profile's data from disk. If there are any existing usage
  // of this profile, return |profile| immediately and |done_callback| will not
  // be called. Otherwise return nullptr and |done_callback| is called when
  // deletion is complete.
  static std::unique_ptr<Profile> DestroyAndDeleteDataFromDisk(
      std::unique_ptr<Profile> profile,
      base::OnceClosure done_callback);

  virtual ~Profile() {}

  virtual void ClearBrowsingData(
      const std::vector<BrowsingDataType>& data_types,
      base::Time from_time,
      base::Time to_time,
      base::OnceClosure callback) = 0;

  // Allows embedders to override the default download directory, which is the
  // system download directory on Android and on other platforms it's in the
  // home directory.
  virtual void SetDownloadDirectory(const base::FilePath& directory) = 0;

  // Sets the DownloadDelegate. If none is set, downloads will be dropped.
  virtual void SetDownloadDelegate(DownloadDelegate* delegate) = 0;

  // Gets the cookie manager for this profile.
  virtual CookieManager* GetCookieManager() = 0;

  // Set the boolean value of the given setting type.
  virtual void SetBooleanSetting(SettingType type, bool value) = 0;

  // Get the boolean value of the given setting type.
  virtual bool GetBooleanSetting(SettingType type) = 0;
};

}  // namespace weblayer

#endif  // WEBLAYER_PUBLIC_PROFILE_H_
