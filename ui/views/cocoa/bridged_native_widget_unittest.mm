// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/cocoa/bridged_native_widget.h"

#import <Cocoa/Cocoa.h>

#include <memory>

#import "base/mac/foundation_util.h"
#import "base/mac/mac_util.h"
#import "base/mac/sdk_forward_declarations.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#import "testing/gtest_mac.h"
#import "ui/base/cocoa/window_size_constants.h"
#include "ui/base/ime/input_method.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"
#import "ui/views/cocoa/bridged_content_view.h"
#import "ui/views/cocoa/native_widget_mac_nswindow.h"
#import "ui/views/cocoa/views_nswindow_delegate.h"
#include "ui/views/controls/textfield/textfield.h"
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

// Sets |composition_text| as the composition text with caret placed at
// |caret_pos| and updates |caret_range|.
void SetCompositionText(ui::TextInputClient* client,
                        const base::string16& composition_text,
                        const int caret_pos,
                        NSRange* caret_range) {
  ui::CompositionText composition;
  composition.selection = gfx::Range(caret_pos);
  composition.text = composition_text;
  client->SetCompositionText(composition);
  if (caret_range)
    *caret_range = NSMakeRange(caret_pos, 0);
}

// Returns a zero width rectangle corresponding to current caret position.
gfx::Rect GetCaretBounds(const ui::TextInputClient* client) {
  gfx::Rect caret_bounds = client->GetCaretBounds();
  caret_bounds.set_width(0);
  return caret_bounds;
}

// Returns a zero width rectangle corresponding to caret bounds when it's placed
// at |caret_pos| and updates |caret_range|.
gfx::Rect GetCaretBoundsForPosition(ui::TextInputClient* client,
                                    const base::string16& composition_text,
                                    const int caret_pos,
                                    NSRange* caret_range) {
  SetCompositionText(client, composition_text, caret_pos, caret_range);
  return GetCaretBounds(client);
}

// Returns the expected boundary rectangle for characters of |composition_text|
// within the |query_range|.
gfx::Rect GetExpectedBoundsForRange(ui::TextInputClient* client,
                                    const base::string16& composition_text,
                                    NSRange query_range) {
  gfx::Rect left_caret = GetCaretBoundsForPosition(
      client, composition_text, query_range.location, nullptr);
  gfx::Rect right_caret = GetCaretBoundsForPosition(
      client, composition_text, query_range.location + query_range.length,
      nullptr);

  // The expected bounds correspond to the area between the left and right caret
  // positions.
  return gfx::Rect(left_caret.x(), left_caret.y(),
                   right_caret.x() - left_caret.x(), left_caret.height());
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
  std::unique_ptr<BridgedNativeWidget>& bridge() { return bridge_; }

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

  std::unique_ptr<BridgedNativeWidget>& bridge() {
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

    init_params_.bounds = gfx::Rect(100, 100, 100, 100);

    native_widget_mac_->GetWidget()->Init(init_params_);
  }

 protected:
  std::unique_ptr<Widget> widget_;
  MockNativeWidgetMac* native_widget_mac_;  // Weak. Owned by |widget_|.

  // Make the InitParams available to tests to cover initialization codepaths.
  Widget::InitParams init_params_;
};

class BridgedNativeWidgetTest : public BridgedNativeWidgetTestBase {
 public:
  BridgedNativeWidgetTest();
  ~BridgedNativeWidgetTest() override;

  // Install a textfield with input type |text_input_type| in the view hierarchy
  // and make it the text input client.
  void InstallTextField(const std::string& text,
                        ui::TextInputType text_input_type);

  // Install a textfield with input type ui::TEXT_INPUT_TYPE_TEXT in the view
  // hierarchy and make it the text input client.
  void InstallTextField(const std::string& text);

  // Returns the current text as std::string.
  std::string GetText();

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

 protected:
  std::unique_ptr<views::View> view_;
  BridgedContentView* ns_view_;  // Weak. Owned by bridge().
  base::MessageLoopForUI message_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BridgedNativeWidgetTest);
};

BridgedNativeWidgetTest::BridgedNativeWidgetTest() {
}

BridgedNativeWidgetTest::~BridgedNativeWidgetTest() {
}

void BridgedNativeWidgetTest::InstallTextField(
    const std::string& text,
    ui::TextInputType text_input_type) {
  Textfield* textfield = new Textfield();
  textfield->SetText(ASCIIToUTF16(text));
  textfield->SetTextInputType(text_input_type);
  view_->RemoveAllChildViews(true);
  view_->AddChildView(textfield);
  textfield->SetBoundsRect(init_params_.bounds);

  // Request focus so the InputMethod can dispatch events to the RootView, and
  // have them delivered to the textfield. Note that focusing a textfield
  // schedules a task to flash the cursor, so this requires |message_loop_|.
  textfield->RequestFocus();

  [ns_view_ setTextInputClient:textfield];
}

void BridgedNativeWidgetTest::InstallTextField(const std::string& text) {
  InstallTextField(text, ui::TEXT_INPUT_TYPE_TEXT);
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
  if (bridge())
    bridge()->SetRootView(nullptr);
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

TEST_F(BridgedNativeWidgetTest, GetInputMethodShouldNotReturnNull) {
  EXPECT_TRUE(bridge()->GetInputMethod());
}

// A simpler test harness for testing initialization flows.
class BridgedNativeWidgetInitTest : public BridgedNativeWidgetTestBase {
 public:
  BridgedNativeWidgetInitTest() {}

  // Prepares a new |window_| and |widget_| for a call to PerformInit().
  void CreateNewWidgetToInit(NSUInteger style_mask) {
    window_.reset(
        [[NSWindow alloc] initWithContentRect:ui::kWindowSizeDeterminedLater
                                    styleMask:style_mask
                                      backing:NSBackingStoreBuffered
                                        defer:NO]);
    [window_ setReleasedWhenClosed:NO];  // Owned by scoped_nsobject.
    widget_.reset(new Widget);
    native_widget_mac_ = new MockNativeWidgetMac(widget_.get());
    init_params_.native_widget = native_widget_mac_;
  }

  void PerformInit() {
    widget_->Init(init_params_);
    bridge()->Init(window_, init_params_);
  }

 protected:
  base::scoped_nsobject<NSWindow> window_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BridgedNativeWidgetInitTest);
};

// Test that BridgedNativeWidget remains sane if Init() is never called.
TEST_F(BridgedNativeWidgetInitTest, InitNotCalled) {
  EXPECT_FALSE(bridge()->ns_view());
  EXPECT_FALSE(bridge()->ns_window());
  bridge().reset();
}

// Tests the shadow type given in InitParams.
TEST_F(BridgedNativeWidgetInitTest, ShadowType) {
  // Verify Widget::InitParam defaults and arguments added from SetUp().
  EXPECT_EQ(Widget::InitParams::TYPE_WINDOW_FRAMELESS, init_params_.type);
  EXPECT_EQ(Widget::InitParams::OPAQUE_WINDOW, init_params_.opacity);
  EXPECT_EQ(Widget::InitParams::SHADOW_TYPE_DEFAULT, init_params_.shadow_type);

  CreateNewWidgetToInit(NSBorderlessWindowMask);
  EXPECT_FALSE([window_ hasShadow]);  // Default for NSBorderlessWindowMask.
  PerformInit();

  // Borderless is 0, so isn't really a mask. Check that nothing is set.
  EXPECT_EQ(NSBorderlessWindowMask, [window_ styleMask]);
  EXPECT_TRUE([window_ hasShadow]);  // SHADOW_TYPE_DEFAULT means a shadow.

  CreateNewWidgetToInit(NSBorderlessWindowMask);
  init_params_.shadow_type = Widget::InitParams::SHADOW_TYPE_NONE;
  PerformInit();
  EXPECT_FALSE([window_ hasShadow]);  // Preserves lack of shadow.

  // Default for Widget::InitParams::TYPE_WINDOW.
  NSUInteger kBorderedMask =
      NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask |
      NSResizableWindowMask | NSTexturedBackgroundWindowMask;
  CreateNewWidgetToInit(kBorderedMask);
  EXPECT_TRUE([window_ hasShadow]);  // Default for non-borderless.
  PerformInit();
  EXPECT_FALSE([window_ hasShadow]);  // SHADOW_TYPE_NONE removes shadow.

  init_params_.shadow_type = Widget::InitParams::SHADOW_TYPE_DEFAULT;
  CreateNewWidgetToInit(kBorderedMask);
  PerformInit();
  EXPECT_TRUE([window_ hasShadow]);  // Preserves shadow.

  window_.reset();
  widget_.reset();
}

// Ensure a nil NSTextInputContext is returned when the ui::TextInputClient is
// not editable, a password field, or unset.
TEST_F(BridgedNativeWidgetTest, InputContext) {
  const std::string test_string = "test_str";
  InstallTextField(test_string, ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_FALSE([ns_view_ inputContext]);
  InstallTextField(test_string, ui::TEXT_INPUT_TYPE_TEXT);
  EXPECT_TRUE([ns_view_ inputContext]);
  [ns_view_ setTextInputClient:nil];
  EXPECT_FALSE([ns_view_ inputContext]);
  InstallTextField(test_string, ui::TEXT_INPUT_TYPE_NONE);
  EXPECT_FALSE([ns_view_ inputContext]);
}

// Test getting complete string using text input protocol.
TEST_F(BridgedNativeWidgetTest, TextInput_GetCompleteString) {
  const std::string test_string = "foo bar baz";
  InstallTextField(test_string);

  NSRange range = NSMakeRange(0, test_string.size());
  NSRange actual_range;
  NSAttributedString* text =
      [ns_view_ attributedSubstringForProposedRange:range
                                        actualRange:&actual_range];
  EXPECT_EQ(test_string, SysNSStringToUTF8([text string]));
  EXPECT_EQ_RANGE(range, actual_range);
}

// Test getting middle substring using text input protocol.
TEST_F(BridgedNativeWidgetTest, TextInput_GetMiddleSubstring) {
  const std::string test_string = "foo bar baz";
  InstallTextField(test_string);

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
  const std::string test_string = "foo bar baz";
  InstallTextField(test_string);

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
  const std::string test_string = "foo bar baz";
  InstallTextField(test_string);

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
  const std::string test_string = "foo";
  InstallTextField(test_string);

  [ns_view_ insertText:SysUTF8ToNSString(test_string)
      replacementRange:EmptyRange()];
  gfx::Range range(0, test_string.size());
  base::string16 text;
  EXPECT_TRUE([ns_view_ textInputClient]->GetTextFromRange(range, &text));
  EXPECT_EQ(ASCIIToUTF16(test_string), text);
}

// Test replacing text using text input protocol.
TEST_F(BridgedNativeWidgetTest, TextInput_ReplaceText) {
  const std::string test_string = "foo bar";
  InstallTextField(test_string);

  [ns_view_ insertText:@"baz" replacementRange:NSMakeRange(4, 3)];
  EXPECT_EQ("foo baz", GetText());
}

// Test IME composition using text input protocol.
TEST_F(BridgedNativeWidgetTest, TextInput_Compose) {
  const std::string test_string = "foo ";
  InstallTextField(test_string);

  EXPECT_FALSE([ns_view_ hasMarkedText]);
  EXPECT_EQ_RANGE(EmptyRange(), [ns_view_ markedRange]);

  // Start composition.
  NSString* compositionText = @"bar";
  NSUInteger compositionLength = [compositionText length];
  [ns_view_ setMarkedText:compositionText
            selectedRange:NSMakeRange(0, 2)
         replacementRange:EmptyRange()];
  EXPECT_TRUE([ns_view_ hasMarkedText]);
  EXPECT_EQ_RANGE(NSMakeRange(test_string.size(), compositionLength),
                  [ns_view_ markedRange]);
  EXPECT_EQ_RANGE(NSMakeRange(test_string.size(), 2), [ns_view_ selectedRange]);

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

// Test firstRectForCharacterRange:actualRange for cases where query range is
// empty or outside composition range.
TEST_F(BridgedNativeWidgetTest, TextInput_FirstRectForCharacterRange_Caret) {
  InstallTextField("");
  ui::TextInputClient* client = [ns_view_ textInputClient];

  // No composition. Ensure bounds and range corresponding to the current caret
  // position are returned.
  // Initially selection range will be [0, 0].
  NSRange caret_range = NSMakeRange(0, 0);
  NSRange query_range = NSMakeRange(1, 1);
  NSRange actual_range;
  NSRect rect = [ns_view_ firstRectForCharacterRange:query_range
                                         actualRange:&actual_range];
  EXPECT_EQ(GetCaretBounds(client), gfx::ScreenRectFromNSRect(rect));
  EXPECT_EQ_RANGE(caret_range, actual_range);

  // Set composition with caret before second character ('e').
  const base::string16 test_string = base::ASCIIToUTF16("test_str");
  const size_t kTextLength = 8;
  SetCompositionText(client, test_string, 1, &caret_range);

  // Test bounds returned for empty ranges within composition range. As per
  // Apple's documentation for firstRectForCharacterRange:actualRange:, for an
  // empty query range, the returned rectangle should coincide with the
  // insertion point and have zero width. However in our implementation, if the
  // empty query range lies within the composition range, we return a zero width
  // rectangle corresponding to the query range location.

  // Test bounds returned for empty range before second character ('e') are same
  // as caret bounds when placed before second character.
  query_range = NSMakeRange(1, 0);
  rect = [ns_view_ firstRectForCharacterRange:query_range
                                  actualRange:&actual_range];
  EXPECT_EQ(GetCaretBoundsForPosition(client, test_string, 1, &caret_range),
            gfx::ScreenRectFromNSRect(rect));
  EXPECT_EQ_RANGE(query_range, actual_range);

  // Test bounds returned for empty range after the composition text are same as
  // caret bounds when placed after the composition text.
  query_range = NSMakeRange(kTextLength, 0);
  rect = [ns_view_ firstRectForCharacterRange:query_range
                                  actualRange:&actual_range];
  EXPECT_NE(GetCaretBoundsForPosition(client, test_string, 1, &caret_range),
            gfx::ScreenRectFromNSRect(rect));
  EXPECT_EQ(
      GetCaretBoundsForPosition(client, test_string, kTextLength, &caret_range),
      gfx::ScreenRectFromNSRect(rect));
  EXPECT_EQ_RANGE(query_range, actual_range);

  // Query outside composition range. Ensure bounds and range corresponding to
  // the current caret position are returned.
  query_range = NSMakeRange(kTextLength + 1, 0);
  rect = [ns_view_ firstRectForCharacterRange:query_range
                                  actualRange:&actual_range];
  EXPECT_EQ(GetCaretBounds(client), gfx::ScreenRectFromNSRect(rect));
  EXPECT_EQ_RANGE(caret_range, actual_range);

  // Make sure not crashing by passing null pointer instead of actualRange.
  rect = [ns_view_ firstRectForCharacterRange:query_range actualRange:nullptr];
}

// Test firstRectForCharacterRange:actualRange for non-empty query ranges within
// the composition range.
TEST_F(BridgedNativeWidgetTest, TextInput_FirstRectForCharacterRange) {
  InstallTextField("");
  ui::TextInputClient* client = [ns_view_ textInputClient];

  const base::string16 test_string = base::ASCIIToUTF16("test_str");
  const size_t kTextLength = 8;
  SetCompositionText(client, test_string, 1, nullptr);

  // Query bounds for the whole composition string.
  NSRange query_range = NSMakeRange(0, kTextLength);
  NSRange actual_range;
  NSRect rect = [ns_view_ firstRectForCharacterRange:query_range
                                         actualRange:&actual_range];
  EXPECT_EQ(GetExpectedBoundsForRange(client, test_string, query_range),
            gfx::ScreenRectFromNSRect(rect));
  EXPECT_EQ_RANGE(query_range, actual_range);

  // Query bounds for the substring "est_".
  query_range = NSMakeRange(1, 4);
  rect = [ns_view_ firstRectForCharacterRange:query_range
                                  actualRange:&actual_range];
  EXPECT_EQ(GetExpectedBoundsForRange(client, test_string, query_range),
            gfx::ScreenRectFromNSRect(rect));
  EXPECT_EQ_RANGE(query_range, actual_range);
}

typedef BridgedNativeWidgetTestBase BridgedNativeWidgetSimulateFullscreenTest;

// Simulate the notifications that AppKit would send out if a fullscreen
// operation begins, and then fails and must abort. This notification sequence
// was determined by posting delayed tasks to toggle fullscreen state and then
// mashing Ctrl+Left/Right to keep OSX in a transition between Spaces to cause
// the fullscreen transition to fail.
TEST_F(BridgedNativeWidgetSimulateFullscreenTest, FailToEnterAndExit) {
  base::scoped_nsobject<NSWindow> owned_window(
      [[BridgedNativeWidgetTestFullScreenWindow alloc]
          initWithContentRect:NSMakeRect(50, 50, 400, 300)
                    styleMask:NSBorderlessWindowMask
                      backing:NSBackingStoreBuffered
                        defer:NO]);
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
  // is no equivalent notification for failure).
  ViewsNSWindowDelegate* window_delegate =
      base::mac::ObjCCast<ViewsNSWindowDelegate>([window delegate]);
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
