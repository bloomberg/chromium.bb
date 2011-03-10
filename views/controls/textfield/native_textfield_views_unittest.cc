// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "views/controls/menu/menu_2.h"
#include "views/controls/textfield/native_textfield_views.h"
#include "views/controls/textfield/textfield.h"
#include "views/controls/textfield/textfield_controller.h"
#include "views/controls/textfield/textfield_views_model.h"
#include "views/events/event.h"
#include "views/focus/focus_manager.h"
#include "views/test/test_views_delegate.h"
#include "views/test/views_test_base.h"
#include "views/views_delegate.h"
#include "views/widget/widget.h"

namespace views {

// Convert to Wide so that the printed string will be readable when
// check fails.
#define EXPECT_STR_EQ(ascii, utf16) \
  EXPECT_EQ(ASCIIToWide(ascii), UTF16ToWide(utf16))
#define EXPECT_STR_NE(ascii, utf16) \
  EXPECT_NE(ASCIIToWide(ascii), UTF16ToWide(utf16))

// TODO(oshima): Move tests that are independent of TextfieldViews to
// textfield_unittests.cc once we move the test utility functions
// from chrome/browser/automation/ to app/test/.
class NativeTextfieldViewsTest : public ViewsTestBase,
                                 public TextfieldController {
 public:
  NativeTextfieldViewsTest()
      : widget_(NULL),
        textfield_(NULL),
        textfield_view_(NULL),
        model_(NULL) {
  }

  // ::testing::Test:
  virtual void SetUp() {
    NativeTextfieldViews::SetEnableTextfieldViews(true);
  }

  virtual void TearDown() {
    NativeTextfieldViews::SetEnableTextfieldViews(false);
    if (widget_)
      widget_->Close();
    ViewsTestBase::TearDown();
  }

  // TextfieldController:
  virtual void ContentsChanged(Textfield* sender,
                               const string16& new_contents){
    last_contents_ = new_contents;
  }

  virtual bool HandleKeyEvent(Textfield* sender,
                              const KeyEvent& key_event) {

    // TODO(oshima): figure out how to test the keystroke.
    return false;
  }

  void InitTextfield(Textfield::StyleFlags style) {
    InitTextfields(style, 1);
  }

  void InitTextfields(Textfield::StyleFlags style, int count) {
    ASSERT_FALSE(textfield_);
    textfield_ = new Textfield(style);
    textfield_->SetController(this);
    widget_ = Widget::CreatePopupWidget(
        Widget::NotTransparent,
        Widget::AcceptEvents,
        Widget::DeleteOnDestroy,
        Widget::DontMirrorOriginInRTL);
    widget_->Init(NULL, gfx::Rect(100, 100, 100, 100));
    View* container = new View();
    widget_->SetContentsView(container);
    container->AddChildView(textfield_);

    textfield_view_
        = static_cast<NativeTextfieldViews*>(textfield_->native_wrapper());
    textfield_->SetID(1);

    for (int i = 1; i < count; i++) {
      Textfield* textfield = new Textfield(style);
      container->AddChildView(textfield);
      textfield->SetID(i + 1);
    }

    DCHECK(textfield_view_);
    model_ = textfield_view_->model_.get();
  }

  views::Menu2* GetContextMenu() {
    textfield_view_->InitContextMenuIfRequired();
    return textfield_view_->context_menu_menu_.get();
  }

  NativeTextfieldViews::ClickState GetClickState() {
    return textfield_view_->click_state_;
  }

 protected:
  bool SendKeyEventToTextfieldViews(ui::KeyboardCode key_code,
                                    bool shift,
                                    bool control,
                                    bool capslock) {
    int flags = (shift ? ui::EF_SHIFT_DOWN : 0) |
        (control ? ui::EF_CONTROL_DOWN : 0) |
        (capslock ? ui::EF_CAPS_LOCK_DOWN : 0);
    KeyEvent event(ui::ET_KEY_PRESSED, key_code, flags);
    return textfield_->OnKeyPressed(event);
  }

  bool SendKeyEventToTextfieldViews(ui::KeyboardCode key_code,
                                    bool shift,
                                    bool control) {
    return SendKeyEventToTextfieldViews(key_code, shift, control, false);
  }

  bool SendKeyEventToTextfieldViews(ui::KeyboardCode key_code) {
    return SendKeyEventToTextfieldViews(key_code, false, false);
  }

  View* GetFocusedView() {
    return widget_->GetFocusManager()->GetFocusedView();
  }

  // We need widget to populate wrapper class.
  Widget* widget_;

  Textfield* textfield_;
  NativeTextfieldViews* textfield_view_;
  TextfieldViewsModel* model_;

  // The string from Controller::ContentsChanged callback.
  string16 last_contents_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeTextfieldViewsTest);
};

TEST_F(NativeTextfieldViewsTest, ModelChangesTeset) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  textfield_->SetText(ASCIIToUTF16("this is"));

  EXPECT_STR_EQ("this is", model_->text());
  EXPECT_STR_EQ("this is", last_contents_);
  last_contents_.clear();

  textfield_->AppendText(ASCIIToUTF16(" a test"));
  EXPECT_STR_EQ("this is a test", model_->text());
  EXPECT_STR_EQ("this is a test", last_contents_);
  last_contents_.clear();

  // Cases where the callback should not be called.
  textfield_->SetText(ASCIIToUTF16("this is a test"));
  EXPECT_STR_EQ("this is a test", model_->text());
  EXPECT_EQ(string16(), last_contents_);

  textfield_->AppendText(string16());
  EXPECT_STR_EQ("this is a test", model_->text());
  EXPECT_EQ(string16(), last_contents_);

  EXPECT_EQ(string16(), textfield_->GetSelectedText());
  textfield_->SelectAll();
  EXPECT_STR_EQ("this is a test", textfield_->GetSelectedText());
  EXPECT_EQ(string16(), last_contents_);
}

TEST_F(NativeTextfieldViewsTest, KeyTest) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  SendKeyEventToTextfieldViews(ui::VKEY_C, true, false);
  EXPECT_STR_EQ("C", textfield_->text());
  EXPECT_STR_EQ("C", last_contents_);
  last_contents_.clear();

  SendKeyEventToTextfieldViews(ui::VKEY_R, false, false);
  EXPECT_STR_EQ("Cr", textfield_->text());
  EXPECT_STR_EQ("Cr", last_contents_);

  textfield_->SetText(ASCIIToUTF16(""));
  SendKeyEventToTextfieldViews(ui::VKEY_C, true, false, true);
  SendKeyEventToTextfieldViews(ui::VKEY_C, false, false, true);
  SendKeyEventToTextfieldViews(ui::VKEY_1, false, false, true);
  SendKeyEventToTextfieldViews(ui::VKEY_1, true, false, true);
  SendKeyEventToTextfieldViews(ui::VKEY_1, true, false, false);
  EXPECT_STR_EQ("cC1!!", textfield_->text());
  EXPECT_STR_EQ("cC1!!", last_contents_);
}

TEST_F(NativeTextfieldViewsTest, ControlAndSelectTest) {
  // Insert a test string in a textfield.
  InitTextfield(Textfield::STYLE_DEFAULT);
  textfield_->SetText(ASCIIToUTF16("one two three"));
  SendKeyEventToTextfieldViews(ui::VKEY_RIGHT,
                              true /* shift */, false /* control */);
  SendKeyEventToTextfieldViews(ui::VKEY_RIGHT, true, false);
  SendKeyEventToTextfieldViews(ui::VKEY_RIGHT, true, false);

  EXPECT_STR_EQ("one", textfield_->GetSelectedText());

  // Test word select.
  SendKeyEventToTextfieldViews(ui::VKEY_RIGHT, true, true);
  EXPECT_STR_EQ("one two", textfield_->GetSelectedText());
  SendKeyEventToTextfieldViews(ui::VKEY_RIGHT, true, true);
  EXPECT_STR_EQ("one two three", textfield_->GetSelectedText());
  SendKeyEventToTextfieldViews(ui::VKEY_LEFT, true, true);
  EXPECT_STR_EQ("one two ", textfield_->GetSelectedText());
  SendKeyEventToTextfieldViews(ui::VKEY_LEFT, true, true);
  EXPECT_STR_EQ("one ", textfield_->GetSelectedText());

  // Replace the selected text.
  SendKeyEventToTextfieldViews(ui::VKEY_Z, true, false);
  SendKeyEventToTextfieldViews(ui::VKEY_E, true, false);
  SendKeyEventToTextfieldViews(ui::VKEY_R, true, false);
  SendKeyEventToTextfieldViews(ui::VKEY_O, true, false);
  SendKeyEventToTextfieldViews(ui::VKEY_SPACE, false, false);
  EXPECT_STR_EQ("ZERO two three", textfield_->text());

  SendKeyEventToTextfieldViews(ui::VKEY_END, true, false);
  EXPECT_STR_EQ("two three", textfield_->GetSelectedText());
  SendKeyEventToTextfieldViews(ui::VKEY_HOME, true, false);
  EXPECT_STR_EQ("ZERO ", textfield_->GetSelectedText());
}

TEST_F(NativeTextfieldViewsTest, InsertionDeletionTest) {
  // Insert a test string in a textfield.
  InitTextfield(Textfield::STYLE_DEFAULT);
  char test_str[] = "this is a test";
  for (size_t i = 0; i < sizeof(test_str); i++) {
    // This is ugly and should be replaced by a utility standard function.
    // See comment in NativeTextfieldViews::GetPrintableChar.
    char c = test_str[i];
    ui::KeyboardCode code =
        c == ' ' ? ui::VKEY_SPACE :
        static_cast<ui::KeyboardCode>(ui::VKEY_A + c - 'a');
    SendKeyEventToTextfieldViews(code);
  }
  EXPECT_STR_EQ(test_str, textfield_->text());

  // Move the cursor around.
  for (int i = 0; i < 6; i++) {
    SendKeyEventToTextfieldViews(ui::VKEY_LEFT);
  }
  SendKeyEventToTextfieldViews(ui::VKEY_RIGHT);

  // Delete using backspace and check resulting string.
  SendKeyEventToTextfieldViews(ui::VKEY_BACK);
  EXPECT_STR_EQ("this is  test", textfield_->text());

  // Delete using delete key and check resulting string.
  for (int i = 0; i < 5; i++) {
    SendKeyEventToTextfieldViews(ui::VKEY_DELETE);
  }
  EXPECT_STR_EQ("this is ", textfield_->text());

  // Select all and replace with "k".
  textfield_->SelectAll();
  SendKeyEventToTextfieldViews(ui::VKEY_K);
  EXPECT_STR_EQ("k", textfield_->text());

  // Delete the previous word from cursor.
  textfield_->SetText(ASCIIToUTF16("one two three four"));
  SendKeyEventToTextfieldViews(ui::VKEY_END);
  SendKeyEventToTextfieldViews(ui::VKEY_BACK, false, true, false);
  EXPECT_STR_EQ("one two three ", textfield_->text());

  // Delete upto the beginning of the buffer from cursor in chromeos, do nothing
  // in windows.
  SendKeyEventToTextfieldViews(ui::VKEY_LEFT, false, true, false);
  SendKeyEventToTextfieldViews(ui::VKEY_BACK, true, true, false);
#if defined(OS_WIN)
  EXPECT_STR_EQ("one two three ", textfield_->text());
#else
  EXPECT_STR_EQ("three ", textfield_->text());
#endif

  // Delete the next word from cursor.
  textfield_->SetText(ASCIIToUTF16("one two three four"));
  SendKeyEventToTextfieldViews(ui::VKEY_HOME);
  SendKeyEventToTextfieldViews(ui::VKEY_DELETE, false, true, false);
  EXPECT_STR_EQ(" two three four", textfield_->text());

  // Delete upto the end of the buffer from cursor in chromeos, do nothing
  // in windows.
  SendKeyEventToTextfieldViews(ui::VKEY_RIGHT, false, true, false);
  SendKeyEventToTextfieldViews(ui::VKEY_DELETE, true, true, false);
#if defined(OS_WIN)
  EXPECT_STR_EQ(" two three four", textfield_->text());
#else
  EXPECT_STR_EQ(" two", textfield_->text());
#endif
}

TEST_F(NativeTextfieldViewsTest, PasswordTest) {
  InitTextfield(Textfield::STYLE_PASSWORD);
  textfield_->SetText(ASCIIToUTF16("my password"));
  // Just to make sure the text() and callback returns
  // the actual text instead of "*".
  EXPECT_STR_EQ("my password", textfield_->text());
  EXPECT_STR_EQ("my password", last_contents_);
}

TEST_F(NativeTextfieldViewsTest, OnKeyPressReturnValueTest) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  EXPECT_TRUE(SendKeyEventToTextfieldViews(ui::VKEY_A));
  // F24, up/down key won't be handled.
  EXPECT_FALSE(SendKeyEventToTextfieldViews(ui::VKEY_F24));
  EXPECT_FALSE(SendKeyEventToTextfieldViews(ui::VKEY_UP));
  EXPECT_FALSE(SendKeyEventToTextfieldViews(ui::VKEY_DOWN));
}

TEST_F(NativeTextfieldViewsTest, CursorMovement) {
  InitTextfield(Textfield::STYLE_DEFAULT);

  // Test with trailing whitespace.
  textfield_->SetText(ASCIIToUTF16("one two hre "));

  // Send the cursor at the end.
  SendKeyEventToTextfieldViews(ui::VKEY_END);

  // Ctrl+Left should move the cursor just before the last word.
  SendKeyEventToTextfieldViews(ui::VKEY_LEFT, false, true);
  SendKeyEventToTextfieldViews(ui::VKEY_T);
  EXPECT_STR_EQ("one two thre ", textfield_->text());
  EXPECT_STR_EQ("one two thre ", last_contents_);

  // Ctrl+Right should move the cursor to the end of the last word.
  SendKeyEventToTextfieldViews(ui::VKEY_RIGHT, false, true);
  SendKeyEventToTextfieldViews(ui::VKEY_E);
  EXPECT_STR_EQ("one two three ", textfield_->text());
  EXPECT_STR_EQ("one two three ", last_contents_);

  // Ctrl+Right again should move the cursor to the end.
  SendKeyEventToTextfieldViews(ui::VKEY_RIGHT, false, true);
  SendKeyEventToTextfieldViews(ui::VKEY_BACK);
  EXPECT_STR_EQ("one two three", textfield_->text());
  EXPECT_STR_EQ("one two three", last_contents_);

  // Test with leading whitespace.
  textfield_->SetText(ASCIIToUTF16(" ne two"));

  // Send the cursor at the beginning.
  SendKeyEventToTextfieldViews(ui::VKEY_HOME);

  // Ctrl+Right, then Ctrl+Left should move the cursor to the beginning of the
  // first word.
  SendKeyEventToTextfieldViews(ui::VKEY_RIGHT, false, true);
  SendKeyEventToTextfieldViews(ui::VKEY_LEFT, false, true);
  SendKeyEventToTextfieldViews(ui::VKEY_O);
  EXPECT_STR_EQ(" one two", textfield_->text());
  EXPECT_STR_EQ(" one two", last_contents_);

  // Ctrl+Left to move the cursor to the beginning of the first word.
  SendKeyEventToTextfieldViews(ui::VKEY_LEFT, false, true);
  // Ctrl+Left again should move the cursor back to the very beginning.
  SendKeyEventToTextfieldViews(ui::VKEY_LEFT, false, true);
  SendKeyEventToTextfieldViews(ui::VKEY_DELETE);
  EXPECT_STR_EQ("one two", textfield_->text());
  EXPECT_STR_EQ("one two", last_contents_);
}

#if defined(OS_WIN)
// TODO(oshima): Windows' FocusManager::ClearNativeFocus() resets the
// focused view to NULL, which causes crash in this test.  Figure out
// why and fix this.
#define MAYBE_FocusTraversalTest DISABLED_FocusTraversalTest
#else
#define MAYBE_FocusTraversalTest FocusTraversalTest
#endif
TEST_F(NativeTextfieldViewsTest, MAYBE_FocusTraversalTest) {
  InitTextfields(Textfield::STYLE_DEFAULT, 3);
  textfield_->RequestFocus();

  EXPECT_EQ(1, GetFocusedView()->GetID());
  widget_->GetFocusManager()->AdvanceFocus(false);
  EXPECT_EQ(2, GetFocusedView()->GetID());
  widget_->GetFocusManager()->AdvanceFocus(false);
  EXPECT_EQ(3, GetFocusedView()->GetID());
  // Cycle back to the first textfield.
  widget_->GetFocusManager()->AdvanceFocus(false);
  EXPECT_EQ(1, GetFocusedView()->GetID());

  widget_->GetFocusManager()->AdvanceFocus(true);
  EXPECT_EQ(3, GetFocusedView()->GetID());
  widget_->GetFocusManager()->AdvanceFocus(true);
  EXPECT_EQ(2, GetFocusedView()->GetID());
  widget_->GetFocusManager()->AdvanceFocus(true);
  EXPECT_EQ(1, GetFocusedView()->GetID());
  // Cycle back to the last textfield.
  widget_->GetFocusManager()->AdvanceFocus(true);
  EXPECT_EQ(3, GetFocusedView()->GetID());

  // Request focus should still work.
  textfield_->RequestFocus();
  EXPECT_EQ(1, GetFocusedView()->GetID());
}

void VerifyTextfieldContextMenuContents(bool textfield_has_selection,
    ui::MenuModel* menu_model) {
  EXPECT_TRUE(menu_model->IsEnabledAt(4 /* Separator */));
  EXPECT_TRUE(menu_model->IsEnabledAt(5 /* SELECT ALL */));
  EXPECT_EQ(textfield_has_selection, menu_model->IsEnabledAt(0 /* CUT */));
  EXPECT_EQ(textfield_has_selection, menu_model->IsEnabledAt(1 /* COPY */));
  EXPECT_EQ(textfield_has_selection, menu_model->IsEnabledAt(3 /* DELETE */));
  string16 str;
  views::ViewsDelegate::views_delegate->GetClipboard()->
      ReadText(ui::Clipboard::BUFFER_STANDARD, &str);
  EXPECT_NE(str.empty(), menu_model->IsEnabledAt(2 /* PASTE */));
}

TEST_F(NativeTextfieldViewsTest, ContextMenuDisplayTest) {
  scoped_ptr<TestViewsDelegate> test_views_delegate(new TestViewsDelegate());
  AutoReset<views::ViewsDelegate*> auto_reset(
      &views::ViewsDelegate::views_delegate, test_views_delegate.get());
  views::ViewsDelegate::views_delegate = test_views_delegate.get();
  InitTextfield(Textfield::STYLE_DEFAULT);
  textfield_->SetText(ASCIIToUTF16("hello world"));
  EXPECT_TRUE(GetContextMenu());
  VerifyTextfieldContextMenuContents(false, GetContextMenu()->model());

  textfield_->SelectAll();
  VerifyTextfieldContextMenuContents(true, GetContextMenu()->model());
}

TEST_F(NativeTextfieldViewsTest, DoubleAndTripleClickTest) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  textfield_->SetText(ASCIIToUTF16("hello world"));
  MouseEvent me(ui::ET_MOUSE_PRESSED, 0, 0, ui::EF_LEFT_BUTTON_DOWN);
  EXPECT_EQ(NativeTextfieldViews::NONE, GetClickState());

  // Test for double click.
  textfield_view_->OnMousePressed(me);
  EXPECT_STR_EQ("", textfield_->GetSelectedText());
  EXPECT_EQ(NativeTextfieldViews::TRACKING_DOUBLE_CLICK, GetClickState());
  textfield_view_->OnMousePressed(me);
  EXPECT_STR_EQ("hello", textfield_->GetSelectedText());
  EXPECT_EQ(NativeTextfieldViews::TRACKING_TRIPLE_CLICK, GetClickState());

  // Test for triple click.
  textfield_view_->OnMousePressed(me);
  EXPECT_STR_EQ("hello world", textfield_->GetSelectedText());
  EXPECT_EQ(NativeTextfieldViews::NONE, GetClickState());

  // Another click should reset back to single click.
  textfield_view_->OnMousePressed(me);
  EXPECT_STR_EQ("", textfield_->GetSelectedText());
  EXPECT_EQ(NativeTextfieldViews::TRACKING_DOUBLE_CLICK, GetClickState());
}

TEST_F(NativeTextfieldViewsTest, ReadOnlyTest) {
  scoped_ptr<TestViewsDelegate> test_views_delegate(new TestViewsDelegate());
  AutoReset<views::ViewsDelegate*> auto_reset(
      &views::ViewsDelegate::views_delegate, test_views_delegate.get());

  InitTextfield(Textfield::STYLE_DEFAULT);
  textfield_->SetText(ASCIIToUTF16(" one two three "));
  textfield_->SetReadOnly(true);
  SendKeyEventToTextfieldViews(ui::VKEY_HOME);
  EXPECT_EQ(0U, textfield_->GetCursorPosition());

  SendKeyEventToTextfieldViews(ui::VKEY_END);
  EXPECT_EQ(15U, textfield_->GetCursorPosition());

  SendKeyEventToTextfieldViews(ui::VKEY_LEFT, false, false);
  EXPECT_EQ(14U, textfield_->GetCursorPosition());

  SendKeyEventToTextfieldViews(ui::VKEY_LEFT, false, true);
  EXPECT_EQ(9U, textfield_->GetCursorPosition());

  SendKeyEventToTextfieldViews(ui::VKEY_LEFT, true, true);
  EXPECT_EQ(5U, textfield_->GetCursorPosition());
  EXPECT_STR_EQ("two ", textfield_->GetSelectedText());

  textfield_->SelectAll();
  EXPECT_STR_EQ(" one two three ", textfield_->GetSelectedText());

  // CUT&PASTE does not work, but COPY works
  SendKeyEventToTextfieldViews(ui::VKEY_X, false, true);
  EXPECT_STR_EQ(" one two three ", textfield_->GetSelectedText());
  string16 str;
  views::ViewsDelegate::views_delegate->GetClipboard()->
      ReadText(ui::Clipboard::BUFFER_STANDARD, &str);
  EXPECT_STR_NE(" one two three ", str);

  SendKeyEventToTextfieldViews(ui::VKEY_C, false, true);
  views::ViewsDelegate::views_delegate->GetClipboard()->
      ReadText(ui::Clipboard::BUFFER_STANDARD, &str);
  EXPECT_STR_EQ(" one two three ", str);

  // SetText should work even in read only mode.
  textfield_->SetText(ASCIIToUTF16(" four five six "));
  EXPECT_STR_EQ(" four five six ", textfield_->text());

  // Paste shouldn't work.
  SendKeyEventToTextfieldViews(ui::VKEY_V, false, true);
  EXPECT_STR_EQ(" four five six ", textfield_->text());
  EXPECT_TRUE(textfield_->GetSelectedText().empty());

  textfield_->SelectAll();
  EXPECT_STR_EQ(" four five six ", textfield_->GetSelectedText());

  // Text field is unmodifiable and selection shouldn't change.
  SendKeyEventToTextfieldViews(ui::VKEY_DELETE);
  EXPECT_STR_EQ(" four five six ", textfield_->GetSelectedText());
  SendKeyEventToTextfieldViews(ui::VKEY_BACK);
  EXPECT_STR_EQ(" four five six ", textfield_->GetSelectedText());
  SendKeyEventToTextfieldViews(ui::VKEY_T);
  EXPECT_STR_EQ(" four five six ", textfield_->GetSelectedText());
}

}  // namespace views
