// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/cocoa/bridged_native_widget.h"

#import <Cocoa/Cocoa.h>

#import "base/mac/foundation_util.h"
#import "base/mac/mac_util.h"
#import "base/mac/sdk_forward_declarations.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "testing/gtest_mac.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"
#import "ui/views/cocoa/bridged_content_view.h"
#import "ui/views/cocoa/native_widget_mac_nswindow.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/ime/input_method.h"
#include "ui/views/view.h"
#include "ui/views/widget/native_widget_mac.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

using base::ASCIIToUTF16;
using base::SysNSStringToUTF8;
using base::SysNSStringToUTF16;
using base::SysUTF8ToNSString;

#define EXPECT_EQ_RANGE(a, b)        \
  EXPECT_EQ(a.location, b.location); \
  EXPECT_EQ(a.length, b.length);

namespace {

// Empty range shortcut for readibility.
NSRange EmptyRange() {
  return NSMakeRange(NSNotFound, 0);
}

}  // namespace

// Class to override -[NSWindow toggleFullScreen:] to a no-op. This simulates
// NSWindow's behavior when attempting to toggle fullscreen state again, when
// the last attempt failed but Cocoa has not yet sent
// windowDidFailToEnterFullScreen:.
@interface BridgedNativeWidgetTestFullScreenWindow : NativeWidgetMacNSWindow {
 @private
  int ignoredToggleFullScreenCount_;
}
@property(readonly, nonatomic) int ignoredToggleFullScreenCount;
@end

@implementation BridgedNativeWidgetTestFullScreenWindow

@synthesize ignoredToggleFullScreenCount = ignoredToggleFullScreenCount_;

- (void)toggleFullScreen:(id)sender {
  ++ignoredToggleFullScreenCount_;
}

@end

namespace views {
namespace test {

// Provides the |parent| argument to construct a BridgedNativeWidget.
class MockNativeWidgetMac : public NativeWidgetMac {
 public:
  MockNativeWidgetMac(Widget* delegate) : NativeWidgetMac(delegate) {}

  // Expose a reference, so that it can be reset() independently.
  scoped_ptr<BridgedNativeWidget>& bridge() {
    return bridge_;
  }

  // internal::NativeWidgetPrivate:
  void InitNativeWidget(const Widget::InitParams& params) override {
    ownership_ = params.ownership;

    // Usually the bridge gets initialized here. It is skipped to run extra
    // checks in tests, and so that a second window isn't created.
    delegate()->OnNativeWidgetCreated(true);

    // To allow events to dispatch to a view, it needs a way to get focus.
    bridge_->SetFocusManager(GetWidget()->GetFocusManager());
  }

  void ReorderNativeViews() override {
    // Called via Widget::Init to set the content view. No-op in these tests.
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(MockNativeWidgetMac);
};

// Helper test base to construct a BridgedNativeWidget with a valid parent.
class BridgedNativeWidgetTestBase : public ui::CocoaTest {
 public:
  BridgedNativeWidgetTestBase()
      : widget_(new Widget),
        native_widget_mac_(new MockNativeWidgetMac(widget_.get())) {
  }

  scoped_ptr<BridgedNativeWidget>& bridge() {
    return native_widget_mac_->bridge();
  }

  // Overridden from testing::Test:
  void SetUp() override {
    ui::CocoaTest::SetUp();

    init_params_.native_widget = native_widget_mac_;

    // Use a frameless window, otherwise Widget will try to center the window
    // before the tests covering the Init() flow are ready to do that.
    init_params_.type = Widget::InitParams::TYPE_WINDOW_FRAMELESS;

    // To control the lifetime without an actual window that must be closed,
    // tests in this file need to use WIDGET_OWNS_NATIVE_WIDGET.
    init_params_.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;

    // Opacity defaults to "infer" which is usually updated by ViewsDelegate.
    init_params_.opacity = Widget::InitParams::OPAQUE_WINDOW;

    native_widget_mac_->GetWidget()->Init(init_params_);
  }

 protected:
  scoped_ptr<Widget> widget_;
  MockNativeWidgetMac* native_widget_mac_;  // Weak. Owned by |widget_|.

  // Make the InitParams available to tests to cover initialization codepaths.
  Widget::InitParams init_params_;
};

class BridgedNativeWidgetTest : public BridgedNativeWidgetTestBase {
 public:
  BridgedNativeWidgetTest();
  ~BridgedNativeWidgetTest() override;

  // Install a textfield in the view hierarchy and make it the text input
  // client.
  void InstallTextField(const std::string& text);

  // Returns the current text as std::string.
  std::string GetText();

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

 protected:
  scoped_ptr<views::View> view_;
  scoped_ptr<BridgedNativeWidget> bridge_;
  BridgedContentView* ns_view_;  // Weak. Owned by bridge_.
  base::MessageLoopForUI message_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BridgedNativeWidgetTest);
};

BridgedNativeWidgetTest::BridgedNativeWidgetTest() {
}

BridgedNativeWidgetTest::~BridgedNativeWidgetTest() {
}

void BridgedNativeWidgetTest::InstallTextField(const std::string& text) {
  Textfield* textfield = new Textfield();
  textfield->SetText(ASCIIToUTF16(text));
  view_->AddChildView(textfield);

  // Request focus so the InputMethod can dispatch events to the RootView, and
  // have them delivered to the textfield. Note that focusing a textfield
  // schedules a task to flash the cursor, so this requires |message_loop_|.
  textfield->RequestFocus();

  [ns_view_ setTextInputClient:textfield];
}

std::string BridgedNativeWidgetTest::GetText() {
  NSRange range = NSMakeRange(0, NSUIntegerMax);
  NSAttributedString* text =
      [ns_view_ attributedSubstringForProposedRange:range actualRange:NULL];
  return SysNSStringToUTF8([text string]);
}

void BridgedNativeWidgetTest::SetUp() {
  BridgedNativeWidgetTestBase::SetUp();

  view_.reset(new views::internal::RootView(widget_.get()));
  base::scoped_nsobject<NSWindow> window([test_window() retain]);

  // BridgedNativeWidget expects to be initialized with a hidden (deferred)
  // window.
  [window orderOut:nil];
  EXPECT_FALSE([window delegate]);
  bridge()->Init(window, init_params_);

  // The delegate should exist before setting the root view.
  EXPECT_TRUE([window delegate]);
  bridge()->SetRootView(view_.get());
  ns_view_ = bridge()->ns_view();

  // Pretend it has been shown via NativeWidgetMac::Show().
  [window orderFront:nil];
  [test_window() makePretendKeyWindowAndSetFirstResponder:bridge()->ns_view()];
}

void BridgedNativeWidgetTest::TearDown() {
  view_.reset();
  BridgedNativeWidgetTestBase::TearDown();
}

// The TEST_VIEW macro expects the view it's testing to have a superview. In
// these tests, the NSView bridge is a contentView, at the root. These mimic
// what TEST_VIEW usually does.
TEST_F(BridgedNativeWidgetTest, BridgedNativeWidgetTest_TestViewAddRemove) {
  base::scoped_nsobject<BridgedContentView> view([bridge()->ns_view() retain]);
  EXPECT_NSEQ([test_window() contentView], view);
  EXPECT_NSEQ(test_window(), [view window]);

  // The superview of a contentView is an NSNextStepFrame.
  EXPECT_TRUE([view superview]);
  EXPECT_TRUE([view hostedView]);

  // Ensure the tracking area to propagate mouseMoved: events to the RootView is
  // installed.
  EXPECT_EQ(1u, [[view trackingAreas] count]);

  // Destroying the C++ bridge should remove references to any C++ objects in
  // the ObjectiveC object, and remove it from the hierarchy.
  bridge().reset();
  EXPECT_FALSE([view hostedView]);
  EXPECT_FALSE([view superview]);
  EXPECT_FALSE([view window]);
  EXPECT_EQ(0u, [[view trackingAreas] count]);
  EXPECT_FALSE([test_window() contentView]);
  EXPECT_FALSE([test_window() delegate]);
}

TEST_F(BridgedNativeWidgetTest, BridgedNativeWidgetTest_TestViewDisplay) {
  [bridge()->ns_view() display];
}

// Test that resizing the window resizes the root view appropriately.
TEST_F(BridgedNativeWidgetTest, ViewSizeTracksWindow) {
  const int kTestNewWidth = 400;
  const int kTestNewHeight = 300;

  // |test_window()| is borderless, so these should align.
  NSSize window_size = [test_window() frame].size;
  EXPECT_EQ(view_->width(), static_cast<int>(window_size.width));
  EXPECT_EQ(view_->height(), static_cast<int>(window_size.height));

  // Make sure a resize actually occurs.
  EXPECT_NE(kTestNewWidth, view_->width());
  EXPECT_NE(kTestNewHeight, view_->height());

  [test_window() setFrame:NSMakeRect(0, 0, kTestNewWidth, kTestNewHeight)
                  display:NO];
  EXPECT_EQ(kTestNewWidth, view_->width());
  EXPECT_EQ(kTestNewHeight, view_->height());
}

TEST_F(BridgedNativeWidgetTest, CreateInputMethodShouldNotReturnNull) {
  scoped_ptr<views::InputMethod> input_method(bridge()->CreateInputMethod());
  EXPECT_TRUE(input_method);
}

TEST_F(BridgedNativeWidgetTest, GetHostInputMethodShouldNotReturnNull) {
  EXPECT_TRUE(bridge()->GetHostInputMethod());
}

// A simpler test harness for testing initialization flows.
typedef BridgedNativeWidgetTestBase BridgedNativeWidgetInitTest;

// Test that BridgedNativeWidget remains sane if Init() is never called.
TEST_F(BridgedNativeWidgetInitTest, InitNotCalled) {
  EXPECT_FALSE(bridge()->ns_view());
  EXPECT_FALSE(bridge()->ns_window());
  bridge().reset();
}

// Test getting complete string using text input protocol.
TEST_F(BridgedNativeWidgetTest, TextInput_GetCompleteString) {
  const std::string kTestString = "foo bar baz";
  InstallTextField(kTestString);

  NSRange range = NSMakeRange(0, kTestString.size());
  NSRange actual_range;
  NSAttributedString* text =
      [ns_view_ attributedSubstringForProposedRange:range
                                        actualRange:&actual_range];
  EXPECT_EQ(kTestString, SysNSStringToUTF8([text string]));
  EXPECT_EQ_RANGE(range, actual_range);
}

// Test getting middle substring using text input protocol.
TEST_F(BridgedNativeWidgetTest, TextInput_GetMiddleSubstring) {
  const std::string kTestString = "foo bar baz";
  InstallTextField(kTestString);

  NSRange range = NSMakeRange(4, 3);
  NSRange actual_range;
  NSAttributedString* text =
      [ns_view_ attributedSubstringForProposedRange:range
                                        actualRange:&actual_range];
  EXPECT_EQ("bar", SysNSStringToUTF8([text string]));
  EXPECT_EQ_RANGE(range, actual_range);
}

// Test getting ending substring using text input protocol.
TEST_F(BridgedNativeWidgetTest, TextInput_GetEndingSubstring) {
  const std::string kTestString = "foo bar baz";
  InstallTextField(kTestString);

  NSRange range = NSMakeRange(8, 100);
  NSRange actual_range;
  NSAttributedString* text =
      [ns_view_ attributedSubstringForProposedRange:range
                                        actualRange:&actual_range];
  EXPECT_EQ("baz", SysNSStringToUTF8([text string]));
  EXPECT_EQ(range.location, actual_range.location);
  EXPECT_EQ(3U, actual_range.length);
}

// Test getting empty substring using text input protocol.
TEST_F(BridgedNativeWidgetTest, TextInput_GetEmptySubstring) {
  const std::string kTestString = "foo bar baz";
  InstallTextField(kTestString);

  NSRange range = EmptyRange();
  NSRange actual_range;
  NSAttributedString* text =
      [ns_view_ attributedSubstringForProposedRange:range
                                        actualRange:&actual_range];
  EXPECT_EQ("", SysNSStringToUTF8([text string]));
  EXPECT_EQ_RANGE(range, actual_range);
}

// Test inserting text using text input protocol.
TEST_F(BridgedNativeWidgetTest, TextInput_InsertText) {
  const std::string kTestString = "foo";
  InstallTextField(kTestString);

  [ns_view_ insertText:SysUTF8ToNSString(kTestString)
      replacementRange:EmptyRange()];
  gfx::Range range(0, kTestString.size());
  base::string16 text;
  EXPECT_TRUE([ns_view_ textInputClient]->GetTextFromRange(range, &text));
  EXPECT_EQ(ASCIIToUTF16(kTestString), text);
}

// Test replacing text using text input protocol.
TEST_F(BridgedNativeWidgetTest, TextInput_ReplaceText) {
  const std::string kTestString = "foo bar";
  InstallTextField(kTestString);

  [ns_view_ insertText:@"baz" replacementRange:NSMakeRange(4, 3)];
  EXPECT_EQ("foo baz", GetText());
}

// Test IME composition using text input protocol.
TEST_F(BridgedNativeWidgetTest, TextInput_Compose) {
  const std::string kTestString = "foo ";
  InstallTextField(kTestString);

  EXPECT_FALSE([ns_view_ hasMarkedText]);
  EXPECT_EQ_RANGE(EmptyRange(), [ns_view_ markedRange]);

  // Start composition.
  NSString* compositionText = @"bar";
  NSUInteger compositionLength = [compositionText length];
  [ns_view_ setMarkedText:compositionText
            selectedRange:NSMakeRange(0, 2)
         replacementRange:EmptyRange()];
  EXPECT_TRUE([ns_view_ hasMarkedText]);
  EXPECT_EQ_RANGE(NSMakeRange(kTestString.size(), compositionLength),
                  [ns_view_ markedRange]);
  EXPECT_EQ_RANGE(NSMakeRange(kTestString.size(), 2), [ns_view_ selectedRange]);

  // Confirm composition.
  [ns_view_ unmarkText];
  EXPECT_FALSE([ns_view_ hasMarkedText]);
  EXPECT_EQ_RANGE(EmptyRange(), [ns_view_ markedRange]);
  EXPECT_EQ("foo bar", GetText());
  EXPECT_EQ_RANGE(NSMakeRange(GetText().size(), 0), [ns_view_ selectedRange]);
}

// Test moving the caret left and right using text input protocol.
TEST_F(BridgedNativeWidgetTest, TextInput_MoveLeftRight) {
  InstallTextField("foo");
  EXPECT_EQ_RANGE(NSMakeRange(3, 0), [ns_view_ selectedRange]);

  // Move right not allowed, out of range.
  [ns_view_ doCommandBySelector:@selector(moveRight:)];
  EXPECT_EQ_RANGE(NSMakeRange(3, 0), [ns_view_ selectedRange]);

  // Move left.
  [ns_view_ doCommandBySelector:@selector(moveLeft:)];
  EXPECT_EQ_RANGE(NSMakeRange(2, 0), [ns_view_ selectedRange]);

  // Move right.
  [ns_view_ doCommandBySelector:@selector(moveRight:)];
  EXPECT_EQ_RANGE(NSMakeRange(3, 0), [ns_view_ selectedRange]);
}

// Test backward delete using text input protocol.
TEST_F(BridgedNativeWidgetTest, TextInput_DeleteBackward) {
  InstallTextField("a");
  EXPECT_EQ_RANGE(NSMakeRange(1, 0), [ns_view_ selectedRange]);

  // Delete one character.
  [ns_view_ doCommandBySelector:@selector(deleteBackward:)];
  EXPECT_EQ("", GetText());
  EXPECT_EQ_RANGE(NSMakeRange(0, 0), [ns_view_ selectedRange]);

  // Try to delete again on an empty string.
  [ns_view_ doCommandBySelector:@selector(deleteBackward:)];
  EXPECT_EQ("", GetText());
  EXPECT_EQ_RANGE(NSMakeRange(0, 0), [ns_view_ selectedRange]);
}

// Test forward delete using text input protocol.
TEST_F(BridgedNativeWidgetTest, TextInput_DeleteForward) {
  InstallTextField("a");
  EXPECT_EQ_RANGE(NSMakeRange(1, 0), [ns_view_ selectedRange]);

  // At the end of the string, can't delete forward.
  [ns_view_ doCommandBySelector:@selector(deleteForward:)];
  EXPECT_EQ("a", GetText());
  EXPECT_EQ_RANGE(NSMakeRange(1, 0), [ns_view_ selectedRange]);

  // Should succeed after moving left first.
  [ns_view_ doCommandBySelector:@selector(moveLeft:)];
  [ns_view_ doCommandBySelector:@selector(deleteForward:)];
  EXPECT_EQ("", GetText());
  EXPECT_EQ_RANGE(NSMakeRange(0, 0), [ns_view_ selectedRange]);
}

typedef BridgedNativeWidgetTestBase BridgedNativeWidgetSimulateFullscreenTest;

// Simulate the notifications that AppKit would send out if a fullscreen
// operation begins, and then fails and must abort. This notification sequence
// was determined by posting delayed tasks to toggle fullscreen state and then
// mashing Ctrl+Left/Right to keep OSX in a transition between Spaces to cause
// the fullscreen transition to fail.
TEST_F(BridgedNativeWidgetSimulateFullscreenTest, FailToEnterAndExit) {
  if (base::mac::IsOSSnowLeopard())
    return;

  base::scoped_nsobject<NSWindow> owned_window(
      [[BridgedNativeWidgetTestFullScreenWindow alloc]
          initWithContentRect:NSMakeRect(50, 50, 400, 300)
                    styleMask:NSBorderlessWindowMask
                      backing:NSBackingStoreBuffered
                        defer:YES]);
  [owned_window setReleasedWhenClosed:NO];  // Owned by scoped_nsobject.
  bridge()->Init(owned_window, init_params_);  // Transfers ownership.

  BridgedNativeWidgetTestFullScreenWindow* window =
      base::mac::ObjCCastStrict<BridgedNativeWidgetTestFullScreenWindow>(
          widget_->GetNativeWindow());
  widget_->Show();

  NSNotificationCenter* center = [NSNotificationCenter defaultCenter];

  EXPECT_FALSE(bridge()->target_fullscreen_state());

  // Simulate an initial toggleFullScreen: (user- or Widget-initiated).
  [center postNotificationName:NSWindowWillEnterFullScreenNotification
                        object:window];

  // On a failure, Cocoa starts by sending an unexpected *exit* fullscreen, and
  // BridgedNativeWidget will think it's just a delayed transition and try to go
  // back into fullscreen but get ignored by Cocoa.
  EXPECT_EQ(0, [window ignoredToggleFullScreenCount]);
  EXPECT_TRUE(bridge()->target_fullscreen_state());
  [center postNotificationName:NSWindowDidExitFullScreenNotification
                        object:window];
  EXPECT_EQ(1, [window ignoredToggleFullScreenCount]);
  EXPECT_FALSE(bridge()->target_fullscreen_state());

  // Cocoa follows up with a failure message sent to the NSWindowDelegate (there
  // is no equivalent notification for failure). Called via id so that this
  // compiles on 10.6.
  id window_delegate = [window delegate];
  [window_delegate windowDidFailToEnterFullScreen:window];
  EXPECT_FALSE(bridge()->target_fullscreen_state());

  // Now perform a successful fullscreen operation.
  [center postNotificationName:NSWindowWillEnterFullScreenNotification
                        object:window];
  EXPECT_TRUE(bridge()->target_fullscreen_state());
  [center postNotificationName:NSWindowDidEnterFullScreenNotification
                        object:window];
  EXPECT_TRUE(bridge()->target_fullscreen_state());

  // And try to get out.
  [center postNotificationName:NSWindowWillExitFullScreenNotification
                        object:window];
  EXPECT_FALSE(bridge()->target_fullscreen_state());

  // On a failure, Cocoa sends a failure message, but then just dumps the window
  // out of fullscreen anyway (in that order).
  [window_delegate windowDidFailToExitFullScreen:window];
  EXPECT_FALSE(bridge()->target_fullscreen_state());
  [center postNotificationName:NSWindowDidExitFullScreenNotification
                        object:window];
  EXPECT_EQ(1, [window ignoredToggleFullScreenCount]);  // No change.
  EXPECT_FALSE(bridge()->target_fullscreen_state());

  widget_->CloseNow();
}

}  // namespace test
}  // namespace views
