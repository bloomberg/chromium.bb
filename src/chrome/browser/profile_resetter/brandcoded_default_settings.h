// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILE_RESETTER_BRANDCODED_DEFAULT_SETTINGS_H_
#define CHROME_BROWSER_PROFILE_RESETTER_BRANDCODED_DEFAULT_SETTINGS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/values.h"

// BrandcodedDefaultSettings provides a set of default settings
// for ProfileResetter. They are specific to Chrome distribution channels.
class BrandcodedDefaultSettings {
 public:
  BrandcodedDefaultSettings();
  // Constructs BrandcodedDefaultSettings directly from preferences.
  explicit BrandcodedDefaultSettings(const std::string& prefs);
  ~BrandcodedDefaultSettings();

  // The following methods return non-zero value if the default value was
  // provided for given setting.
  // After the call return_value contains a list of default engines.
  // |return_value[0]| is default one.
  std::unique_ptr<base::ListValue> GetSearchProviderOverrides() const;

  bool GetHomepage(std::string* homepage) const;
  bool GetHomepageIsNewTab(bool* homepage_is_ntp) const;
  bool GetShowHomeButton(bool* show_home_button) const;

  // |extension_ids| is a list of extension ids.
  bool GetExtensions(std::vector<std::string>* extension_ids) const;

  bool GetRestoreOnStartup(int* restore_on_startup) const;
  std::unique_ptr<base::ListValue> GetUrlsToRestoreOnStartup() const;

 private:
  std::unique_ptr<base::ListValue> ExtractList(const char* pref_name) const;

  std::unique_ptr<base::DictionaryValue> master_dictionary_;

  DISALLOW_COPY_AND_ASSIGN(BrandcodedDefaultSettings);
};

#endif  // CHROME_BROWSER_PROFILE_RESETTER_BRANDCODED_DEFAULT_SETTINGS_H_
