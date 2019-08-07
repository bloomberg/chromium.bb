// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/status_icons/status_icon_menu_model.h"
#include "chrome/browser/ui/cocoa/status_icons/status_icon_mac.h"
#import "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

class SkBitmap;

class StatusIconMacTest : public CocoaTest {
};

TEST_F(StatusIconMacTest, Create) {
  // Create an icon, set the tool tip, then shut it down (checks for leaks).
  std::unique_ptr<StatusIcon> icon(new StatusIconMac());
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::ImageSkia* image = rb.GetImageSkiaNamed(IDR_STATUS_TRAY_ICON);
  icon->SetImage(*image);
  icon->SetToolTip(base::ASCIIToUTF16("tool tip"));
}

TEST_F(StatusIconMacTest, CreateMenu) {
  // Create a menu and verify by getting the title of the first menu item.
  const char* menu_title = "Menu Title";
  std::unique_ptr<StatusIconMenuModel> model(new StatusIconMenuModel(NULL));
  model->AddItem(0, base::ASCIIToUTF16(menu_title));

  std::unique_ptr<StatusIconMac> icon(new StatusIconMac());
  icon->UpdatePlatformContextMenu(model.get());
  EXPECT_EQ(1, [[icon->item() menu] numberOfItems]);

  NSMenuItem* menuItem = [[icon->item() menu] itemAtIndex:0];
  EXPECT_STREQ(menu_title, [[menuItem title] UTF8String]);
}

TEST_F(StatusIconMacTest, MenuToolTip) {
  // Create a menu and set a tool tip. Verify the tool tip is inserted as the
  // first menu item.
  const char* menu_title = "Menu Title";
  const char* tool_tip = "Tool tip";
  std::unique_ptr<StatusIconMenuModel> model(new StatusIconMenuModel(NULL));
  model->AddItem(0, base::ASCIIToUTF16(menu_title));

  std::unique_ptr<StatusIconMac> icon(new StatusIconMac());
  icon->UpdatePlatformContextMenu(model.get());
  icon->SetToolTip(base::ASCIIToUTF16(tool_tip));
  EXPECT_EQ(2, [[icon->item() menu] numberOfItems]);

  NSMenuItem* toolTipItem = [[icon->item() menu] itemAtIndex:0];
  EXPECT_STREQ(tool_tip, [[toolTipItem title] UTF8String]);
  NSMenuItem* menuItem = [[icon->item() menu] itemAtIndex:1];
  EXPECT_STREQ(menu_title, [[menuItem title] UTF8String]);
}
