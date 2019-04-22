// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/chooser_context_base.h"

#include <utility>

#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"

const char kObjectListKey[] = "chosen-objects";

ChooserContextBase::ChooserContextBase(
    Profile* profile,
    const ContentSettingsType guard_content_settings_type,
    const ContentSettingsType data_content_settings_type)
    : guard_content_settings_type_(guard_content_settings_type),
      data_content_settings_type_(data_content_settings_type),
      host_content_settings_map_(
          HostContentSettingsMapFactory::GetForProfile(profile)) {
  DCHECK(host_content_settings_map_);
}

ChooserContextBase::~ChooserContextBase() = default;

ChooserContextBase::Object::Object(GURL requesting_origin,
                                   GURL embedding_origin,
                                   base::DictionaryValue* value,
                                   content_settings::SettingSource source,
                                   bool incognito)
    : requesting_origin(requesting_origin),
      embedding_origin(embedding_origin),
      source(source),
      incognito(incognito) {
  this->value.Swap(value);
}

ChooserContextBase::Object::~Object() = default;

void ChooserContextBase::PermissionObserver::OnChooserObjectPermissionChanged(
    ContentSettingsType data_content_settings_type,
    ContentSettingsType guard_content_settings_type) {}

void ChooserContextBase::PermissionObserver::OnPermissionRevoked(
    const GURL& requesting_origin,
    const GURL& embedding_origin) {}

void ChooserContextBase::AddObserver(PermissionObserver* observer) {
  permission_observer_list_.AddObserver(observer);
}

void ChooserContextBase::RemoveObserver(PermissionObserver* observer) {
  permission_observer_list_.RemoveObserver(observer);
}

bool ChooserContextBase::CanRequestObjectPermission(
    const GURL& requesting_origin,
    const GURL& embedding_origin) {
  ContentSetting content_setting =
      host_content_settings_map_->GetContentSetting(
          requesting_origin, embedding_origin, guard_content_settings_type_,
          std::string());
  DCHECK(content_setting == CONTENT_SETTING_ASK ||
         content_setting == CONTENT_SETTING_BLOCK);
  return content_setting == CONTENT_SETTING_ASK;
}

std::vector<std::unique_ptr<ChooserContextBase::Object>>
ChooserContextBase::GetGrantedObjects(const GURL& requesting_origin,
                                      const GURL& embedding_origin) {
  DCHECK_EQ(requesting_origin, requesting_origin.GetOrigin());
  DCHECK_EQ(embedding_origin, embedding_origin.GetOrigin());

  if (!CanRequestObjectPermission(requesting_origin, embedding_origin))
    return {};

  std::vector<std::unique_ptr<Object>> results;
  content_settings::SettingInfo info;
  std::unique_ptr<base::DictionaryValue> setting =
      GetWebsiteSetting(requesting_origin, embedding_origin, &info);
  std::unique_ptr<base::Value> objects;
  if (!setting->Remove(kObjectListKey, &objects))
    return results;

  std::unique_ptr<base::ListValue> object_list =
      base::ListValue::From(std::move(objects));
  if (!object_list)
    return results;

  for (auto& object : *object_list) {
    base::DictionaryValue* object_dict;
    if (object.GetAsDictionary(&object_dict) && IsValidObject(*object_dict)) {
      results.push_back(std::make_unique<Object>(
          requesting_origin, embedding_origin, object_dict, info.source,
          host_content_settings_map_->is_incognito()));
    }
  }
  return results;
}

std::vector<std::unique_ptr<ChooserContextBase::Object>>
ChooserContextBase::GetAllGrantedObjects() {
  ContentSettingsForOneType content_settings;
  host_content_settings_map_->GetSettingsForOneType(
      data_content_settings_type_, std::string(), &content_settings);

  std::vector<std::unique_ptr<Object>> results;
  for (const ContentSettingPatternSource& content_setting : content_settings) {
    GURL requesting_origin(content_setting.primary_pattern.ToString());
    GURL embedding_origin(content_setting.secondary_pattern.ToString());
    if (!requesting_origin.is_valid() || !embedding_origin.is_valid())
      continue;

    if (!CanRequestObjectPermission(requesting_origin, embedding_origin))
      continue;

    content_settings::SettingInfo info;
    std::unique_ptr<base::DictionaryValue> setting =
        GetWebsiteSetting(requesting_origin, embedding_origin, &info);
    base::ListValue* object_list;
    if (!setting->GetList(kObjectListKey, &object_list))
      continue;

    for (auto& object : *object_list) {
      base::DictionaryValue* object_dict;
      if (!object.GetAsDictionary(&object_dict) ||
          !IsValidObject(*object_dict)) {
        continue;
      }

      results.push_back(std::make_unique<Object>(
          requesting_origin, embedding_origin, object_dict, info.source,
          content_setting.incognito));
    }
  }

  return results;
}

void ChooserContextBase::GrantObjectPermission(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    std::unique_ptr<base::DictionaryValue> object) {
  DCHECK_EQ(requesting_origin, requesting_origin.GetOrigin());
  DCHECK_EQ(embedding_origin, embedding_origin.GetOrigin());
  DCHECK(object);
  DCHECK(IsValidObject(*object));

  std::unique_ptr<base::DictionaryValue> setting =
      GetWebsiteSetting(requesting_origin, embedding_origin, /*info=*/nullptr);
  base::ListValue* object_list;
  if (!setting->GetList(kObjectListKey, &object_list)) {
    object_list =
        setting->SetList(kObjectListKey, std::make_unique<base::ListValue>());
  }
  object_list->AppendIfNotPresent(std::move(object));
  SetWebsiteSetting(requesting_origin, embedding_origin, std::move(setting));
  NotifyPermissionChanged();
}

void ChooserContextBase::RevokeObjectPermission(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    const base::DictionaryValue& object) {
  DCHECK_EQ(requesting_origin, requesting_origin.GetOrigin());
  DCHECK_EQ(embedding_origin, embedding_origin.GetOrigin());
  DCHECK(IsValidObject(object));

  std::unique_ptr<base::DictionaryValue> setting =
      GetWebsiteSetting(requesting_origin, embedding_origin, /*info=*/nullptr);
  base::ListValue* object_list;
  if (!setting->GetList(kObjectListKey, &object_list))
    return;
  object_list->Remove(object, nullptr);
  SetWebsiteSetting(requesting_origin, embedding_origin, std::move(setting));
  NotifyPermissionRevoked(requesting_origin, embedding_origin);
}

void ChooserContextBase::NotifyPermissionChanged() {
  for (auto& observer : permission_observer_list_) {
    observer.OnChooserObjectPermissionChanged(guard_content_settings_type_,
                                              data_content_settings_type_);
  }
}

void ChooserContextBase::NotifyPermissionRevoked(const GURL& requesting_origin,
                                                 const GURL& embedding_origin) {
  for (auto& observer : permission_observer_list_) {
    observer.OnChooserObjectPermissionChanged(guard_content_settings_type_,
                                              data_content_settings_type_);
    observer.OnPermissionRevoked(requesting_origin, embedding_origin);
  }
}

std::unique_ptr<base::DictionaryValue> ChooserContextBase::GetWebsiteSetting(
    const GURL& requesting_origin,
    const GURL& embedding_origin,
    content_settings::SettingInfo* info) {
  std::unique_ptr<base::DictionaryValue> value =
      base::DictionaryValue::From(host_content_settings_map_->GetWebsiteSetting(
          requesting_origin, embedding_origin, data_content_settings_type_,
          std::string(), info));
  if (!value)
    value.reset(new base::DictionaryValue());

  return value;
}

void ChooserContextBase::SetWebsiteSetting(const GURL& requesting_origin,
                                           const GURL& embedding_origin,
                                           std::unique_ptr<base::Value> value) {
  host_content_settings_map_->SetWebsiteSettingDefaultScope(
      requesting_origin, embedding_origin, data_content_settings_type_,
      std::string(), std::move(value));
}
