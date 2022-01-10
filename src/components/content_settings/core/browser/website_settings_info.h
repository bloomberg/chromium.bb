// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_WEBSITE_SETTINGS_INFO_H_
#define COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_WEBSITE_SETTINGS_INFO_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "components/content_settings/core/common/content_settings_types.h"

namespace base {
class Value;
}  // namespace base

namespace content_settings {

// This class stores the properties related to a website setting.
class WebsiteSettingsInfo {
 public:
  enum SyncStatus {
    // The setting will be synced.
    SYNCABLE,
    // The setting is stored locally.
    UNSYNCABLE
  };

  enum LossyStatus {
    // This marks the setting as "lossy". There is no strict time guarantee on
    // when a lossy setting will be persisted to permanent storage when it is
    // modified.
    LOSSY,
    // Changes to the setting will be persisted immediately.
    NOT_LOSSY
  };

  enum ScopingType {
    // Settings scoped to the domain of the requesting frame by default.
    // Embedded settings can be stored.
    COOKIES_SCOPE,

    // Storage access specific scoped that is scoped to the pair of requesting
    // and embedding origin.
    STORAGE_ACCESS_SCOPE,

    // Settings scoped to a single origin (generally either the requesting
    // origin or the top level origin of a frame) for a request. Embedded
    // exceptions are not allowed.
    SINGLE_ORIGIN_ONLY_SCOPE,

    // Settings scoped to a single origin (generally either the requesting
    // origin or the top level origin of a frame) for a request. Embedded
    // exceptions are allowed. This should only be used after careful thought.
    // Allowing embedded exceptions requires much more complicated UI.
    SINGLE_ORIGIN_WITH_EMBEDDED_EXCEPTIONS_SCOPE,
  };

  enum IncognitoBehavior {
    // Settings will be inherited from regular to incognito profiles as usual.
    INHERIT_IN_INCOGNITO,

    // Settings will not be inherited from regular to incognito profiles.
    DONT_INHERIT_IN_INCOGNITO,
  };

  WebsiteSettingsInfo(ContentSettingsType type,
                      const std::string& name,
                      std::unique_ptr<base::Value> initial_default_value,
                      SyncStatus sync_status,
                      LossyStatus lossy_status,
                      ScopingType scoping_type,
                      IncognitoBehavior incognito_behavior);

  WebsiteSettingsInfo(const WebsiteSettingsInfo&) = delete;
  WebsiteSettingsInfo& operator=(const WebsiteSettingsInfo&) = delete;

  ~WebsiteSettingsInfo();

  ContentSettingsType type() const { return type_; }
  const std::string& name() const { return name_; }

  const std::string& pref_name() const { return pref_name_; }
  const std::string& default_value_pref_name() const {
    return default_value_pref_name_;
  }
  const base::Value* initial_default_value() const {
    return initial_default_value_.get();
  }

  uint32_t GetPrefRegistrationFlags() const;

  bool SupportsSecondaryPattern() const;

  ScopingType scoping_type() const { return scoping_type_; }
  IncognitoBehavior incognito_behavior() const { return incognito_behavior_; }

 private:
  const ContentSettingsType type_;
  const std::string name_;

  const std::string pref_name_;
  const std::string default_value_pref_name_;
  const std::unique_ptr<base::Value> initial_default_value_;
  const SyncStatus sync_status_;
  const LossyStatus lossy_status_;
  const ScopingType scoping_type_;
  const IncognitoBehavior incognito_behavior_;
};

}  // namespace content_settings

#endif  // COMPONENTS_CONTENT_SETTINGS_CORE_BROWSER_WEBSITE_SETTINGS_INFO_H_
