// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/base/cocoa/hover_button.h"

#import <Cocoa/Cocoa.h>

#import "ui/events/test/cocoa_test_event_utils.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"

@interface HoverButtonTestTarget : NSObject
@property(nonatomic, copy) void (^actionHandler)(id);
@end

@implementation HoverButtonTestTarget
@synthesize actionHandler = actionHandler_;

- (void)dealloc {
  [actionHandler_ release];
  [super dealloc];
}

- (IBAction)action:(id)sender {
  actionHandler_(sender);
}
@end

namespace {

class HoverButtonTest : public ui::CocoaTest {
 public:
  HoverButtonTest() {
    NSRect frame = NSMakeRect(0, 0, 20, 20);
    base::scoped_nsobject<HoverButton> button(
        [[HoverButton alloc] initWithFrame:frame]);
    button_ = button;
    [[test_window() contentView] addSubview:button_];
  }

 protected:
  void HoverAndExpect(HoverState hoverState) {
    EXPECT_EQ(kHoverStateNone, button_.hoverState);
    [button_ mouseEntered:cocoa_test_event_utils::EnterEvent()];
    EXPECT_EQ(hoverState, button_.hoverState);
    [button_ mouseExited:cocoa_test_event_utils::ExitEvent()];
    EXPECT_EQ(kHoverStateNone, button_.hoverState);
  }

  HoverButton* button_;  // Weak, owned by test_window().
};

TEST_VIEW(HoverButtonTest, button_)

TEST_F(HoverButtonTest, Hover) {
  EXPECT_EQ(kHoverStateNone, button_.hoverState);

  // Default
  HoverAndExpect(kHoverStateMouseOver);

  // Tracking disabled
  button_.trackingEnabled = NO;
  HoverAndExpect(kHoverStateNone);
  button_.trackingEnabled = YES;

  // Button disabled
  button_.enabled = NO;
  HoverAndExpect(kHoverStateNone);
  button_.enabled = YES;

  // Back to normal
  HoverAndExpect(kHoverStateMouseOver);
}

TEST_F(HoverButtonTest, Click) {
  EXPECT_EQ(kHoverStateNone, button_.hoverState);
  base::scoped_nsobject<HoverButtonTestTarget> target(
      [[HoverButtonTestTarget alloc] init]);
  __block bool action_sent = false;
  target.get().actionHandler = ^(id sender) {
    action_sent = true;
    EXPECT_EQ(kHoverStateMouseDown, button_.hoverState);
  };
  button_.target = target;
  button_.action = @selector(action:);
  const auto click = cocoa_test_event_utils::MouseClickInView(button_, 1);

  [NSApp postEvent:click.second atStart:YES];
  [button_ mouseDown:click.first];
  EXPECT_TRUE(action_sent);
  action_sent = false;

  button_.enabled = NO;
  [button_ mouseDown:click.first];
  EXPECT_FALSE(action_sent);

  EXPECT_EQ(kHoverStateNone, button_.hoverState);
}

}  // namespace
