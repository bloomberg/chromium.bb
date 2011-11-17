// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/message_loop.h"
#include "base/pickle.h"
#include "base/utf_string_conversions.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/render_text.h"
#include "ui/views/ime/mock_input_method.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/test/views_test_base.h"
#include "views/controls/textfield/native_textfield_views.h"
#include "views/controls/textfield/textfield.h"
#include "views/controls/textfield/textfield_controller.h"
#include "views/controls/textfield/textfield_views_model.h"
#include "views/events/event.h"
#include "views/focus/focus_manager.h"
#include "views/views_delegate.h"
#include "views/widget/native_widget_private.h"
#include "views/widget/widget.h"

// Drag and drop for aura in linux hasn't been implemented yet.
// Bug http://crbug.com/97845
#if defined(USE_AURA) && defined(OS_LINUX)
#define MAYBE_DragAndDrop_InitiateDrag DISABLED_DragAndDrop_InitiateDrag
#define MAYBE_DragAndDrop_ToTheLeft DISABLED_DragAndDrop_ToTheLeft
#define MAYBE_DragAndDrop_ToTheRight DISABLED_DragAndDrop_ToTheRight
#define MAYBE_DragAndDrop_Canceled DISABLED_DragAndDrop_Canceled
#else
#define MAYBE_DragAndDrop_InitiateDrag DragAndDrop_InitiateDrag
#define MAYBE_DragAndDrop_ToTheLeft DragAndDrop_ToTheLeft
#define MAYBE_DragAndDrop_ToTheRight DragAndDrop_ToTheRight
#define MAYBE_DragAndDrop_Canceled DragAndDrop_Canceled
#endif  // OS_LINUX && USE_AURA

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

// A helper class for use with ui::TextInputClient::GetTextFromRange().
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

const char16 kHebrewLetterSamekh = 0x05E1;

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
// from chrome/browser/automation/ to ui/base/test/.
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
    ViewsTestBase::SetUp();
    Widget::SetPureViews(true);
  }

  virtual void TearDown() {
    if (widget_)
      widget_->Close();
    Widget::SetPureViews(false);
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
    widget_ = new Widget;
    Widget::InitParams params(Widget::InitParams::TYPE_POPUP);
    params.bounds = gfx::Rect(100, 100, 100, 100);
    widget_->Init(params);
    View* container = new View();
    widget_->SetContentsView(container);
    container->AddChildView(textfield_);

    textfield_view_ = static_cast<NativeTextfieldViews*>(
        textfield_->GetNativeWrapperForTesting());
    textfield_view_->SetBoundsRect(params.bounds);
    textfield_->set_id(1);

    for (int i = 1; i < count; i++) {
      Textfield* textfield = new Textfield(style);
      container->AddChildView(textfield);
      textfield->set_id(i + 1);
    }

    DCHECK(textfield_view_);
    model_ = textfield_view_->model_.get();
    model_->ClearEditHistory();

    input_method_ = new MockInputMethod();
    widget_->ReplaceInputMethod(input_method_);

    // Assumes the Widget is always focused.
    input_method_->OnFocus();

    textfield_->RequestFocus();
  }

  ui::MenuModel* GetContextMenuModel() {
    textfield_view_->UpdateContextMenu();
    return textfield_view_->context_menu_contents_.get();
  }

 protected:
  void SendKeyEvent(ui::KeyboardCode key_code,
                    bool shift,
                    bool control,
                    bool caps_lock) {
    int flags = (shift ? ui::EF_SHIFT_DOWN : 0) |
        (control ? ui::EF_CONTROL_DOWN : 0) |
        (caps_lock ? ui::EF_CAPS_LOCK_DOWN : 0);
    KeyEvent event(ui::ET_KEY_PRESSED, key_code, flags);
    input_method_->DispatchKeyEvent(event);
  }

  void SendKeyEvent(ui::KeyboardCode key_code, bool shift, bool control) {
    SendKeyEvent(key_code, shift, control, false);
  }

  void SendKeyEvent(ui::KeyboardCode key_code) {
    SendKeyEvent(key_code, false, false);
  }

  void SendKeyEvent(char16 ch) {
    if (ch < 0x80) {
      ui::KeyboardCode code =
          ch == ' ' ? ui::VKEY_SPACE :
          static_cast<ui::KeyboardCode>(ui::VKEY_A + ch - 'a');
      SendKeyEvent(code);
    } else {
      KeyEvent event(ui::ET_KEY_PRESSED, ui::VKEY_UNKNOWN, 0);
      event.set_character(ch);
      input_method_->DispatchKeyEvent(event);
    }
  }

  string16 GetClipboardText() const {
    string16 text;
    views::ViewsDelegate::views_delegate->GetClipboard()->
        ReadText(ui::Clipboard::BUFFER_STANDARD, &text);
    return text;
  }

  void SetClipboardText(const std::string& text) {
    ui::ScopedClipboardWriter clipboard_writer(
        views::ViewsDelegate::views_delegate->GetClipboard());
    clipboard_writer.WriteText(ASCIIToUTF16(text));
  }

  View* GetFocusedView() {
    return widget_->GetFocusManager()->GetFocusedView();
  }

  int GetCursorPositionX(int cursor_pos) {
    gfx::RenderText* render_text = textfield_view_->GetRenderText();
    return render_text->GetCursorBounds(
        gfx::SelectionModel(cursor_pos), false).x();
  }

  // Get the current cursor bounds.
  gfx::Rect GetCursorBounds() {
    gfx::RenderText* render_text = textfield_view_->GetRenderText();
    gfx::Rect bounds = render_text->GetUpdatedCursorBounds();
    return bounds;
  }

  // Get the cursor bounds of |sel|.
  gfx::Rect GetCursorBounds(const gfx::SelectionModel& sel) {
    gfx::RenderText* render_text = textfield_view_->GetRenderText();
    gfx::Rect bounds = render_text->GetCursorBounds(sel, true);
    return bounds;
  }

  gfx::Rect GetDisplayRect() {
    return textfield_view_->GetRenderText()->display_rect();
  }

  // Mouse click on the point whose x-axis is |bound|'s x plus |x_offset| and
  // y-axis is in the middle of |bound|'s vertical range.
  void MouseClick(const gfx::Rect bound, int x_offset) {
    int x = bound.x() +  x_offset;
    int y = bound.y() + bound.height() / 2;
    MouseEvent click(ui::ET_MOUSE_PRESSED, x, y, ui::EF_LEFT_BUTTON_DOWN);
    textfield_view_->OnMousePressed(click);
    MouseEvent release(ui::ET_MOUSE_RELEASED, x, y, ui::EF_LEFT_BUTTON_DOWN);
    textfield_view_->OnMouseReleased(release);
  }

  // This is to avoid double/triple click.
  void NonClientMouseClick() {
    MouseEvent click(ui::ET_MOUSE_PRESSED, 0, 0,
                     ui::EF_LEFT_BUTTON_DOWN | ui::EF_IS_NON_CLIENT);
    textfield_view_->OnMousePressed(click);
    MouseEvent release(ui::ET_MOUSE_RELEASED, 0, 0,
                       ui::EF_LEFT_BUTTON_DOWN | ui::EF_IS_NON_CLIENT);
    textfield_view_->OnMouseReleased(release);
  }

  // Wrap for visibility in test classes.
  ui::TextInputType GetTextInputType() {
    return textfield_view_->GetTextInputType();
  }

  void VerifyTextfieldContextMenuContents(bool textfield_has_selection,
                                          ui::MenuModel* menu_model) {
    EXPECT_TRUE(menu_model->IsEnabledAt(4 /* Separator */));
    EXPECT_TRUE(menu_model->IsEnabledAt(5 /* SELECT ALL */));
    EXPECT_EQ(textfield_has_selection, menu_model->IsEnabledAt(0 /* CUT */));
    EXPECT_EQ(textfield_has_selection, menu_model->IsEnabledAt(1 /* COPY */));
    EXPECT_EQ(textfield_has_selection, menu_model->IsEnabledAt(3 /* DELETE */));
    string16 str(GetClipboardText());
    EXPECT_NE(str.empty(), menu_model->IsEnabledAt(2 /* PASTE */));
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

  EXPECT_STR_EQ("this is", model_->GetText());
  EXPECT_STR_EQ("this is", textfield_->text());
  EXPECT_TRUE(last_contents_.empty());

  textfield_->AppendText(ASCIIToUTF16(" a test"));
  EXPECT_STR_EQ("this is a test", model_->GetText());
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

  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, GetTextInputType());

  last_contents_.clear();
  textfield_->SetText(ASCIIToUTF16("my password"));
  // Just to make sure the text() and callback returns
  // the actual text instead of "*".
  EXPECT_STR_EQ("my password", textfield_->text());
  EXPECT_TRUE(last_contents_.empty());
}

TEST_F(NativeTextfieldViewsTest, InputTypeSetsPassword) {
  InitTextfield(Textfield::STYLE_DEFAULT);

  // Defaults to TEXT
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, GetTextInputType());

  // Setting to passwords also sets password state of textfield.
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, GetTextInputType());
  EXPECT_TRUE(textfield_->IsPassword());
}

TEST_F(NativeTextfieldViewsTest, PasswordSetsInputType) {
  InitTextfield(Textfield::STYLE_DEFAULT);

  // Defaults to TEXT
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, GetTextInputType());

  textfield_->SetPassword(true);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, GetTextInputType());

  textfield_->SetPassword(false);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, GetTextInputType());
}

TEST_F(NativeTextfieldViewsTest, TextInputType) {
  InitTextfield(Textfield::STYLE_DEFAULT);

  // Defaults to TEXT
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, GetTextInputType());

  // And can be set.
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_URL);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_URL, GetTextInputType());

  // Readonly textfields have type NONE
  textfield_->SetReadOnly(true);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, GetTextInputType());

  textfield_->SetReadOnly(false);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_URL, GetTextInputType());

  // As do disabled textfields
  textfield_->SetEnabled(false);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_NONE, GetTextInputType());
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

  EXPECT_EQ(1, GetFocusedView()->id());
  widget_->GetFocusManager()->AdvanceFocus(false);
  EXPECT_EQ(2, GetFocusedView()->id());
  widget_->GetFocusManager()->AdvanceFocus(false);
  EXPECT_EQ(3, GetFocusedView()->id());
  // Cycle back to the first textfield.
  widget_->GetFocusManager()->AdvanceFocus(false);
  EXPECT_EQ(1, GetFocusedView()->id());

  widget_->GetFocusManager()->AdvanceFocus(true);
  EXPECT_EQ(3, GetFocusedView()->id());
  widget_->GetFocusManager()->AdvanceFocus(true);
  EXPECT_EQ(2, GetFocusedView()->id());
  widget_->GetFocusManager()->AdvanceFocus(true);
  EXPECT_EQ(1, GetFocusedView()->id());
  // Cycle back to the last textfield.
  widget_->GetFocusManager()->AdvanceFocus(true);
  EXPECT_EQ(3, GetFocusedView()->id());

  // Request focus should still work.
  textfield_->RequestFocus();
  EXPECT_EQ(1, GetFocusedView()->id());

  // Test if clicking on textfield view sets the focus to textfield_.
  widget_->GetFocusManager()->AdvanceFocus(true);
  EXPECT_EQ(3, GetFocusedView()->id());
  MouseEvent click(ui::ET_MOUSE_PRESSED, 0, 0, ui::EF_LEFT_BUTTON_DOWN);
  textfield_view_->OnMousePressed(click);
  EXPECT_EQ(1, GetFocusedView()->id());
}

TEST_F(NativeTextfieldViewsTest, ContextMenuDisplayTest) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  textfield_->SetText(ASCIIToUTF16("hello world"));
  EXPECT_TRUE(GetContextMenuModel());
  VerifyTextfieldContextMenuContents(false, GetContextMenuModel());

  textfield_->SelectAll();
  VerifyTextfieldContextMenuContents(true, GetContextMenuModel());
}

TEST_F(NativeTextfieldViewsTest, DoubleAndTripleClickTest) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  textfield_->SetText(ASCIIToUTF16("hello world"));
  MouseEvent click(ui::ET_MOUSE_PRESSED, 0, 0, ui::EF_LEFT_BUTTON_DOWN);
  MouseEvent release(ui::ET_MOUSE_RELEASED, 0, 0, ui::EF_LEFT_BUTTON_DOWN);
  MouseEvent double_click(ui::ET_MOUSE_PRESSED, 0, 0,
                          ui::EF_LEFT_BUTTON_DOWN | ui::EF_IS_DOUBLE_CLICK);

  // Test for double click.
  textfield_view_->OnMousePressed(click);
  textfield_view_->OnMouseReleased(release);
  EXPECT_TRUE(textfield_->GetSelectedText().empty());
  textfield_view_->OnMousePressed(double_click);
  textfield_view_->OnMouseReleased(release);
  EXPECT_STR_EQ("hello", textfield_->GetSelectedText());

  // Test for triple click.
  textfield_view_->OnMousePressed(click);
  textfield_view_->OnMouseReleased(release);
  EXPECT_STR_EQ("hello world", textfield_->GetSelectedText());

  // Another click should reset back to single click.
  textfield_view_->OnMousePressed(click);
  textfield_view_->OnMouseReleased(release);
  EXPECT_TRUE(textfield_->GetSelectedText().empty());
}

TEST_F(NativeTextfieldViewsTest, DragToSelect) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  textfield_->SetText(ASCIIToUTF16("hello world"));
  const int kStart = GetCursorPositionX(5);
  const int kEnd = 500;
  MouseEvent click_a(ui::ET_MOUSE_PRESSED, kStart, 0, ui::EF_LEFT_BUTTON_DOWN);
  MouseEvent click_b(ui::ET_MOUSE_PRESSED, kEnd, 0, ui::EF_LEFT_BUTTON_DOWN);
  MouseEvent drag_left(ui::ET_MOUSE_DRAGGED, 0, 0, ui::EF_LEFT_BUTTON_DOWN);
  MouseEvent drag_right(ui::ET_MOUSE_DRAGGED, kEnd, 0, ui::EF_LEFT_BUTTON_DOWN);
  MouseEvent release(ui::ET_MOUSE_RELEASED, kEnd, 0, ui::EF_LEFT_BUTTON_DOWN);
  textfield_view_->OnMousePressed(click_a);
  EXPECT_TRUE(textfield_->GetSelectedText().empty());
  // Check that dragging left selects the beginning of the string.
  textfield_view_->OnMouseDragged(drag_left);
  string16 text_left = textfield_->GetSelectedText();
  EXPECT_STR_EQ("hello", text_left);
  // Check that dragging right selects the rest of the string.
  textfield_view_->OnMouseDragged(drag_right);
  string16 text_right = textfield_->GetSelectedText();
  EXPECT_STR_EQ(" world", text_right);
  // Check that releasing in the same location does not alter the selection.
  textfield_view_->OnMouseReleased(release);
  EXPECT_EQ(text_right, textfield_->GetSelectedText());
  // Check that dragging from beyond the text length works too.
  textfield_view_->OnMousePressed(click_b);
  textfield_view_->OnMouseDragged(drag_left);
  textfield_view_->OnMouseReleased(release);
  EXPECT_EQ(textfield_->text(), textfield_->GetSelectedText());
}

#if defined(OS_WIN) || defined(TOOLKIT_USES_GTK)
TEST_F(NativeTextfieldViewsTest, DragAndDrop_AcceptDrop) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  textfield_->SetText(ASCIIToUTF16("hello world"));

  ui::OSExchangeData data;
  string16 string(ASCIIToUTF16("string "));
  data.SetString(string);
  int formats = 0;
  std::set<OSExchangeData::CustomFormat> custom_formats;

  // Ensure that disabled textfields do not accept drops.
  textfield_->SetEnabled(false);
  EXPECT_FALSE(textfield_view_->GetDropFormats(&formats, &custom_formats));
  EXPECT_EQ(0, formats);
  EXPECT_TRUE(custom_formats.empty());
  EXPECT_FALSE(textfield_view_->CanDrop(data));
  textfield_->SetEnabled(true);

  // Ensure that read-only textfields do not accept drops.
  textfield_->SetReadOnly(true);
  EXPECT_FALSE(textfield_view_->GetDropFormats(&formats, &custom_formats));
  EXPECT_EQ(0, formats);
  EXPECT_TRUE(custom_formats.empty());
  EXPECT_FALSE(textfield_view_->CanDrop(data));
  textfield_->SetReadOnly(false);

  // Ensure that enabled and editable textfields do accept drops.
  EXPECT_TRUE(textfield_view_->GetDropFormats(&formats, &custom_formats));
  EXPECT_EQ(ui::OSExchangeData::STRING, formats);
  EXPECT_TRUE(custom_formats.empty());
  EXPECT_TRUE(textfield_view_->CanDrop(data));
  DropTargetEvent drop(data, GetCursorPositionX(6), 0,
      ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_MOVE);
  EXPECT_EQ(ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_MOVE,
            textfield_view_->OnDragUpdated(drop));
  EXPECT_EQ(ui::DragDropTypes::DRAG_COPY, textfield_view_->OnPerformDrop(drop));
  EXPECT_STR_EQ("hello string world", textfield_->text());

  // Ensure that textfields do not accept non-OSExchangeData::STRING types.
  ui::OSExchangeData bad_data;
  bad_data.SetFilename(FilePath(FILE_PATH_LITERAL("x")));
#if defined(OS_WIN)
  bad_data.SetPickledData(CF_BITMAP, Pickle());
  bad_data.SetFileContents(FilePath(L"x"), "x");
  bad_data.SetHtml(string16(ASCIIToUTF16("x")), GURL("x.org"));
  ui::OSExchangeData::DownloadFileInfo download(FilePath(), NULL);
  bad_data.SetDownloadFileInfo(download);
#else
  // Skip OSExchangeDataProviderWin::SetURL, which also sets CF_TEXT / STRING.
  bad_data.SetURL(GURL("x.org"), string16(ASCIIToUTF16("x")));
  bad_data.SetPickledData(GDK_SELECTION_PRIMARY, Pickle());
#endif
  EXPECT_FALSE(textfield_view_->CanDrop(bad_data));
}
#endif

#if !defined(TOUCH_UI)
TEST_F(NativeTextfieldViewsTest, MAYBE_DragAndDrop_InitiateDrag) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  textfield_->SetText(ASCIIToUTF16("hello string world"));

  // Ensure the textfield will provide selected text for drag data.
  string16 string;
  ui::OSExchangeData data;
  const ui::Range kStringRange(6, 12);
  textfield_->SelectRange(kStringRange);
  const gfx::Point kStringPoint(GetCursorPositionX(9), 0);
  textfield_view_->WriteDragDataForView(NULL, kStringPoint, &data);
  EXPECT_TRUE(data.GetString(&string));
  EXPECT_EQ(textfield_->GetSelectedText(), string);

  // Ensure that disabled textfields do not support drag operations.
  textfield_->SetEnabled(false);
  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE,
            textfield_view_->GetDragOperationsForView(NULL, kStringPoint));
  textfield_->SetEnabled(true);
  // Ensure that textfields without selections do not support drag operations.
  textfield_->ClearSelection();
  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE,
            textfield_view_->GetDragOperationsForView(NULL, kStringPoint));
  textfield_->SelectRange(kStringRange);
  // Ensure that textfields only initiate drag operations inside the selection.
  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE,
            textfield_view_->GetDragOperationsForView(NULL, gfx::Point()));
  EXPECT_FALSE(textfield_view_->CanStartDragForView(NULL, gfx::Point(),
                                                    gfx::Point()));
  EXPECT_EQ(ui::DragDropTypes::DRAG_COPY,
            textfield_view_->GetDragOperationsForView(NULL, kStringPoint));
  EXPECT_TRUE(textfield_view_->CanStartDragForView(NULL, kStringPoint,
                                                   gfx::Point()));
  // Ensure that textfields support local moves.
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE | ui::DragDropTypes::DRAG_COPY,
      textfield_view_->GetDragOperationsForView(textfield_view_, kStringPoint));
}

TEST_F(NativeTextfieldViewsTest, MAYBE_DragAndDrop_ToTheRight) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  textfield_->SetText(ASCIIToUTF16("hello world"));

  string16 string;
  ui::OSExchangeData data;
  int formats = 0;
  int operations = 0;
  std::set<OSExchangeData::CustomFormat> custom_formats;

  // Start dragging "ello".
  textfield_->SelectRange(ui::Range(1, 5));
  MouseEvent click_a(ui::ET_MOUSE_PRESSED, GetCursorPositionX(3), 0,
                     ui::EF_LEFT_BUTTON_DOWN);
  textfield_view_->OnMousePressed(click_a);
  EXPECT_TRUE(textfield_view_->CanStartDragForView(textfield_view_,
                  click_a.location(), gfx::Point()));
  operations = textfield_view_->GetDragOperationsForView(textfield_view_,
                                                         click_a.location());
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE | ui::DragDropTypes::DRAG_COPY,
            operations);
  textfield_view_->WriteDragDataForView(NULL, click_a.location(), &data);
  EXPECT_TRUE(data.GetString(&string));
  EXPECT_EQ(textfield_->GetSelectedText(), string);
  EXPECT_TRUE(textfield_view_->GetDropFormats(&formats, &custom_formats));
  EXPECT_EQ(ui::OSExchangeData::STRING, formats);
  EXPECT_TRUE(custom_formats.empty());

  // Drop "ello" after "w".
  const gfx::Point kDropPoint(GetCursorPositionX(7), 0);
  EXPECT_TRUE(textfield_view_->CanDrop(data));
  DropTargetEvent drop_a(data, kDropPoint.x(), 0, operations);
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE,
            textfield_view_->OnDragUpdated(drop_a));
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE,
            textfield_view_->OnPerformDrop(drop_a));
  EXPECT_STR_EQ("h welloorld", textfield_->text());
  textfield_view_->OnDragDone();

  // Undo/Redo the drag&drop change.
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("hello world", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("hello world", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("h welloorld", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("h welloorld", textfield_->text());
}

TEST_F(NativeTextfieldViewsTest, MAYBE_DragAndDrop_ToTheLeft) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  textfield_->SetText(ASCIIToUTF16("hello world"));

  string16 string;
  ui::OSExchangeData data;
  int formats = 0;
  int operations = 0;
  std::set<OSExchangeData::CustomFormat> custom_formats;

  // Start dragging " worl".
  textfield_->SelectRange(ui::Range(5, 10));
  MouseEvent click_a(ui::ET_MOUSE_PRESSED, GetCursorPositionX(7), 0,
                     ui::EF_LEFT_BUTTON_DOWN);
  textfield_view_->OnMousePressed(click_a);
  EXPECT_TRUE(textfield_view_->CanStartDragForView(textfield_view_,
                  click_a.location(), gfx::Point()));
  operations = textfield_view_->GetDragOperationsForView(textfield_view_,
                                                         click_a.location());
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE | ui::DragDropTypes::DRAG_COPY,
            operations);
  textfield_view_->WriteDragDataForView(NULL, click_a.location(), &data);
  EXPECT_TRUE(data.GetString(&string));
  EXPECT_EQ(textfield_->GetSelectedText(), string);
  EXPECT_TRUE(textfield_view_->GetDropFormats(&formats, &custom_formats));
  EXPECT_EQ(ui::OSExchangeData::STRING, formats);
  EXPECT_TRUE(custom_formats.empty());

  // Drop " worl" after "h".
  EXPECT_TRUE(textfield_view_->CanDrop(data));
  DropTargetEvent drop_a(data, GetCursorPositionX(1), 0, operations);
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE,
            textfield_view_->OnDragUpdated(drop_a));
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE,
            textfield_view_->OnPerformDrop(drop_a));
  EXPECT_STR_EQ("h worlellod", textfield_->text());
  textfield_view_->OnDragDone();

  // Undo/Redo the drag&drop change.
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("hello world", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("hello world", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("h worlellod", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("h worlellod", textfield_->text());
}

TEST_F(NativeTextfieldViewsTest, MAYBE_DragAndDrop_Canceled) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  textfield_->SetText(ASCIIToUTF16("hello world"));

  // Start dragging "worl".
  textfield_->SelectRange(ui::Range(6, 10));
  MouseEvent click(ui::ET_MOUSE_PRESSED, GetCursorPositionX(8), 0,
                   ui::EF_LEFT_BUTTON_DOWN);
  textfield_view_->OnMousePressed(click);
  ui::OSExchangeData data;
  textfield_view_->WriteDragDataForView(NULL, click.location(), &data);
  EXPECT_TRUE(textfield_view_->CanDrop(data));
  // Drag the text over somewhere valid, outside the current selection.
  DropTargetEvent drop(data, GetCursorPositionX(2), 0,
                       ui::DragDropTypes::DRAG_MOVE);
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE, textfield_view_->OnDragUpdated(drop));
  // "Cancel" the drag, via move and release over the selection, and OnDragDone.
  MouseEvent drag(ui::ET_MOUSE_DRAGGED, GetCursorPositionX(9), 0,
                  ui::EF_LEFT_BUTTON_DOWN);
  MouseEvent release(ui::ET_MOUSE_RELEASED, GetCursorPositionX(9), 0,
                     ui::EF_LEFT_BUTTON_DOWN);
  textfield_view_->OnMouseDragged(drag);
  textfield_view_->OnMouseReleased(release);
  textfield_view_->OnDragDone();
  EXPECT_EQ(ASCIIToUTF16("hello world"), textfield_->text());
}
#endif

TEST_F(NativeTextfieldViewsTest, ReadOnlyTest) {
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
  SetClipboardText("Test");
  SendKeyEvent(ui::VKEY_X, false, true);
  EXPECT_STR_EQ(" one two three ", textfield_->GetSelectedText());
  string16 str(GetClipboardText());
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
  ui::TextInputClient* client = textfield_->GetTextInputClient();
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

TEST_F(NativeTextfieldViewsTest, UndoRedoTest) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  SendKeyEvent(ui::VKEY_A);
  EXPECT_STR_EQ("a", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("a", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("a", textfield_->text());

  // AppendText
  textfield_->AppendText(ASCIIToUTF16("b"));
  last_contents_.clear();  // AppendText doesn't call ContentsChanged.
  EXPECT_STR_EQ("ab", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("a", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("ab", textfield_->text());

  // SetText
  SendKeyEvent(ui::VKEY_C);
  // Undo'ing append moves the cursor to the end for now.
  // no-op SetText won't add new edit. See TextfieldViewsModel::SetText
  // description.
  EXPECT_STR_EQ("abc", textfield_->text());
  textfield_->SetText(ASCIIToUTF16("abc"));
  EXPECT_STR_EQ("abc", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("ab", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("abc", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("abc", textfield_->text());
  textfield_->SetText(ASCIIToUTF16("123"));
  textfield_->SetText(ASCIIToUTF16("123"));
  EXPECT_STR_EQ("123", textfield_->text());
  SendKeyEvent(ui::VKEY_END, false, false);
  SendKeyEvent(ui::VKEY_4, false, false);
  EXPECT_STR_EQ("1234", textfield_->text());
  last_contents_.clear();
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("123", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  // the insert edit "c" and set edit "123" are merged to single edit,
  // so text becomes "ab" after undo.
  EXPECT_STR_EQ("ab", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("a", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("ab", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("123", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("1234", textfield_->text());

  // Undoing to the same text shouldn't call ContentsChanged.
  SendKeyEvent(ui::VKEY_A, false, true);  // select all
  SendKeyEvent(ui::VKEY_A);
  EXPECT_STR_EQ("a", textfield_->text());
  SendKeyEvent(ui::VKEY_B);
  SendKeyEvent(ui::VKEY_C);
  EXPECT_STR_EQ("abc", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("1234", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("abc", textfield_->text());

  // Delete/Backspace
  SendKeyEvent(ui::VKEY_BACK);
  EXPECT_STR_EQ("ab", textfield_->text());
  SendKeyEvent(ui::VKEY_HOME);
  SendKeyEvent(ui::VKEY_DELETE);
  EXPECT_STR_EQ("b", textfield_->text());
  SendKeyEvent(ui::VKEY_A, false, true);
  SendKeyEvent(ui::VKEY_DELETE);
  EXPECT_STR_EQ("", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("b", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("ab", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("abc", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("ab", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("b", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("", textfield_->text());

  // Insert
  textfield_->SetText(ASCIIToUTF16("123"));
  SendKeyEvent(ui::VKEY_HOME);
  SendKeyEvent(ui::VKEY_INSERT);
  SendKeyEvent(ui::VKEY_A);
  EXPECT_STR_EQ("a23", textfield_->text());
  SendKeyEvent(ui::VKEY_B);
  EXPECT_STR_EQ("ab3", textfield_->text());
  SendKeyEvent(ui::VKEY_Z, false, true);
  EXPECT_STR_EQ("123", textfield_->text());
  SendKeyEvent(ui::VKEY_Y, false, true);
  EXPECT_STR_EQ("ab3", textfield_->text());
}

TEST_F(NativeTextfieldViewsTest, TextCursorDisplayTest) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  // LTR-RTL string in LTR context.
  SendKeyEvent('a');
  EXPECT_STR_EQ("a", textfield_->text());
  int x = GetCursorBounds().x();
  int prev_x = x;

  SendKeyEvent('b');
  EXPECT_STR_EQ("ab", textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_LT(prev_x, x);
  prev_x = x;

  SendKeyEvent(0x05E1);
  EXPECT_EQ(WideToUTF16(L"ab\x05E1"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_EQ(prev_x, x);

  SendKeyEvent(0x05E2);
  EXPECT_EQ(WideToUTF16(L"ab\x05E1\x5E2"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_EQ(prev_x, x);

  // Clear text.
  SendKeyEvent(ui::VKEY_A, false, true);
  SendKeyEvent('\n');

  // RTL-LTR string in LTR context.
  SendKeyEvent(0x05E1);
  EXPECT_EQ(WideToUTF16(L"\x05E1"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_EQ(GetDisplayRect().x(), x);
  prev_x = x;

  SendKeyEvent(0x05E2);
  EXPECT_EQ(WideToUTF16(L"\x05E1\x05E2"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_EQ(prev_x, x);

  SendKeyEvent('a');
  EXPECT_EQ(WideToUTF16(L"\x05E1\x5E2"L"a"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_LT(prev_x, x);
  prev_x = x;

  SendKeyEvent('b');
  EXPECT_EQ(WideToUTF16(L"\x05E1\x5E2"L"ab"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_LT(prev_x, x);
}

TEST_F(NativeTextfieldViewsTest, TextCursorDisplayInRTLTest) {
  std::string locale = l10n_util::GetApplicationLocale("");
  base::i18n::SetICUDefaultLocale("he");

  InitTextfield(Textfield::STYLE_DEFAULT);
  // LTR-RTL string in RTL context.
  SendKeyEvent('a');
  EXPECT_STR_EQ("a", textfield_->text());
  int x = GetCursorBounds().x();
  EXPECT_EQ(GetDisplayRect().right() - 1, x);
  int prev_x = x;

  SendKeyEvent('b');
  EXPECT_STR_EQ("ab", textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_EQ(prev_x, x);

  SendKeyEvent(0x05E1);
  EXPECT_EQ(WideToUTF16(L"ab\x05E1"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_GT(prev_x, x);
  prev_x = x;

  SendKeyEvent(0x05E2);
  EXPECT_EQ(WideToUTF16(L"ab\x05E1\x5E2"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_GT(prev_x, x);

  SendKeyEvent(ui::VKEY_A, false, true);
  SendKeyEvent('\n');

  // RTL-LTR string in RTL context.
  SendKeyEvent(0x05E1);
  EXPECT_EQ(WideToUTF16(L"\x05E1"), textfield_->text());
  x = GetCursorBounds().x();
  prev_x = x;

  SendKeyEvent(0x05E2);
  EXPECT_EQ(WideToUTF16(L"\x05E1\x05E2"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_GT(prev_x, x);
  prev_x = x;

  SendKeyEvent('a');
  EXPECT_EQ(WideToUTF16(L"\x05E1\x5E2"L"a"), textfield_->text());
  x = GetCursorBounds().x();
#if defined(OS_WIN)
  // In Windows, the text is always in LTR directionality even in RTL UI.
  // TODO(xji): it should change if we fix the directionality in Window's
  // NativeTextfieldViews
  EXPECT_LT(prev_x, x);
#else
  EXPECT_EQ(prev_x, x);
#endif
  prev_x = x;

  SendKeyEvent('b');
  EXPECT_EQ(WideToUTF16(L"\x05E1\x5E2"L"ab"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_EQ(prev_x, x);

  // Reset locale.
  base::i18n::SetICUDefaultLocale(locale);
}

TEST_F(NativeTextfieldViewsTest, HitInsideTextAreaTest) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  textfield_->SetText(WideToUTF16(L"ab\x05E1\x5E2"));
  std::vector<gfx::Rect> cursor_bounds;

  // Save each cursor bound.
  gfx::SelectionModel sel(0, 0, gfx::SelectionModel::LEADING);
  cursor_bounds.push_back(GetCursorBounds(sel));

  sel = gfx::SelectionModel(1, 0, gfx::SelectionModel::TRAILING);
  gfx::Rect bound = GetCursorBounds(sel);
  sel = gfx::SelectionModel(1, 1, gfx::SelectionModel::LEADING);
  EXPECT_EQ(bound, GetCursorBounds(sel));
  cursor_bounds.push_back(bound);

  sel = gfx::SelectionModel(2, 1, gfx::SelectionModel::TRAILING);
  bound = GetCursorBounds(sel);
  sel = gfx::SelectionModel(4, 3, gfx::SelectionModel::TRAILING);
  EXPECT_EQ(bound, GetCursorBounds(sel));
  cursor_bounds.push_back(bound);

  sel = gfx::SelectionModel(3, 2, gfx::SelectionModel::TRAILING);
  bound = GetCursorBounds(sel);
  sel = gfx::SelectionModel(3, 3, gfx::SelectionModel::LEADING);
  EXPECT_EQ(bound, GetCursorBounds(sel));
  cursor_bounds.push_back(bound);

  sel = gfx::SelectionModel(2, 2, gfx::SelectionModel::LEADING);
  bound = GetCursorBounds(sel);
  sel = gfx::SelectionModel(4, 2, gfx::SelectionModel::LEADING);
  EXPECT_EQ(bound, GetCursorBounds(sel));
  cursor_bounds.push_back(bound);

  // Expected cursor position when clicking left and right of each character.
  size_t cursor_pos_expected[] = {0, 1, 1, 2, 4, 3, 3, 2};

  int index = 0;
  for (int i = 0; i < static_cast<int>(cursor_bounds.size() - 1); ++i) {
    int half_width = (cursor_bounds[i + 1].x() - cursor_bounds[i].x()) / 2;
    MouseClick(cursor_bounds[i], half_width / 2);
    EXPECT_EQ(cursor_pos_expected[index++], textfield_->GetCursorPosition());

    // To avoid trigger double click. Not using sleep() since it takes longer
    // for the test to run if using sleep().
    NonClientMouseClick();

    MouseClick(cursor_bounds[i + 1], - (half_width / 2));
    EXPECT_EQ(cursor_pos_expected[index++], textfield_->GetCursorPosition());

    NonClientMouseClick();
  }
}

TEST_F(NativeTextfieldViewsTest, HitOutsideTextAreaTest) {
  InitTextfield(Textfield::STYLE_DEFAULT);

  // LTR-RTL string in LTR context.
  textfield_->SetText(WideToUTF16(L"ab\x05E1\x5E2"));

  SendKeyEvent(ui::VKEY_HOME);
  gfx::Rect bound = GetCursorBounds();
  MouseClick(bound, -10);
  EXPECT_EQ(bound, GetCursorBounds());

  SendKeyEvent(ui::VKEY_END);
  bound = GetCursorBounds();
  MouseClick(bound, 10);
  EXPECT_EQ(bound, GetCursorBounds());

  NonClientMouseClick();

  // RTL-LTR string in LTR context.
  textfield_->SetText(WideToUTF16(L"\x05E1\x5E2"L"ab"));

  SendKeyEvent(ui::VKEY_HOME);
  bound = GetCursorBounds();
#if defined(OS_WIN)
  MouseClick(bound, -10);
#else
  MouseClick(bound, 10);
#endif
  EXPECT_EQ(bound, GetCursorBounds());

  SendKeyEvent(ui::VKEY_END);
  bound = GetCursorBounds();
#if defined(OS_WIN)
  MouseClick(bound, 10);
#else
  MouseClick(bound, -10);
#endif
  EXPECT_EQ(bound, GetCursorBounds());
}

TEST_F(NativeTextfieldViewsTest, HitOutsideTextAreaInRTLTest) {
  std::string locale = l10n_util::GetApplicationLocale("");
  base::i18n::SetICUDefaultLocale("he");

  InitTextfield(Textfield::STYLE_DEFAULT);

  // RTL-LTR string in RTL context.
  textfield_->SetText(WideToUTF16(L"\x05E1\x5E2"L"ab"));
  SendKeyEvent(ui::VKEY_HOME);
  gfx::Rect bound = GetCursorBounds();
  MouseClick(bound, 10);
  EXPECT_EQ(bound, GetCursorBounds());

  SendKeyEvent(ui::VKEY_END);
  bound = GetCursorBounds();
  MouseClick(bound, -10);
  EXPECT_EQ(bound, GetCursorBounds());

  NonClientMouseClick();

  // LTR-RTL string in RTL context.
  textfield_->SetText(WideToUTF16(L"ab\x05E1\x5E2"));
  SendKeyEvent(ui::VKEY_HOME);
  bound = GetCursorBounds();
#if defined(OS_WIN)
  MouseClick(bound, 10);
#else
  MouseClick(bound, -10);
#endif
  EXPECT_EQ(bound, GetCursorBounds());

  SendKeyEvent(ui::VKEY_END);
  bound = GetCursorBounds();
#if defined(OS_WIN)
  MouseClick(bound, -10);
#else
  MouseClick(bound, 10);
#endif
  EXPECT_EQ(bound, GetCursorBounds());

  // Reset locale.
  base::i18n::SetICUDefaultLocale(locale);
}

// This verifies that |bound| is contained by |display|. |bound|'s right edge
// must be less than |diaplay|'s right edge.
void OverflowCursorBoundTestVerifier(const gfx::Rect& display,
                                     const gfx::Rect& bound) {
  EXPECT_LE(display.x(), bound.x());
  EXPECT_GT(display.right(), bound.right());
  EXPECT_LE(display.y(), bound.y());
  EXPECT_GE(display.bottom(), bound.bottom());
}

TEST_F(NativeTextfieldViewsTest, OverflowTest) {
  InitTextfield(Textfield::STYLE_DEFAULT);

  string16 str;
  for (int i = 0; i < 500; ++i)
    SendKeyEvent('a');
  SendKeyEvent(kHebrewLetterSamekh);
  gfx::Rect bound = GetCursorBounds();
  gfx::Rect display = GetDisplayRect();
  OverflowCursorBoundTestVerifier(display, bound);

  // Test mouse pointing.
  MouseClick(bound, -1);
  EXPECT_EQ(500U, textfield_->GetCursorPosition());

  // Clear text.
  SendKeyEvent(ui::VKEY_A, false, true);
  SendKeyEvent('\n');

  for (int i = 0; i < 500; ++i)
    SendKeyEvent(kHebrewLetterSamekh);
  SendKeyEvent('a');
  bound = GetCursorBounds();
  display = GetDisplayRect();
  OverflowCursorBoundTestVerifier(display, bound);

  MouseClick(bound, -1);
  EXPECT_EQ(501U, textfield_->GetCursorPosition());
}

TEST_F(NativeTextfieldViewsTest, OverflowInRTLTest) {
  std::string locale = l10n_util::GetApplicationLocale("");
  base::i18n::SetICUDefaultLocale("he");

  InitTextfield(Textfield::STYLE_DEFAULT);

  string16 str;
  for (int i = 0; i < 500; ++i)
    SendKeyEvent('a');
  SendKeyEvent(kHebrewLetterSamekh);
  gfx::Rect bound = GetCursorBounds();
  gfx::Rect display = GetDisplayRect();
  OverflowCursorBoundTestVerifier(display, bound);

  MouseClick(bound, 1);
  EXPECT_EQ(501U, textfield_->GetCursorPosition());

  // Clear text.
  SendKeyEvent(ui::VKEY_A, false, true);
  SendKeyEvent('\n');

  for (int i = 0; i < 500; ++i)
    SendKeyEvent(kHebrewLetterSamekh);
  SendKeyEvent('a');
  bound = GetCursorBounds();
  display = GetDisplayRect();
  OverflowCursorBoundTestVerifier(display, bound);


#if !defined(OS_WIN)
  // TODO(jennyz): NonClientMouseClick() does not work for os_win builds;
  // see crbug.com/104150. The mouse click in the next test will be confused
  // as a double click, which breaks the test. Disable the test until the
  // issue is fixed.
  NonClientMouseClick();

  MouseClick(bound, 1);
#if defined(OS_WIN)
  // In Windows, the text is always in LTR directionality even in RTL UI.
  // TODO(xji): it should change if we fix the directionality in Window's
  // NativeTextfieldViews
  EXPECT_EQ(0U, textfield_->GetCursorPosition());
#else
  EXPECT_EQ(500U, textfield_->GetCursorPosition());
#endif
#endif  // !defined(OS_WIN)

  // Reset locale.
  base::i18n::SetICUDefaultLocale(locale);
}

}  // namespace views
