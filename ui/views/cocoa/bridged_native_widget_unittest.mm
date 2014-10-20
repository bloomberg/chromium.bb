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
#include "base/run_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "testing/gtest_mac.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"
#import "ui/views/cocoa/bridged_content_view.h"
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

@interface NativeWidgetMacNotificationWaiter : NSObject {
 @private
  scoped_ptr<base::RunLoop> runLoop_;
  base::scoped_nsobject<NSWindow> window_;
  int enterCount_;
  int exitCount_;
  int targetEnterCount_;
  int targetExitCount_;
}

@property(readonly, nonatomic) int enterCount;
@property(readonly, nonatomic) int exitCount;

// Initialize for the given window and start tracking notifications.
- (id)initWithWindow:(NSWindow*)window;

// Keep spinning a run loop until the enter and exit counts match.
- (void)waitForEnterCount:(int)enterCount exitCount:(int)exitCount;

// private:
// Exit the RunLoop if there is one and the counts being tracked match.
- (void)maybeQuitForChangedArg:(int*)changedArg;

- (void)onEnter:(NSNotification*)notification;
- (void)onExit:(NSNotification*)notification;

@end

@implementation NativeWidgetMacNotificationWaiter

@synthesize enterCount = enterCount_;
@synthesize exitCount = exitCount_;

- (id)initWithWindow:(NSWindow*)window {
  if ((self = [super init])) {
    window_.reset([window retain]);
    NSNotificationCenter* defaultCenter = [NSNotificationCenter defaultCenter];
    [defaultCenter addObserver:self
                      selector:@selector(onEnter:)
                          name:NSWindowDidEnterFullScreenNotification
                        object:window];
    [defaultCenter addObserver:self
                      selector:@selector(onExit:)
                          name:NSWindowDidExitFullScreenNotification
                        object:window];
  }
  return self;
}

- (void)dealloc {
  DCHECK(!runLoop_);
  [[NSNotificationCenter defaultCenter] removeObserver:self];
  [super dealloc];
}

- (void)waitForEnterCount:(int)enterCount exitCount:(int)exitCount {
  if (enterCount_ >= enterCount && exitCount_ >= exitCount)
    return;

  targetEnterCount_ = enterCount;
  targetExitCount_ = exitCount;
  runLoop_.reset(new base::RunLoop);
  runLoop_->Run();
  runLoop_.reset();
}

- (void)maybeQuitForChangedArg:(int*)changedArg {
  ++*changedArg;
  if (!runLoop_)
    return;

  if (enterCount_ >= targetEnterCount_ && exitCount_ >= targetExitCount_)
    runLoop_->Quit();
}

- (void)onEnter:(NSNotification*)notification {
  [self maybeQuitForChangedArg:&enterCount_];
}

- (void)onExit:(NSNotification*)notification {
  [self maybeQuitForChangedArg:&exitCount_];
}

@end

// Class to override -[NSWindow toggleFullScreen:] to a no-op. This simulates
// NSWindow's behavior when attempting to toggle fullscreen state again, when
// the last attempt failed but Cocoa has not yet sent
// windowDidFailToEnterFullScreen:.
@interface BridgedNativeWidgetTestFullScreenWindow : NSWindow {
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
  virtual void InitNativeWidget(const Widget::InitParams& params) override {
    ownership_ = params.ownership;

    // Usually the bridge gets initialized here. It is skipped to run extra
    // checks in tests, and so that a second window isn't created.
    delegate()->OnNativeWidgetCreated(true);
  }

  virtual void ReorderNativeViews() override {
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
  virtual void SetUp() override {
    ui::CocoaTest::SetUp();

    Widget::InitParams params;
    params.native_widget = native_widget_mac_;
    // To control the lifetime without an actual window that must be closed,
    // tests in this file need to use WIDGET_OWNS_NATIVE_WIDGET.
    params.ownership = Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    native_widget_mac_->GetWidget()->Init(params);
  }

 protected:
  scoped_ptr<Widget> widget_;
  MockNativeWidgetMac* native_widget_mac_;  // Weak. Owned by |widget_|.
};

class BridgedNativeWidgetTest : public BridgedNativeWidgetTestBase {
 public:
  BridgedNativeWidgetTest();
  virtual ~BridgedNativeWidgetTest();

  // Install a textfield in the view hierarchy and make it the text input
  // client.
  void InstallTextField(const std::string& text);

  // Returns the current text as std::string.
  std::string GetText();

  // testing::Test:
  virtual void SetUp() override;
  virtual void TearDown() override;

 protected:
  scoped_ptr<views::View> view_;
  scoped_ptr<BridgedNativeWidget> bridge_;
  BridgedContentView* ns_view_;  // Weak. Owned by bridge_.

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
  bridge()->Init(window, Widget::InitParams());

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

// Test attaching to a parent window that is not a NativeWidgetMac. When the
// parent is a NativeWidgetMac, that is covered in widget_unittest.cc by
// WidgetOwnershipTest.Ownership_ViewsNativeWidgetOwnsWidget*.
TEST_F(BridgedNativeWidgetInitTest, ParentWindowNotNativeWidgetMac) {
  Widget::InitParams params;
  params.parent = [test_window() contentView];
  EXPECT_EQ(0u, [[test_window() childWindows] count]);

  base::scoped_nsobject<NSWindow> child_window(
      [[NSWindow alloc] initWithContentRect:NSMakeRect(50, 50, 400, 300)
                                  styleMask:NSBorderlessWindowMask
                                    backing:NSBackingStoreBuffered
                                      defer:YES]);
  [child_window setReleasedWhenClosed:NO];  // Owned by scoped_nsobject.

  EXPECT_FALSE([child_window parentWindow]);
  bridge()->Init(child_window, params);

  EXPECT_EQ(1u, [[test_window() childWindows] count]);
  EXPECT_EQ(test_window(), [bridge()->ns_window() parentWindow]);
  bridge().reset();
  EXPECT_EQ(0u, [[test_window() childWindows] count]);
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

// Tests for correct fullscreen tracking, regardless of whether it is initiated
// by the Widget code or elsewhere (e.g. by the user).
TEST_F(BridgedNativeWidgetTest, FullscreenSynchronousState) {
  EXPECT_FALSE(widget_->IsFullscreen());
  if (base::mac::IsOSSnowLeopard())
    return;

  // Allow user-initiated fullscreen changes on the Window.
  [test_window()
      setCollectionBehavior:[test_window() collectionBehavior] |
                            NSWindowCollectionBehaviorFullScreenPrimary];

  base::scoped_nsobject<NativeWidgetMacNotificationWaiter> waiter(
      [[NativeWidgetMacNotificationWaiter alloc] initWithWindow:test_window()]);
  const gfx::Rect restored_bounds = widget_->GetRestoredBounds();

  // Simulate a user-initiated fullscreen. Note trying to to this again before
  // spinning a runloop will cause Cocoa to emit text to stdio and ignore it.
  [test_window() toggleFullScreen:nil];
  EXPECT_TRUE(widget_->IsFullscreen());
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());

  // Note there's now an animation running. While that's happening, toggling the
  // state should work as expected, but do "nothing".
  widget_->SetFullscreen(false);
  EXPECT_FALSE(widget_->IsFullscreen());
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());
  widget_->SetFullscreen(false);  // Same request - should no-op.
  EXPECT_FALSE(widget_->IsFullscreen());
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());

  widget_->SetFullscreen(true);
  EXPECT_TRUE(widget_->IsFullscreen());
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());

  // Always finish out of fullscreen. Otherwise there are 4 NSWindow objects
  // that Cocoa creates which don't close themselves and will be seen by the Mac
  // test harness on teardown. Note that the test harness will be waiting until
  // all animations complete, since these temporary animation windows will not
  // be removed from the window list until they do.
  widget_->SetFullscreen(false);
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());

  // Now we must wait for the notifications. Since, if the widget is torn down,
  // the NSWindowDelegate is removed, and the pending request to take out of
  // fullscreen is lost. Since a message loop has not yet spun up in this test
  // we can reliably say there will be one enter and one exit, despite all the
  // toggling above.
  base::MessageLoopForUI message_loop;
  [waiter waitForEnterCount:1 exitCount:1];
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());
}

// Test fullscreen without overlapping calls and without changing collection
// behavior on the test window.
TEST_F(BridgedNativeWidgetTest, FullscreenEnterAndExit) {
  base::MessageLoopForUI message_loop;
  base::scoped_nsobject<NativeWidgetMacNotificationWaiter> waiter(
      [[NativeWidgetMacNotificationWaiter alloc] initWithWindow:test_window()]);

  EXPECT_FALSE(widget_->IsFullscreen());
  const gfx::Rect restored_bounds = widget_->GetRestoredBounds();
  EXPECT_FALSE(restored_bounds.IsEmpty());

  // Ensure this works without having to change collection behavior as for the
  // test above.
  widget_->SetFullscreen(true);
  if (base::mac::IsOSSnowLeopard()) {
    // On Snow Leopard, SetFullscreen() isn't implemented. But shouldn't crash.
    EXPECT_FALSE(widget_->IsFullscreen());
    return;
  }

  EXPECT_TRUE(widget_->IsFullscreen());
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());

  // Should be zero until the runloop spins.
  EXPECT_EQ(0, [waiter enterCount]);
  [waiter waitForEnterCount:1 exitCount:0];

  // Verify it hasn't exceeded.
  EXPECT_EQ(1, [waiter enterCount]);
  EXPECT_EQ(0, [waiter exitCount]);
  EXPECT_TRUE(widget_->IsFullscreen());
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());

  widget_->SetFullscreen(false);
  EXPECT_FALSE(widget_->IsFullscreen());
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());

  [waiter waitForEnterCount:1 exitCount:1];
  EXPECT_EQ(1, [waiter enterCount]);
  EXPECT_EQ(1, [waiter exitCount]);
  EXPECT_EQ(restored_bounds, widget_->GetRestoredBounds());
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
  bridge()->Init(owned_window, Widget::InitParams());  // Transfers ownership.

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
