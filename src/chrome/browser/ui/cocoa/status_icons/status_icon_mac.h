// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_STATUS_ICONS_STATUS_ICON_MAC_H_
#define CHROME_BROWSER_UI_COCOA_STATUS_ICONS_STATUS_ICON_MAC_H_

#import <Cocoa/Cocoa.h>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "chrome/browser/status_icons/desktop_notification_balloon.h"
#include "chrome/browser/status_icons/status_icon.h"

@class MenuControllerCocoa;
@class NSStatusItem;
@class StatusItemController;

class StatusIconMac : public StatusIcon {
 public:
  StatusIconMac();
  ~StatusIconMac() override;

  // Overridden from StatusIcon.
  void SetImage(const gfx::ImageSkia& image) override;
  void SetToolTip(const base::string16& tool_tip) override;
  void DisplayBalloon(const gfx::ImageSkia& icon,
                      const base::string16& title,
                      const base::string16& contents,
                      const message_center::NotifierId& notifier_id) override;

  bool HasStatusIconMenu();

 protected:
  // Overridden from StatusIcon.
  void UpdatePlatformContextMenu(StatusIconMenuModel* model) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(StatusIconMacTest, CreateMenu);
  FRIEND_TEST_ALL_PREFIXES(StatusIconMacTest, MenuToolTip);

  void SetToolTip(NSString* toolTip);
  void CreateMenu(ui::MenuModel* model, NSString* toolTip);

  // Getter for item_ that allows lazy initialization.
  NSStatusItem* item();
  base::scoped_nsobject<NSStatusItem> item_;

  base::scoped_nsobject<StatusItemController> controller_;

  // Notification balloon.
  DesktopNotificationBalloon notification_;

  base::scoped_nsobject<NSString> toolTip_;

  // Status menu shown when right-clicking the system icon, if it has been
  // created by |UpdatePlatformContextMenu|.
  base::scoped_nsobject<MenuControllerCocoa> menu_;

  DISALLOW_COPY_AND_ASSIGN(StatusIconMac);
};

#endif // CHROME_BROWSER_UI_COCOA_STATUS_ICONS_STATUS_ICON_MAC_H_
