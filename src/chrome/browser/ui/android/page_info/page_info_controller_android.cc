// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/page_info/page_info_controller_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "base/stl_util.h"
#include "chrome/android/chrome_jni_headers/PageInfoController_jni.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/ui/page_info/page_info.h"
#include "chrome/browser/ui/page_info/page_info_ui.h"
#include "chrome/common/chrome_features.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/security_state/core/security_state.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "url/origin.h"

using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;

// static
static jlong JNI_PageInfoController_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);

  return reinterpret_cast<intptr_t>(
      new PageInfoControllerAndroid(env, obj, web_contents));
}

PageInfoControllerAndroid::PageInfoControllerAndroid(
    JNIEnv* env,
    jobject java_page_info_pop,
    content::WebContents* web_contents) {
  // Important to use GetVisibleEntry to match what's showing in the omnibox.
  content::NavigationEntry* nav_entry =
      web_contents->GetController().GetVisibleEntry();
  if (nav_entry == NULL)
    return;

  url_ = nav_entry->GetURL();
  web_contents_ = web_contents;

  controller_jobject_.Reset(env, java_page_info_pop);

  SecurityStateTabHelper* helper =
      SecurityStateTabHelper::FromWebContents(web_contents);
  DCHECK(helper);

  presenter_.reset(new PageInfo(
      this, Profile::FromBrowserContext(web_contents->GetBrowserContext()),
      TabSpecificContentSettings::FromWebContents(web_contents), web_contents,
      nav_entry->GetURL(), helper->GetSecurityLevel(),
      *helper->GetVisibleSecurityState()));
}

PageInfoControllerAndroid::~PageInfoControllerAndroid() {}

void PageInfoControllerAndroid::Destroy(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  delete this;
}

void PageInfoControllerAndroid::RecordPageInfoAction(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint action) {
  presenter_->RecordPageInfoAction(
      static_cast<PageInfo::PageInfoAction>(action));
}

void PageInfoControllerAndroid::SetIdentityInfo(
    const IdentityInfo& identity_info) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::unique_ptr<PageInfoUI::SecurityDescription> security_description =
      GetSecurityDescription(identity_info);

  Java_PageInfoController_setSecurityDescription(
      env, controller_jobject_,
      ConvertUTF16ToJavaString(env, security_description->summary),
      ConvertUTF16ToJavaString(env, security_description->details));
}

void PageInfoControllerAndroid::SetCookieInfo(
    const CookieInfoList& cookie_info_list) {
  NOTIMPLEMENTED();
}

void PageInfoControllerAndroid::SetPageFeatureInfo(
    const PageFeatureInfo& info) {
  NOTIMPLEMENTED();
}

void PageInfoControllerAndroid::SetPermissionInfo(
    const PermissionInfoList& permission_info_list,
    ChosenObjectInfoList chosen_object_info_list) {
  JNIEnv* env = base::android::AttachCurrentThread();

  // On Android, we only want to display a subset of the available options in a
  // particular order, but only if their value is different from the default.
  // This order comes from https://crbug.com/610358.
  std::vector<ContentSettingsType> permissions_to_display;
  permissions_to_display.push_back(CONTENT_SETTINGS_TYPE_GEOLOCATION);
  permissions_to_display.push_back(CONTENT_SETTINGS_TYPE_MEDIASTREAM_CAMERA);
  permissions_to_display.push_back(CONTENT_SETTINGS_TYPE_MEDIASTREAM_MIC);
  permissions_to_display.push_back(CONTENT_SETTINGS_TYPE_NOTIFICATIONS);
  permissions_to_display.push_back(CONTENT_SETTINGS_TYPE_IMAGES);
  permissions_to_display.push_back(CONTENT_SETTINGS_TYPE_JAVASCRIPT);
  permissions_to_display.push_back(CONTENT_SETTINGS_TYPE_POPUPS);
  permissions_to_display.push_back(CONTENT_SETTINGS_TYPE_ADS);
  permissions_to_display.push_back(CONTENT_SETTINGS_TYPE_AUTOPLAY);
  permissions_to_display.push_back(CONTENT_SETTINGS_TYPE_SOUND);
  base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();
  if (cmd->HasSwitch(switches::kEnableWebBluetoothScanning))
    permissions_to_display.push_back(CONTENT_SETTINGS_TYPE_BLUETOOTH_SCANNING);

  std::map<ContentSettingsType, ContentSetting>
      user_specified_settings_to_display;

  for (const auto& permission : permission_info_list) {
    if (base::Contains(permissions_to_display, permission.type)) {
      base::Optional<ContentSetting> setting_to_display =
          GetSettingToDisplay(permission);
      if (setting_to_display) {
        user_specified_settings_to_display[permission.type] =
            *setting_to_display;
      }
    }
  }

  for (const auto& permission : permissions_to_display) {
    if (base::Contains(user_specified_settings_to_display, permission)) {
      base::string16 setting_title =
          PageInfoUI::PermissionTypeToUIString(permission);

      Java_PageInfoController_addPermissionSection(
          env, controller_jobject_,
          ConvertUTF16ToJavaString(env, setting_title),
          static_cast<jint>(permission),
          static_cast<jint>(user_specified_settings_to_display[permission]));
    }
  }

  for (const auto& chosen_object : chosen_object_info_list) {
    base::string16 object_title =
        PageInfoUI::ChosenObjectToUIString(*chosen_object);

    Java_PageInfoController_addPermissionSection(
        env, controller_jobject_, ConvertUTF16ToJavaString(env, object_title),
        static_cast<jint>(chosen_object->ui_info.content_settings_type),
        static_cast<jint>(CONTENT_SETTING_ALLOW));
  }

  Java_PageInfoController_updatePermissionDisplay(env, controller_jobject_);
}

base::Optional<ContentSetting> PageInfoControllerAndroid::GetSettingToDisplay(
    const PermissionInfo& permission) {
  // All permissions should be displayed if they are non-default.
  if (permission.setting != CONTENT_SETTING_DEFAULT)
    return permission.setting;

  // Handle exceptions for permissions which need to be displayed even if they
  // are set to the default.
  if (permission.type == CONTENT_SETTINGS_TYPE_ADS) {
    // The subresource filter permission should always display the default
    // setting if it is showing up in Page Info. Logic for whether the
    // setting should show up in Page Info is in ShouldShowPermission in
    // page_info.cc.
    return permission.default_setting;
  } else if (permission.type == CONTENT_SETTINGS_TYPE_SOUND) {
    // The sound content setting should always show up when the tab has played
    // audio since last navigation.
    if (web_contents_->WasEverAudible())
      return permission.default_setting;
  }
  return base::Optional<ContentSetting>();
}
