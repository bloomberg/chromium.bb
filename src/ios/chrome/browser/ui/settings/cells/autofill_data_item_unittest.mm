// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/cells/autofill_data_item.h"

#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using AutofillDataItemTest = PlatformTest;

// Tests that the UILabels are set properly after a call to |configureCell:|.
TEST_F(AutofillDataItemTest, TextLabels) {
  AutofillDataItem* item = [[AutofillDataItem alloc] initWithType:0];
  NSString* mainText = @"Main text";
  NSString* leadingDetailText = @"Leading detail text";
  NSString* trailingDetailText = @"Trailing detail text";

  item.text = mainText;
  item.leadingDetailText = leadingDetailText;
  item.trailingDetailText = trailingDetailText;
  item.accessoryType = UITableViewCellAccessoryCheckmark;

  id cell = [[[item cellClass] alloc] init];
  ASSERT_TRUE([cell isMemberOfClass:[AutofillDataCell class]]);

  AutofillDataCell* autofillDataCell = cell;
  EXPECT_FALSE(autofillDataCell.textLabel.text);
  EXPECT_FALSE(autofillDataCell.leadingDetailTextLabel.text);
  EXPECT_FALSE(autofillDataCell.trailingDetailTextLabel.text);
  EXPECT_EQ(UITableViewCellAccessoryNone, autofillDataCell.accessoryType);

  [item configureCell:cell withStyler:[[ChromeTableViewStyler alloc] init]];
  EXPECT_NSEQ(mainText, autofillDataCell.textLabel.text);
  EXPECT_NSEQ(leadingDetailText, autofillDataCell.leadingDetailTextLabel.text);
  EXPECT_NSEQ(trailingDetailText,
              autofillDataCell.trailingDetailTextLabel.text);
  EXPECT_EQ(UITableViewCellAccessoryCheckmark, autofillDataCell.accessoryType);
}
