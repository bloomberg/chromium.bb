// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_CONTROLLER_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "content/public/browser/web_contents_user_data.h"

class Profile;

namespace content {
class WebContents;
}

namespace send_tab_to_self {

class SendTabToSelfBubbleView;
struct TargetDeviceInfo;

class SendTabToSelfBubbleController
    : public content::WebContentsUserData<SendTabToSelfBubbleController> {
 public:
  ~SendTabToSelfBubbleController() override;

  static SendTabToSelfBubbleController* CreateOrGetFromWebContents(
      content::WebContents* web_contents);
  // Hides send tab to self bubble.
  void HideBubble();
  // Displays send tab to self bubble.
  void ShowBubble();

  // Returns nullptr if no bubble is currently shown.
  SendTabToSelfBubbleView* send_tab_to_self_bubble_view() const;
  // Returns the title of send tab to self bubble.
  base::string16 GetWindowTitle() const;
  // Returns the valid devices info map.
  std::map<std::string, TargetDeviceInfo> GetValidDevices() const;
  // Returns current profile.
  Profile* GetProfile() const;

  // Handles the action when the user click on one valid device. Sends tab to
  // the target device; closes the button and hides the omnibox icon.
  void OnDeviceSelected(const std::string& target_device_name,
                        const std::string& target_device_guid);
  // Close the bubble when the user click on the close button.
  void OnBubbleClosed();

 protected:
  explicit SendTabToSelfBubbleController(content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<SendTabToSelfBubbleController>;
  // Get information of valid devices.
  void FetchDeviceInfo();

  // The web_contents associated with this controller.
  content::WebContents* web_contents_;
  // Weak reference. Will be nullptr if no bubble is currently shown.
  SendTabToSelfBubbleView* send_tab_to_self_bubble_view_ = nullptr;
  // Valid devices data.
  std::map<std::string, TargetDeviceInfo> valid_devices_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(SendTabToSelfBubbleController);
};

}  // namespace send_tab_to_self

#endif  // CHROME_BROWSER_UI_SEND_TAB_TO_SELF_SEND_TAB_TO_SELF_BUBBLE_CONTROLLER_H_
