// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/browser/content_settings_default_provider.h"

#include <memory>
#include <string>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/content_settings/core/browser/content_settings_info.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/browser/content_settings_rule.h"
#include "components/content_settings/core/browser/content_settings_utils.h"
#include "components/content_settings/core/browser/website_settings_info.h"
#include "components/content_settings/core/browser/website_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_pattern.h"
#include "components/content_settings/core/common/content_settings_utils.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "url/gurl.h"

namespace content_settings {

namespace {

// These settings are no longer used, and should be deleted on profile startup.
#if !defined(OS_IOS)
const char kObsoleteFullscreenDefaultPref[] =
    "profile.default_content_setting_values.fullscreen";
#if !defined(OS_ANDROID)
const char kObsoleteMouseLockDefaultPref[] =
    "profile.default_content_setting_values.mouselock";
const char kObsoletePluginsDefaultPref[] =
    "profile.default_content_setting_values.plugins";
const char kObsoletePluginsDataDefaultPref[] =
    "profile.default_content_setting_values.flash_data";
#endif  // !defined(OS_ANDROID)
#endif  // !defined(OS_IOS)

#if BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_WIN)
// This setting was moved and should be migrated on profile startup.
const char kDeprecatedEnableDRM[] = "settings.privacy.drm_enabled";
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_WIN)

ContentSetting GetDefaultValue(const WebsiteSettingsInfo* info) {
  const base::Value* initial_default = info->initial_default_value();
  if (!initial_default)
    return CONTENT_SETTING_DEFAULT;
  return static_cast<ContentSetting>(initial_default->GetInt());
}

ContentSetting GetDefaultValue(ContentSettingsType type) {
  return GetDefaultValue(WebsiteSettingsRegistry::GetInstance()->Get(type));
}

const std::string& GetPrefName(ContentSettingsType type) {
  return WebsiteSettingsRegistry::GetInstance()
      ->Get(type)
      ->default_value_pref_name();
}

class DefaultRuleIterator : public RuleIterator {
 public:
  explicit DefaultRuleIterator(const base::Value* value) {
    if (value)
      value_ = value->Clone();
    else
      is_done_ = true;
  }

  DefaultRuleIterator(const DefaultRuleIterator&) = delete;
  DefaultRuleIterator& operator=(const DefaultRuleIterator&) = delete;

  bool HasNext() const override { return !is_done_; }

  Rule Next() override {
    DCHECK(HasNext());
    is_done_ = true;
    return Rule(ContentSettingsPattern::Wildcard(),
                ContentSettingsPattern::Wildcard(), std::move(value_),
                base::Time(), SessionModel::Durable);
  }

 private:
  bool is_done_ = false;
  base::Value value_;
};

}  // namespace

// static
void DefaultProvider::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // Register the default settings' preferences.
  WebsiteSettingsRegistry* website_settings =
      WebsiteSettingsRegistry::GetInstance();
  for (const WebsiteSettingsInfo* info : *website_settings) {
    registry->RegisterIntegerPref(info->default_value_pref_name(),
                                  GetDefaultValue(info),
                                  info->GetPrefRegistrationFlags());
  }

  // Obsolete prefs -------------------------------------------------------

  // These prefs have been deprecated, but need to be registered so they can
  // be deleted on startup (see DiscardOrMigrateObsoletePreferences).
#if !defined(OS_IOS)
  registry->RegisterIntegerPref(
      kObsoleteFullscreenDefaultPref, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
#if !defined(OS_ANDROID)
  registry->RegisterIntegerPref(
      kObsoleteMouseLockDefaultPref, 0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterIntegerPref(kObsoletePluginsDataDefaultPref, 0);
  registry->RegisterIntegerPref(kObsoletePluginsDefaultPref, 0);
#endif  // !defined(OS_ANDROID)
#endif  // !defined(OS_IOS)

#if BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_WIN)
  registry->RegisterBooleanPref(kDeprecatedEnableDRM, true);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_WIN)
}

DefaultProvider::DefaultProvider(PrefService* prefs, bool off_the_record)
    : prefs_(prefs),
      is_off_the_record_(off_the_record),
      updating_preferences_(false) {
  DCHECK(prefs_);

  // Remove the obsolete preferences from the pref file.
  DiscardOrMigrateObsoletePreferences();

  // Read global defaults.
  ReadDefaultSettings();

  UMA_HISTOGRAM_ENUMERATION("ContentSettings.DefaultCookiesSetting",
                            IntToContentSetting(prefs_->GetInteger(
                                GetPrefName(ContentSettingsType::COOKIES))),
                            CONTENT_SETTING_NUM_SETTINGS);
  UMA_HISTOGRAM_ENUMERATION("ContentSettings.DefaultPopupsSetting",
                            IntToContentSetting(prefs_->GetInteger(
                                GetPrefName(ContentSettingsType::POPUPS))),
                            CONTENT_SETTING_NUM_SETTINGS);
#if !defined(OS_IOS) && !defined(OS_ANDROID)
  UMA_HISTOGRAM_ENUMERATION("ContentSettings.DefaultImagesSetting",
                            IntToContentSetting(prefs_->GetInteger(
                                GetPrefName(ContentSettingsType::IMAGES))),
                            CONTENT_SETTING_NUM_SETTINGS);
#endif

#if !defined(OS_IOS)
  UMA_HISTOGRAM_ENUMERATION("ContentSettings.DefaultJavaScriptSetting",
                            IntToContentSetting(prefs_->GetInteger(
                                GetPrefName(ContentSettingsType::JAVASCRIPT))),
                            CONTENT_SETTING_NUM_SETTINGS);

  UMA_HISTOGRAM_ENUMERATION("ContentSettings.DefaultLocationSetting",
                            IntToContentSetting(prefs_->GetInteger(
                                GetPrefName(ContentSettingsType::GEOLOCATION))),
                            CONTENT_SETTING_NUM_SETTINGS);
  UMA_HISTOGRAM_ENUMERATION("ContentSettings.DefaultNotificationsSetting",
                            IntToContentSetting(prefs_->GetInteger(GetPrefName(
                                ContentSettingsType::NOTIFICATIONS))),
                            CONTENT_SETTING_NUM_SETTINGS);

  UMA_HISTOGRAM_ENUMERATION("ContentSettings.DefaultMediaStreamMicSetting",
                            IntToContentSetting(prefs_->GetInteger(GetPrefName(
                                ContentSettingsType::MEDIASTREAM_MIC))),
                            CONTENT_SETTING_NUM_SETTINGS);
  UMA_HISTOGRAM_ENUMERATION("ContentSettings.DefaultMediaStreamCameraSetting",
                            IntToContentSetting(prefs_->GetInteger(GetPrefName(
                                ContentSettingsType::MEDIASTREAM_CAMERA))),
                            CONTENT_SETTING_NUM_SETTINGS);
  UMA_HISTOGRAM_ENUMERATION("ContentSettings.DefaultMIDISysExSetting",
                            IntToContentSetting(prefs_->GetInteger(
                                GetPrefName(ContentSettingsType::MIDI_SYSEX))),
                            CONTENT_SETTING_NUM_SETTINGS);
  UMA_HISTOGRAM_ENUMERATION("ContentSettings.DefaultWebBluetoothGuardSetting",
                            IntToContentSetting(prefs_->GetInteger(GetPrefName(
                                ContentSettingsType::BLUETOOTH_GUARD))),
                            CONTENT_SETTING_NUM_SETTINGS);
  UMA_HISTOGRAM_ENUMERATION("ContentSettings.DefaultAutoplaySetting",
                            IntToContentSetting(prefs_->GetInteger(
                                GetPrefName(ContentSettingsType::AUTOPLAY))),
                            CONTENT_SETTING_NUM_SETTINGS);
  UMA_HISTOGRAM_ENUMERATION("ContentSettings.DefaultSubresourceFilterSetting",
                            IntToContentSetting(prefs_->GetInteger(
                                GetPrefName(ContentSettingsType::ADS))),
                            CONTENT_SETTING_NUM_SETTINGS);
  UMA_HISTOGRAM_ENUMERATION("ContentSettings.DefaultSoundSetting",
                            IntToContentSetting(prefs_->GetInteger(
                                GetPrefName(ContentSettingsType::SOUND))),
                            CONTENT_SETTING_NUM_SETTINGS);
  UMA_HISTOGRAM_ENUMERATION("ContentSettings.DefaultUsbGuardSetting",
                            IntToContentSetting(prefs_->GetInteger(
                                GetPrefName(ContentSettingsType::USB_GUARD))),
                            CONTENT_SETTING_NUM_SETTINGS);
  UMA_HISTOGRAM_ENUMERATION("ContentSettings.DefaultIdleDetectionSetting",
                            IntToContentSetting(prefs_->GetInteger(GetPrefName(
                                ContentSettingsType::IDLE_DETECTION))),
                            CONTENT_SETTING_NUM_SETTINGS);
#endif

#if defined(OS_ANDROID)
  UMA_HISTOGRAM_ENUMERATION("ContentSettings.DefaultAutoDarkWebContentSetting",
                            IntToContentSetting(prefs_->GetInteger(GetPrefName(
                                ContentSettingsType::AUTO_DARK_WEB_CONTENT))),
                            CONTENT_SETTING_NUM_SETTINGS);

  UMA_HISTOGRAM_ENUMERATION("ContentSettings.DefaultRequestDesktopSiteSetting",
                            IntToContentSetting(prefs_->GetInteger(GetPrefName(
                                ContentSettingsType::REQUEST_DESKTOP_SITE))),
                            CONTENT_SETTING_NUM_SETTINGS);
#endif

  pref_change_registrar_.Init(prefs_);
  PrefChangeRegistrar::NamedChangeCallback callback = base::BindRepeating(
      &DefaultProvider::OnPreferenceChanged, base::Unretained(this));
  WebsiteSettingsRegistry* website_settings =
      WebsiteSettingsRegistry::GetInstance();
  for (const WebsiteSettingsInfo* info : *website_settings)
    pref_change_registrar_.Add(info->default_value_pref_name(), callback);
}

DefaultProvider::~DefaultProvider() {
}

bool DefaultProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    std::unique_ptr<base::Value>&& in_value,
    const ContentSettingConstraints& constraints) {
  DCHECK(CalledOnValidThread());
  DCHECK(prefs_);

  // Ignore non default settings
  if (primary_pattern != ContentSettingsPattern::Wildcard() ||
      secondary_pattern != ContentSettingsPattern::Wildcard()) {
    return false;
  }

  // Put |in_value| in a scoped pointer to ensure that it gets cleaned up
  // properly if we don't pass on the ownership.
  std::unique_ptr<base::Value> value(std::move(in_value));

  // The default settings may not be directly modified for OTR sessions.
  // Instead, they are synced to the main profile's setting.
  if (is_off_the_record_)
    return true;

  {
    base::AutoReset<bool> auto_reset(&updating_preferences_, true);
    // Lock the memory map access, so that values are not read by
    // |GetRuleIterator| at the same time as they are written here. Do not lock
    // the preference access though; preference updates send out notifications
    // whose callbacks may try to reacquire the lock on the same thread.
    {
      base::AutoLock lock(lock_);
      ChangeSetting(content_type, value.get());
    }
    WriteToPref(content_type, value.get());
  }

  NotifyObservers(ContentSettingsPattern::Wildcard(),
                  ContentSettingsPattern::Wildcard(), content_type);

  return true;
}

std::unique_ptr<RuleIterator> DefaultProvider::GetRuleIterator(
    ContentSettingsType content_type,
    bool off_the_record) const {
  // The default provider never has off-the-record-specific settings.
  if (off_the_record)
    return nullptr;

  base::AutoLock lock(lock_);
  auto it = default_settings_.find(content_type);
  if (it == default_settings_.end()) {
    NOTREACHED();
    return nullptr;
  }
  return std::make_unique<DefaultRuleIterator>(it->second.get());
}

void DefaultProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {
  // TODO(markusheintz): This method is only called when the
  // |DesktopNotificationService| calls |ClearAllSettingsForType| method on the
  // |HostContentSettingsMap|. Don't implement this method yet, otherwise the
  // default notification settings will be cleared as well.
}

void DefaultProvider::ShutdownOnUIThread() {
  DCHECK(CalledOnValidThread());
  DCHECK(prefs_);
  RemoveAllObservers();
  pref_change_registrar_.RemoveAll();
  prefs_ = nullptr;
}

void DefaultProvider::ReadDefaultSettings() {
  base::AutoLock lock(lock_);
  WebsiteSettingsRegistry* website_settings =
      WebsiteSettingsRegistry::GetInstance();
  for (const WebsiteSettingsInfo* info : *website_settings)
    ChangeSetting(info->type(), ReadFromPref(info->type()).get());
}

bool DefaultProvider::IsValueEmptyOrDefault(ContentSettingsType content_type,
                                            base::Value* value) {
  return !value ||
         ValueToContentSetting(value) == GetDefaultValue(content_type);
}

void DefaultProvider::ChangeSetting(ContentSettingsType content_type,
                                    base::Value* value) {
  const ContentSettingsInfo* info =
      ContentSettingsRegistry::GetInstance()->Get(content_type);
  DCHECK(!info || !value ||
         info->IsDefaultSettingValid(ValueToContentSetting(value)));
  default_settings_[content_type] =
      value ? base::Value::ToUniquePtrValue(value->Clone())
            : ContentSettingToValue(GetDefaultValue(content_type));
}

void DefaultProvider::WriteToPref(ContentSettingsType content_type,
                                  base::Value* value) {
  if (IsValueEmptyOrDefault(content_type, value)) {
    prefs_->ClearPref(GetPrefName(content_type));
    return;
  }

  prefs_->SetInteger(GetPrefName(content_type), value->GetInt());
}

void DefaultProvider::OnPreferenceChanged(const std::string& name) {
  DCHECK(CalledOnValidThread());
  if (updating_preferences_)
    return;

  // Find out which content setting the preference corresponds to.
  ContentSettingsType content_type = ContentSettingsType::DEFAULT;

  WebsiteSettingsRegistry* website_settings =
      WebsiteSettingsRegistry::GetInstance();
  for (const WebsiteSettingsInfo* info : *website_settings) {
    if (info->default_value_pref_name() == name) {
      content_type = info->type();
      break;
    }
  }

  if (content_type == ContentSettingsType::DEFAULT) {
    NOTREACHED() << "A change of the preference " << name << " was observed, "
                    "but the preference could not be mapped to a content "
                    "settings type.";
    return;
  }

  {
    base::AutoReset<bool> auto_reset(&updating_preferences_, true);
    // Lock the memory map access, so that values are not read by
    // |GetRuleIterator| at the same time as they are written here. Do not lock
    // the preference access though; preference updates send out notifications
    // whose callbacks may try to reacquire the lock on the same thread.
    {
      base::AutoLock lock(lock_);
      ChangeSetting(content_type, ReadFromPref(content_type).get());
    }
  }

  NotifyObservers(ContentSettingsPattern::Wildcard(),
                  ContentSettingsPattern::Wildcard(), content_type);
}

std::unique_ptr<base::Value> DefaultProvider::ReadFromPref(
    ContentSettingsType content_type) {
  int int_value = prefs_->GetInteger(GetPrefName(content_type));
  return ContentSettingToValue(IntToContentSetting(int_value));
}

void DefaultProvider::DiscardOrMigrateObsoletePreferences() {
  if (is_off_the_record_)
    return;
  // These prefs were never stored on iOS/Android so they don't need to be
  // deleted.
#if !defined(OS_IOS)
  prefs_->ClearPref(kObsoleteFullscreenDefaultPref);
#if !defined(OS_ANDROID)
  prefs_->ClearPref(kObsoleteMouseLockDefaultPref);
  prefs_->ClearPref(kObsoletePluginsDefaultPref);
  prefs_->ClearPref(kObsoletePluginsDataDefaultPref);
#endif  // !defined(OS_ANDROID)
#endif  // !defined(OS_IOS)

#if BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_WIN)
  // TODO(crbug.com/1191642): Remove this migration logic in M100.
  WebsiteSettingsRegistry* website_settings =
      WebsiteSettingsRegistry::GetInstance();
  const base::Value* deprecated_enable_drm_value =
      prefs_->GetUserPrefValue(kDeprecatedEnableDRM);
  if (deprecated_enable_drm_value) {
    prefs_->SetInteger(
        website_settings->Get(ContentSettingsType::PROTECTED_MEDIA_IDENTIFIER)
            ->default_value_pref_name(),
        deprecated_enable_drm_value->GetBool() ? CONTENT_SETTING_ALLOW
                                               : CONTENT_SETTING_BLOCK);
  }
  prefs_->ClearPref(kDeprecatedEnableDRM);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH) || defined(OS_WIN)
}

}  // namespace content_settings
