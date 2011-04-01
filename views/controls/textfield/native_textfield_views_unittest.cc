// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
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
#include "views/ime/mock_input_method.h"
#include "views/ime/text_input_client.h"
#include "views/test/test_views_delegate.h"
#include "views/test/views_test_base.h"
#include "views/views_delegate.h"
#include "views/widget/native_widget.h"
#include "views/widget/widget.h"

namespace {

// A wrapper of Textfield to intercept the result of OnKeyPressed() and
// OnKeyReleased() methods.
class TestTextfield : public views::Textfield {
 public:
  TestTextfield()
      : key_handled_(false),
        key_received_(false) {
  }

  explicit TestTextfield(StyleFlags style)
      : Textfield(style),
        key_handled_(false),
        key_received_(false) {
  }

  virtual bool OnKeyPressed(const views::KeyEvent& e) OVERRIDE {
    key_received_ = true;
    key_handled_ = views::Textfield::OnKeyPressed(e);
    return key_handled_;
  }

  virtual bool OnKeyReleased(const views::KeyEvent& e) OVERRIDE {
    key_received_ = true;
    key_handled_ = views::Textfield::OnKeyReleased(e);
    return key_handled_;
  }

  bool key_handled() const { return key_handled_; }
  bool key_received() const { return key_received_; }

  void clear() {
    key_received_ = key_handled_ = false;
  }

 private:
  bool key_handled_;
  bool key_received_;

  DISALLOW_COPY_AND_ASSIGN(TestTextfield);
};

// A helper class for use with TextInputClient::GetTextFromRange().
class GetTextHelper {
 public:
  GetTextHelper() {
  }

  void set_text(const string16& text) { text_ = text; }
  const string16& text() const { return text_; }

 private:
  string16 text_;

  DISALLOW_COPY_AND_ASSIGN(GetTextHelper);
};

}  // namespace

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
        model_(NULL),
        input_method_(NULL),
        on_before_user_action_(0),
        on_after_user_action_(0) {
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
                               const string16& new_contents) {
    ASSERT_NE(last_contents_, new_contents);
    last_contents_ = new_contents;
  }

  virtual bool HandleKeyEvent(Textfield* sender,
                              const KeyEvent& key_event) {

    // TODO(oshima): figure out how to test the keystroke.
    return false;
  }

  virtual void OnBeforeUserAction(Textfield* sender) {
    ++on_before_user_action_;
  }

  virtual void OnAfterUserAction(Textfield* sender) {
    ++on_after_user_action_;
  }

  void InitTextfield(Textfield::StyleFlags style) {
    InitTextfields(style, 1);
  }

  void InitTextfields(Textfield::StyleFlags style, int count) {
    ASSERT_FALSE(textfield_);
    textfield_ = new TestTextfield(style);
    textfield_->SetController(this);
    Widget::CreateParams params(Widget::CreateParams::TYPE_POPUP);
    params.mirror_origin_in_rtl = false;
    widget_ = Widget::CreateWidget(params);
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

    input_method_ = new MockInputMethod();
    widget_->native_widget()->ReplaceInputMethod(input_method_);

    // Assumes the Widget is always focused.
    input_method_->OnFocus();

    textfield_->RequestFocus();
  }

  views::Menu2* GetContextMenu() {
    textfield_view_->InitContextMenuIfRequired();
    return textfield_view_->context_menu_menu_.get();
  }

  NativeTextfieldViews::ClickState GetClickState() {
    return textfield_view_->click_state_;
  }

 protected:
  void SendKeyEvent(ui::KeyboardCode key_code,
                    bool shift,
                    bool control,
                    bool capslock) {
    int flags = (shift ? ui::EF_SHIFT_DOWN : 0) |
        (control ? ui::EF_CONTROL_DOWN : 0) |
        (capslock ? ui::EF_CAPS_LOCK_DOWN : 0);
    KeyEvent event(ui::ET_KEY_PRESSED, key_code, flags);
    input_method_->DispatchKeyEvent(event);
  }

  void SendKeyEvent(ui::KeyboardCode key_code, bool shift, bool control) {
    SendKeyEvent(key_code, shift, control, false);
  }

  void SendKeyEvent(ui::KeyboardCode key_code) {
    SendKeyEvent(key_code, false, false);
  }

  View* GetFocusedView() {
    return widget_->GetFocusManager()->GetFocusedView();
  }

  // We need widget to populate wrapper class.
  Widget* widget_;

  TestTextfield* textfield_;
  NativeTextfieldViews* textfield_view_;
  TextfieldViewsModel* model_;

  // The string from Controller::ContentsChanged callback.
  string16 last_contents_;

  // For testing input method related behaviors.
  MockInputMethod* input_method_;

  // Indicates how many times OnBeforeUserAction() is called.
  int on_before_user_action_;

  // Indicates how many times OnAfterUserAction() is called.
  int on_after_user_action_;

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeTextfieldViewsTest);
};

TEST_F(NativeTextfieldViewsTest, ModelChangesTest) {
  InitTextfield(Textfield::STYLE_DEFAULT);

  // TextfieldController::ContentsChanged() shouldn't be called when changing
  // text programmatically.
  last_contents_.clear();
  textfield_->SetText(ASCIIToUTF16("this is"));

  EXPECT_STR_EQ("this is", model_->text());
  EXPECT_STR_EQ("this is", textfield_->text());
  EXPECT_TRUE(last_contents_.empty());

  textfield_->AppendText(ASCIIToUTF16(" a test"));
  EXPECT_STR_EQ("this is a test", model_->text());
  EXPECT_STR_EQ("this is a test", textfield_->text());
  EXPECT_TRUE(last_contents_.empty());

  EXPECT_EQ(string16(), textfield_->GetSelectedText());
  textfield_->SelectAll();
  EXPECT_STR_EQ("this is a test", textfield_->GetSelectedText());
  EXPECT_TRUE(last_contents_.empty());
}

TEST_F(NativeTextfieldViewsTest, KeyTest) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  SendKeyEvent(ui::VKEY_C, true, false);
  EXPECT_STR_EQ("C", textfield_->text());
  EXPECT_STR_EQ("C", last_contents_);
  last_contents_.clear();

  SendKeyEvent(ui::VKEY_R, false, false);
  EXPECT_STR_EQ("Cr", textfield_->text());
  EXPECT_STR_EQ("Cr", last_contents_);

  textfield_->SetText(ASCIIToUTF16(""));
  SendKeyEvent(ui::VKEY_C, true, false, true);
  SendKeyEvent(ui::VKEY_C, false, false, true);
  SendKeyEvent(ui::VKEY_1, false, false, true);
  SendKeyEvent(ui::VKEY_1, true, false, true);
  SendKeyEvent(ui::VKEY_1, true, false, false);
  EXPECT_STR_EQ("cC1!!", textfield_->text());
  EXPECT_STR_EQ("cC1!!", last_contents_);
}

TEST_F(NativeTextfieldViewsTest, ControlAndSelectTest) {
  // Insert a test string in a textfield.
  InitTextfield(Textfield::STYLE_DEFAULT);
  textfield_->SetText(ASCIIToUTF16("one two three"));
  SendKeyEvent(ui::VKEY_RIGHT,
                              true /* shift */, false /* control */);
  SendKeyEvent(ui::VKEY_RIGHT, true, false);
  SendKeyEvent(ui::VKEY_RIGHT, true, false);

  EXPECT_STR_EQ("one", textfield_->GetSelectedText());

  // Test word select.
  SendKeyEvent(ui::VKEY_RIGHT, true, true);
  EXPECT_STR_EQ("one two", textfield_->GetSelectedText());
  SendKeyEvent(ui::VKEY_RIGHT, true, true);
  EXPECT_STR_EQ("one two three", textfield_->GetSelectedText());
  SendKeyEvent(ui::VKEY_LEFT, true, true);
  EXPECT_STR_EQ("one two ", textfield_->GetSelectedText());
  SendKeyEvent(ui::VKEY_LEFT, true, true);
  EXPECT_STR_EQ("one ", textfield_->GetSelectedText());

  // Replace the selected text.
  SendKeyEvent(ui::VKEY_Z, true, false);
  SendKeyEvent(ui::VKEY_E, true, false);
  SendKeyEvent(ui::VKEY_R, true, false);
  SendKeyEvent(ui::VKEY_O, true, false);
  SendKeyEvent(ui::VKEY_SPACE, false, false);
  EXPECT_STR_EQ("ZERO two three", textfield_->text());

  SendKeyEvent(ui::VKEY_END, true, false);
  EXPECT_STR_EQ("two three", textfield_->GetSelectedText());
  SendKeyEvent(ui::VKEY_HOME, true, false);
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
    SendKeyEvent(code);
  }
  EXPECT_STR_EQ(test_str, textfield_->text());

  // Move the cursor around.
  for (int i = 0; i < 6; i++) {
    SendKeyEvent(ui::VKEY_LEFT);
  }
  SendKeyEvent(ui::VKEY_RIGHT);

  // Delete using backspace and check resulting string.
  SendKeyEvent(ui::VKEY_BACK);
  EXPECT_STR_EQ("this is  test", textfield_->text());

  // Delete using delete key and check resulting string.
  for (int i = 0; i < 5; i++) {
    SendKeyEvent(ui::VKEY_DELETE);
  }
  EXPECT_STR_EQ("this is ", textfield_->text());

  // Select all and replace with "k".
  textfield_->SelectAll();
  SendKeyEvent(ui::VKEY_K);
  EXPECT_STR_EQ("k", textfield_->text());

  // Delete the previous word from cursor.
  textfield_->SetText(ASCIIToUTF16("one two three four"));
  SendKeyEvent(ui::VKEY_END);
  SendKeyEvent(ui::VKEY_BACK, false, true, false);
  EXPECT_STR_EQ("one two three ", textfield_->text());

  // Delete upto the beginning of the buffer from cursor in chromeos, do nothing
  // in windows.
  SendKeyEvent(ui::VKEY_LEFT, false, true, false);
  SendKeyEvent(ui::VKEY_BACK, true, true, false);
#if defined(OS_WIN)
  EXPECT_STR_EQ("one two three ", textfield_->text());
#else
  EXPECT_STR_EQ("three ", textfield_->text());
#endif

  // Delete the next word from cursor.
  textfield_->SetText(ASCIIToUTF16("one two three four"));
  SendKeyEvent(ui::VKEY_HOME);
  SendKeyEvent(ui::VKEY_DELETE, false, true, false);
  EXPECT_STR_EQ(" two three four", textfield_->text());

  // Delete upto the end of the buffer from cursor in chromeos, do nothing
  // in windows.
  SendKeyEvent(ui::VKEY_RIGHT, false, true, false);
  SendKeyEvent(ui::VKEY_DELETE, true, true, false);
#if defined(OS_WIN)
  EXPECT_STR_EQ(" two three four", textfield_->text());
#else
  EXPECT_STR_EQ(" two", textfield_->text());
#endif
}

TEST_F(NativeTextfieldViewsTest, PasswordTest) {
  InitTextfield(Textfield::STYLE_PASSWORD);

  last_contents_.clear();
  textfield_->SetText(ASCIIToUTF16("my password"));
  // Just to make sure the text() and callback returns
  // the actual text instead of "*".
  EXPECT_STR_EQ("my password", textfield_->text());
  EXPECT_TRUE(last_contents_.empty());
}

TEST_F(NativeTextfieldViewsTest, OnKeyPressReturnValueTest) {
  InitTextfield(Textfield::STYLE_DEFAULT);

  // Character keys will be handled by input method.
  SendKeyEvent(ui::VKEY_A);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_FALSE(textfield_->key_handled());
  textfield_->clear();

  // Home will be handled.
  SendKeyEvent(ui::VKEY_HOME);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_TRUE(textfield_->key_handled());
  textfield_->clear();

  // F24, up/down key won't be handled.
  SendKeyEvent(ui::VKEY_F24);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_FALSE(textfield_->key_handled());
  textfield_->clear();

  SendKeyEvent(ui::VKEY_UP);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_FALSE(textfield_->key_handled());
  textfield_->clear();

  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_FALSE(textfield_->key_handled());
}

TEST_F(NativeTextfieldViewsTest, CursorMovement) {
  InitTextfield(Textfield::STYLE_DEFAULT);

  // Test with trailing whitespace.
  textfield_->SetText(ASCIIToUTF16("one two hre "));

  // Send the cursor at the end.
  SendKeyEvent(ui::VKEY_END);

  // Ctrl+Left should move the cursor just before the last word.
  SendKeyEvent(ui::VKEY_LEFT, false, true);
  SendKeyEvent(ui::VKEY_T);
  EXPECT_STR_EQ("one two thre ", textfield_->text());
  EXPECT_STR_EQ("one two thre ", last_contents_);

  // Ctrl+Right should move the cursor to the end of the last word.
  SendKeyEvent(ui::VKEY_RIGHT, false, true);
  SendKeyEvent(ui::VKEY_E);
  EXPECT_STR_EQ("one two three ", textfield_->text());
  EXPECT_STR_EQ("one two three ", last_contents_);

  // Ctrl+Right again should move the cursor to the end.
  SendKeyEvent(ui::VKEY_RIGHT, false, true);
  SendKeyEvent(ui::VKEY_BACK);
  EXPECT_STR_EQ("one two three", textfield_->text());
  EXPECT_STR_EQ("one two three", last_contents_);

  // Test with leading whitespace.
  textfield_->SetText(ASCIIToUTF16(" ne two"));

  // Send the cursor at the beginning.
  SendKeyEvent(ui::VKEY_HOME);

  // Ctrl+Right, then Ctrl+Left should move the cursor to the beginning of the
  // first word.
  SendKeyEvent(ui::VKEY_RIGHT, false, true);
  SendKeyEvent(ui::VKEY_LEFT, false, true);
  SendKeyEvent(ui::VKEY_O);
  EXPECT_STR_EQ(" one two", textfield_->text());
  EXPECT_STR_EQ(" one two", last_contents_);

  // Ctrl+Left to move the cursor to the beginning of the first word.
  SendKeyEvent(ui::VKEY_LEFT, false, true);
  // Ctrl+Left again should move the cursor back to the very beginning.
  SendKeyEvent(ui::VKEY_LEFT, false, true);
  SendKeyEvent(ui::VKEY_DELETE);
  EXPECT_STR_EQ("one two", textfield_->text());
  EXPECT_STR_EQ("one two", last_contents_);
}

TEST_F(NativeTextfieldViewsTest, FocusTraversalTest) {
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
  SendKeyEvent(ui::VKEY_HOME);
  EXPECT_EQ(0U, textfield_->GetCursorPosition());

  SendKeyEvent(ui::VKEY_END);
  EXPECT_EQ(15U, textfield_->GetCursorPosition());

  SendKeyEvent(ui::VKEY_LEFT, false, false);
  EXPECT_EQ(14U, textfield_->GetCursorPosition());

  SendKeyEvent(ui::VKEY_LEFT, false, true);
  EXPECT_EQ(9U, textfield_->GetCursorPosition());

  SendKeyEvent(ui::VKEY_LEFT, true, true);
  EXPECT_EQ(5U, textfield_->GetCursorPosition());
  EXPECT_STR_EQ("two ", textfield_->GetSelectedText());

  textfield_->SelectAll();
  EXPECT_STR_EQ(" one two three ", textfield_->GetSelectedText());

  // CUT&PASTE does not work, but COPY works
  SendKeyEvent(ui::VKEY_X, false, true);
  EXPECT_STR_EQ(" one two three ", textfield_->GetSelectedText());
  string16 str;
  views::ViewsDelegate::views_delegate->GetClipboard()->
      ReadText(ui::Clipboard::BUFFER_STANDARD, &str);
  EXPECT_STR_NE(" one two three ", str);

  SendKeyEvent(ui::VKEY_C, false, true);
  views::ViewsDelegate::views_delegate->GetClipboard()->
      ReadText(ui::Clipboard::BUFFER_STANDARD, &str);
  EXPECT_STR_EQ(" one two three ", str);

  // SetText should work even in read only mode.
  textfield_->SetText(ASCIIToUTF16(" four five six "));
  EXPECT_STR_EQ(" four five six ", textfield_->text());

  // Paste shouldn't work.
  SendKeyEvent(ui::VKEY_V, false, true);
  EXPECT_STR_EQ(" four five six ", textfield_->text());
  EXPECT_TRUE(textfield_->GetSelectedText().empty());

  textfield_->SelectAll();
  EXPECT_STR_EQ(" four five six ", textfield_->GetSelectedText());

  // Text field is unmodifiable and selection shouldn't change.
  SendKeyEvent(ui::VKEY_DELETE);
  EXPECT_STR_EQ(" four five six ", textfield_->GetSelectedText());
  SendKeyEvent(ui::VKEY_BACK);
  EXPECT_STR_EQ(" four five six ", textfield_->GetSelectedText());
  SendKeyEvent(ui::VKEY_T);
  EXPECT_STR_EQ(" four five six ", textfield_->GetSelectedText());
}

TEST_F(NativeTextfieldViewsTest, TextInputClientTest) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  TextInputClient* client = textfield_->GetTextInputClient();
  EXPECT_TRUE(client);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, client->GetTextInputType());

  textfield_->SetText(ASCIIToUTF16("0123456789"));
  ui::Range range;
  EXPECT_TRUE(client->GetTextRange(&range));
  EXPECT_EQ(0U, range.start());
  EXPECT_EQ(10U, range.end());

  EXPECT_TRUE(client->SetSelectionRange(ui::Range(1, 4)));
  EXPECT_TRUE(client->GetSelectionRange(&range));
  EXPECT_EQ(ui::Range(1,4), range);

  // This code can't be compiled because of a bug in base::Callback.
#if 0
  GetTextHelper helper;
  base::Callback<void(string16)> callback =
      base::Bind(&GetTextHelper::set_text, base::Unretained(&helper));

  EXPECT_TRUE(client->GetTextFromRange(range, callback));
  EXPECT_STR_EQ("123", helper.text());
#endif

  EXPECT_TRUE(client->DeleteRange(range));
  EXPECT_STR_EQ("0456789", textfield_->text());

  ui::CompositionText composition;
  composition.text = UTF8ToUTF16("321");
  // Set composition through input method.
  input_method_->Clear();
  input_method_->SetCompositionTextForNextKey(composition);
  textfield_->clear();

  on_before_user_action_ = on_after_user_action_ = 0;
  SendKeyEvent(ui::VKEY_A);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_FALSE(textfield_->key_handled());
  EXPECT_TRUE(client->HasCompositionText());
  EXPECT_TRUE(client->GetCompositionTextRange(&range));
  EXPECT_STR_EQ("0321456789", textfield_->text());
  EXPECT_EQ(ui::Range(1,4), range);
  EXPECT_EQ(2, on_before_user_action_);
  EXPECT_EQ(2, on_after_user_action_);

  input_method_->SetResultTextForNextKey(UTF8ToUTF16("123"));
  on_before_user_action_ = on_after_user_action_ = 0;
  textfield_->clear();
  SendKeyEvent(ui::VKEY_A);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_FALSE(textfield_->key_handled());
  EXPECT_FALSE(client->HasCompositionText());
  EXPECT_FALSE(input_method_->cancel_composition_called());
  EXPECT_STR_EQ("0123456789", textfield_->text());
  EXPECT_EQ(2, on_before_user_action_);
  EXPECT_EQ(2, on_after_user_action_);

  input_method_->Clear();
  input_method_->SetCompositionTextForNextKey(composition);
  textfield_->clear();
  SendKeyEvent(ui::VKEY_A);
  EXPECT_TRUE(client->HasCompositionText());
  EXPECT_STR_EQ("0123321456789", textfield_->text());

  on_before_user_action_ = on_after_user_action_ = 0;
  textfield_->clear();
  SendKeyEvent(ui::VKEY_RIGHT);
  EXPECT_FALSE(client->HasCompositionText());
  EXPECT_TRUE(input_method_->cancel_composition_called());
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_TRUE(textfield_->key_handled());
  EXPECT_STR_EQ("0123321456789", textfield_->text());
  EXPECT_EQ(8U, textfield_->GetCursorPosition());
  EXPECT_EQ(1, on_before_user_action_);
  EXPECT_EQ(1, on_after_user_action_);

  input_method_->Clear();
  textfield_->SetReadOnly(true);
  EXPECT_TRUE(input_method_->text_input_type_changed());
  EXPECT_FALSE(textfield_->GetTextInputClient());

  textfield_->SetReadOnly(false);
  input_method_->Clear();
  textfield_->SetPassword(true);
  EXPECT_TRUE(input_method_->text_input_type_changed());
  EXPECT_TRUE(textfield_->GetTextInputClient());
}


}  // namespace views
