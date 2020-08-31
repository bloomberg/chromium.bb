// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/settings_multiline_detail_item.h"

#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using SettingsMultilineDetailItemTest = PlatformTest;

// Tests that the text and detail text are honoured after a call to
// |configureCell:|.
TEST_F(SettingsMultilineDetailItemTest, ConfigureCell) {
  SettingsMultilineDetailItem* item =
      [[SettingsMultilineDetailItem alloc] initWithType:0];
  NSString* text = @"Test Text";
  NSString* detailText = @"Test Detail Text that can span multiple lines. For "
                         @"example, this line probably fits on three or four "
                         @"lines.";

  item.text = text;
  item.detailText = detailText;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[SettingsMultilineDetailCell class]]);

  SettingsMultilineDetailCell* multilineDetailCell =
      static_cast<SettingsMultilineDetailCell*>(cell);
  EXPECT_FALSE(multilineDetailCell.textLabel.text);
  EXPECT_FALSE(multilineDetailCell.detailTextLabel.text);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_NSEQ(text, multilineDetailCell.textLabel.text);
  EXPECT_NSEQ(detailText, multilineDetailCell.detailTextLabel.text);
}

// Tests that the text label of an SettingsMultilineDetailCell only has one
// line but the detail text label spans multiple lines.
TEST_F(SettingsMultilineDetailItemTest, MultipleLines) {
  SettingsMultilineDetailCell* cell =
      [[SettingsMultilineDetailCell alloc] init];
  EXPECT_EQ(0, cell.textLabel.numberOfLines);
  EXPECT_EQ(0, cell.detailTextLabel.numberOfLines);
}

}  // namespace
