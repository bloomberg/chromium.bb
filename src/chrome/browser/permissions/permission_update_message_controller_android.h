// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERMISSIONS_PERMISSION_UPDATE_MESSAGE_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_PERMISSIONS_PERMISSION_UPDATE_MESSAGE_CONTROLLER_ANDROID_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "chrome/browser/permissions/permission_update_message_delegate_android.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "content/public/browser/web_contents_user_data.h"

namespace content {
class WebContents;
}

// A message controller to be used for requesting missing Android runtime
// permissions for previously allowed ContentSettingsTypes.
// This class exists to hold strong pointers to pending
// |PermissionUpdateMessageDelegate| instances.
// Message UI is an alternative ui to infobars.
class PermissionUpdateMessageController
    : public content::WebContentsUserData<PermissionUpdateMessageController> {
 public:
  // Creates a message to resolve conflicts in Android runtime permissions.
  // The necessary runtime permissions are generated based on the list of
  // ContentSettingsTypes passed in.
  //
  // This function can only be called with one of
  // ContentSettingsType::MEDIASTREAM_MIC,
  // ContentSettingsType::MEDIASTREAM_CAMERA,
  // ContentSettingsType::GEOLOCATION, or
  // ContentSettingsType::AR or with both
  // ContentSettingsType::MEDIASTREAM_MIC and
  // ContentSettingsType::MEDIASTREAM_CAMERA.
  //
  // The |callback| will not be triggered if `this` is deleted.
  void ShowMessage(
      const std::vector<ContentSettingsType>& content_settings_types,
      PermissionUpdatedCallback callback);

  void ShowMessage(const std::vector<std::string>& required_android_permissions,
                   int permission_icon_id,
                   int permission_msg_title_id,
                   int permission_msg_description_id,
                   PermissionUpdatedCallback callback);

  ~PermissionUpdateMessageController() override;

 private:
  friend class content::WebContentsUserData<PermissionUpdateMessageController>;
  friend class PermissionUpdateMessageControllerAndroidTest;
  explicit PermissionUpdateMessageController(
      content::WebContents* web_contents);
  void ShowMessageInternal(
      const std::vector<std::string>& required_android_permissions,
      const std::vector<std::string>& optional_android_permissions,
      const std::vector<ContentSettingsType> content_settings_types,
      int icon_id,
      int title_id,
      int description_id,
      PermissionUpdatedCallback callback);
  void DeleteMessage(PermissionUpdateMessageDelegate* delegate);

  // This function determines the expected drawables and string based on
  // given content setting types and populates the required permissions
  // and optional permissions in the meantime.
  // Returns a tuple composed of icon drawable id, title string id and
  // description string id.
  //
  // This function can only be called with one of
  // ContentSettingsType::MEDIASTREAM_MIC,
  // ContentSettingsType::MEDIASTREAM_CAMERA,
  // ContentSettingsType::GEOLOCATION, or
  // ContentSettingsType::AR or with both
  // ContentSettingsType::MEDIASTREAM_MIC and
  // ContentSettingsType::MEDIASTREAM_CAMERA.
  std::tuple<int, int, int> GetPermissionUpdateUiResourcesId(
      const std::vector<ContentSettingsType>& content_settings_types,
      std::vector<std::string>& required_permissions,
      std::vector<std::string>& optional_permissions);

  std::vector<std::unique_ptr<PermissionUpdateMessageDelegate>>
      message_delegates_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_PERMISSIONS_PERMISSION_UPDATE_MESSAGE_CONTROLLER_ANDROID_H_
