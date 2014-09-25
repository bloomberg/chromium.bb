// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/scoped_nsobject.h"
#include "testing/gtest_mac.h"
#import "ui/base/cocoa/controls/hyperlink_text_view.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"

namespace {

class HyperlinkTextViewTest : public ui::CocoaTest {
 public:
  HyperlinkTextViewTest() {
    NSRect frame = NSMakeRect(0, 0, 50, 50);
    base::scoped_nsobject<HyperlinkTextView> view(
        [[HyperlinkTextView alloc] initWithFrame:frame]);
    view_ = view.get();
    [[test_window() contentView] addSubview:view_];
  }

  NSFont* GetDefaultFont() {
    return [NSFont labelFontOfSize:
         [NSFont systemFontSizeForControlSize:NSRegularControlSize]];
  }

  NSDictionary* GetDefaultTextAttributes() {
    const float kTextBaselineShift = -1.0;
    return @{
      NSForegroundColorAttributeName : [NSColor blackColor],
      NSCursorAttributeName : [NSCursor arrowCursor],
      NSFontAttributeName : GetDefaultFont(),
      NSBaselineOffsetAttributeName : @(kTextBaselineShift)
    };
  }

  NSMutableDictionary* GetDefaultLinkAttributes() {
    if (!linkAttributes_.get()) {
      linkAttributes_.reset(
          [[NSMutableDictionary dictionaryWithDictionary:
              GetDefaultTextAttributes()] retain]);
      [linkAttributes_ addEntriesFromDictionary:@{
          NSForegroundColorAttributeName : [NSColor blueColor],
          NSUnderlineStyleAttributeName : @(YES),
          NSCursorAttributeName : [NSCursor pointingHandCursor],
          NSUnderlineStyleAttributeName : @(NSSingleUnderlineStyle),
          NSLinkAttributeName : @""}];
    }
    return [NSMutableDictionary dictionaryWithDictionary:linkAttributes_];
  }

  HyperlinkTextView* view_;

 private:
  base::scoped_nsobject<NSMutableDictionary> linkAttributes_;
};

TEST_VIEW(HyperlinkTextViewTest, view_);

TEST_F(HyperlinkTextViewTest, TestViewConfiguration) {
  EXPECT_FALSE([view_ isEditable]);
  EXPECT_FALSE([view_ drawsBackground]);
  EXPECT_FALSE([view_ isHorizontallyResizable]);
  EXPECT_FALSE([view_ isVerticallyResizable]);
}

TEST_F(HyperlinkTextViewTest, LinkInsertion) {
  // Test that setMessage:withLink:... inserts the link text.
  [view_ setMessageAndLink:@"This is a short text message"
                  withLink:@"alarmingly "
                  atOffset:10
                      font:GetDefaultFont()
              messageColor:[NSColor blackColor]
                 linkColor:[NSColor blueColor]];
  EXPECT_NSEQ(@"This is a alarmingly short text message",
              [[view_ textStorage] string]);

  // Test insertion at end - most common use case.
  NSString* message=@"This is another test message ";
  [view_ setMessageAndLink:message
                  withLink:@"with link"
                  atOffset:[message length]
                      font:GetDefaultFont()
              messageColor:[NSColor blackColor]
                 linkColor:[NSColor blueColor]];
  EXPECT_NSEQ(@"This is another test message with link",
              [[view_ textStorage] string]);
}

TEST_F(HyperlinkTextViewTest, AttributesForMessageWithLink) {
  // Verifies text attributes are set as expected for setMessageWithLink:...
  [view_ setMessageAndLink:@"aaabbbbb"
                  withLink:@"xxxx"
                  atOffset:3
                      font:GetDefaultFont()
              messageColor:[NSColor blackColor]
                 linkColor:[NSColor blueColor]];

  NSDictionary* attributes;
  NSRange rangeLimit = NSMakeRange(0, 12);
  NSRange range;
  attributes = [[view_ textStorage] attributesAtIndex:0
                                longestEffectiveRange:&range
                                              inRange:rangeLimit];
  EXPECT_EQ(0U, range.location);
  EXPECT_EQ(3U, range.length);
  EXPECT_NSEQ(GetDefaultTextAttributes(), attributes);

  attributes = [[view_ textStorage] attributesAtIndex:3
                                longestEffectiveRange:&range
                                              inRange:rangeLimit];
  EXPECT_EQ(3U, range.location);
  EXPECT_EQ(4U, range.length);
  EXPECT_NSEQ(GetDefaultLinkAttributes(), attributes);

  attributes = [[view_ textStorage] attributesAtIndex:7
                                longestEffectiveRange:&range
                                              inRange:rangeLimit];
  EXPECT_EQ(7U, range.location);
  EXPECT_EQ(5U, range.length);
  EXPECT_NSEQ(GetDefaultTextAttributes(), attributes);

}

TEST_F(HyperlinkTextViewTest, TestSetMessage) {
  // Verifies setMessage sets text and attributes properly.
  NSString* message = @"Test message";
  [view_ setMessage:message
           withFont:GetDefaultFont()
       messageColor:[NSColor blackColor]];
  EXPECT_NSEQ(@"Test message", [[view_ textStorage] string]);

  NSDictionary* attributes;
  NSRange rangeLimit = NSMakeRange(0, [message length]);
  NSRange range;
  attributes = [[view_ textStorage] attributesAtIndex:0
                                longestEffectiveRange:&range
                                              inRange:rangeLimit];
  EXPECT_EQ(0U, range.location);
  EXPECT_EQ([message length], range.length);
  EXPECT_NSEQ(GetDefaultTextAttributes(), attributes);
}

TEST_F(HyperlinkTextViewTest, TestAddLinkRange) {
  NSString* message = @"One Two Three Four";
  [view_ setMessage:message
           withFont:GetDefaultFont()
       messageColor:[NSColor blackColor]];

  NSColor* blue = [NSColor blueColor];
  [view_ addLinkRange:NSMakeRange(4,3) withName:@"Name:Two" linkColor:blue];
  [view_ addLinkRange:NSMakeRange(14,4) withName:@"Name:Four" linkColor:blue];

  NSDictionary* attributes;
  NSRange rangeLimit = NSMakeRange(0, [message length]);
  NSRange range;
  attributes = [[view_ textStorage] attributesAtIndex:0
                                longestEffectiveRange:&range
                                              inRange:rangeLimit];
  EXPECT_EQ(0U, range.location);
  EXPECT_EQ(4U, range.length);
  EXPECT_NSEQ(GetDefaultTextAttributes(), attributes);

  NSMutableDictionary* linkAttributes = GetDefaultLinkAttributes();
  [linkAttributes setObject:@"Name:Two" forKey:NSLinkAttributeName];
  attributes = [[view_ textStorage] attributesAtIndex:4
                                longestEffectiveRange:&range
                                              inRange:rangeLimit];
  EXPECT_EQ(4U, range.location);
  EXPECT_EQ(3U, range.length);
  EXPECT_NSEQ(linkAttributes, attributes);

  attributes = [[view_ textStorage] attributesAtIndex:7
                                longestEffectiveRange:&range
                                              inRange:rangeLimit];
  EXPECT_EQ(7U, range.location);
  EXPECT_EQ(7U, range.length);
  EXPECT_NSEQ(GetDefaultTextAttributes(), attributes);

  [linkAttributes setObject:@"Name:Four" forKey:NSLinkAttributeName];
  attributes = [[view_ textStorage] attributesAtIndex:14
                                longestEffectiveRange:&range
                                              inRange:rangeLimit];
  EXPECT_EQ(14U, range.location);
  EXPECT_EQ(4U, range.length);
  EXPECT_NSEQ(linkAttributes, attributes);
}

TEST_F(HyperlinkTextViewTest, FirstResponderBehavior) {
  // By default, accept.
  EXPECT_TRUE([view_ acceptsFirstResponder]);

  [view_ setRefusesFirstResponder:YES];
  EXPECT_FALSE([view_ acceptsFirstResponder]);
}

}  // namespace
