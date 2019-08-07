// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/website_settings_info.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry.h"

namespace {

const char kPrefPrefix[] = "profile.content_settings.exceptions.";
const char kDefaultPrefPrefix[] = "profile.default_content_setting_values.";

std::string GetPreferenceName(const std::string& name, const char* prefix) {
  std::string pref_name = name;
  base::ReplaceChars(pref_name, "-", "_", &pref_name);
  return std::string(prefix).append(pref_name);
}

}  // namespace

namespace content_settings {

WebsiteSettingsInfo::WebsiteSettingsInfo(
    ContentSettingsType type,
    const std::string& name,
    std::unique_ptr<base::Value> initial_default_value,
    SyncStatus sync_status,
    LossyStatus lossy_status,
    ScopingType scoping_type,
    IncognitoBehavior incognito_behavior)
    : type_(type),
      name_(name),
      pref_name_(GetPreferenceName(name, kPrefPrefix)),
      default_value_pref_name_(GetPreferenceName(name, kDefaultPrefPrefix)),
      initial_default_value_(std::move(initial_default_value)),
      sync_status_(sync_status),
      lossy_status_(lossy_status),
      scoping_type_(scoping_type),
      incognito_behavior_(incognito_behavior) {
  // For legacy reasons the default value is currently restricted to be an int.
  // TODO(raymes): We should migrate the underlying pref to be a dictionary
  // rather than an int.
  DCHECK(!initial_default_value_ || initial_default_value_->is_int());
}

WebsiteSettingsInfo::~WebsiteSettingsInfo() {}

uint32_t WebsiteSettingsInfo::GetPrefRegistrationFlags() const {
  uint32_t flags = PrefRegistry::NO_REGISTRATION_FLAGS;

  if (sync_status_ == SYNCABLE)
    flags |= user_prefs::PrefRegistrySyncable::SYNCABLE_PREF;

  if (lossy_status_ == LOSSY)
    flags |= PrefRegistry::LOSSY_PREF;

  return flags;
}

bool WebsiteSettingsInfo::SupportsEmbeddedExceptions() const {
  // Note that REQUESTING_ORIGIN_AND_TOP_LEVEL_ORIGIN_SCOPE supports embedded
  // exceptions but because these are deprecated and planned to be removed they
  // aren't included here.
  if (scoping_type_ == COOKIES_SCOPE ||
      scoping_type_ == SINGLE_ORIGIN_WITH_EMBEDDED_EXCEPTIONS_SCOPE) {
    return true;
  }

  return false;
}

}  // namespace content_settings
