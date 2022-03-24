// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHARING_HUB_SHARING_HUB_BUBBLE_ACTION_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_SHARING_HUB_SHARING_HUB_BUBBLE_ACTION_BUTTON_H_

#include "base/bind.h"
#include "chrome/browser/ui/views/hover_button.h"

namespace sharing_hub {

class SharingHubBubbleViewImpl;
struct SharingHubAction;

// A button representing an action in the Sharing Hub bubble.
class SharingHubBubbleActionButton : public HoverButton {
 public:
  METADATA_HEADER(SharingHubBubbleActionButton);
  SharingHubBubbleActionButton(SharingHubBubbleViewImpl* bubble,
                               const SharingHubAction& action_info);
  SharingHubBubbleActionButton(const SharingHubBubbleActionButton&) = delete;
  SharingHubBubbleActionButton& operator=(const SharingHubBubbleActionButton&) =
      delete;
  ~SharingHubBubbleActionButton() override;

  int action_command_id() const { return action_command_id_; }
  bool action_is_first_party() const { return action_is_first_party_; }
  std::string action_name_for_metrics() const {
    return action_name_for_metrics_;
  }

  // views::Button:
  void UpdateBackgroundColor() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnThemeChanged() override;

 private:
  const int action_command_id_;
  const bool action_is_first_party_;
  const std::string action_name_for_metrics_;
};

}  // namespace sharing_hub

#endif  // CHROME_BROWSER_UI_VIEWS_SHARING_HUB_SHARING_HUB_BUBBLE_ACTION_BUTTON_H_
