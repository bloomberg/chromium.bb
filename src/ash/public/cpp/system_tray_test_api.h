// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SYSTEM_TRAY_TEST_API_H_
#define ASH_PUBLIC_CPP_SYSTEM_TRAY_TEST_API_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/strings/string16.h"

namespace message_center {
class MessagePopupView;
}

namespace ash {

// Public test API for the system tray. Methods only apply to the system tray
// on the primary display.
class ASH_EXPORT SystemTrayTestApi {
 public:
  static std::unique_ptr<SystemTrayTestApi> Create();

  SystemTrayTestApi();
  ~SystemTrayTestApi();

  // Returns true if the system tray bubble menu is open.
  bool IsTrayBubbleOpen();

  // Returns true if the system tray bubble menu is expanded.
  bool IsTrayBubbleExpanded();

  // Shows the system tray bubble menu.
  void ShowBubble();

  // Closes the system tray bubble menu.
  void CloseBubble();

  // Collapse the system tray bubble menu.
  void CollapseBubble();

  // Expand the system tray bubble menu.
  void ExpandBubble();

  // Shows the submenu view for the given section of the bubble menu.
  void ShowAccessibilityDetailedView();
  void ShowNetworkDetailedView();

  // Returns true if the view exists in the bubble and is visible.
  // If |open_tray| is true, it also opens system tray bubble.
  bool IsBubbleViewVisible(int view_id, bool open_tray);

  // Clicks the view |view_id|.
  void ClickBubbleView(int view_id);

  // Returns the tooltip for a bubble view, or the empty string if the view
  // does not exist.
  base::string16 GetBubbleViewTooltip(int view_id);

  // Get the notification pop up view based on the notification id.
  message_center::MessagePopupView* GetPopupViewForNotificationID(
      const std::string& notification_id);

  // Returns true if the clock is using 24 hour time.
  bool Is24HourClock();

  // Taps on the Select-to-Speak tray.
  void TapSelectToSpeakTray();
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SYSTEM_TRAY_TEST_API_H_
