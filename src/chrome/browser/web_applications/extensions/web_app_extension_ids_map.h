// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_WEB_APP_EXTENSION_IDS_MAP_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_WEB_APP_EXTENSION_IDS_MAP_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"

class GURL;
class PrefService;
class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace web_app {

// A Prefs-backed map from web app URLs to Chrome extension IDs.
//
// This lets us determine, given a web app's URL, whether that web app is
// already installed.
class ExtensionIdsMap {
 public:
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  static bool HasExtensionId(const PrefService* pref_service,
                             const std::string& extension_id);

  // Returns the URLs of the apps that were installed as policy-installed apps.
  static std::vector<GURL> GetPolicyInstalledAppUrls(Profile* profile);

  explicit ExtensionIdsMap(PrefService* pref_service);

  void Insert(const GURL& url, const std::string& extension_id);
  base::Optional<std::string> Lookup(const GURL& url);

 private:
  PrefService* pref_service_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionIdsMap);
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_WEB_APP_EXTENSION_IDS_MAP_H_
