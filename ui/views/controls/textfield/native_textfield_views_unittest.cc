// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/textfield/native_textfield_views.h"

#include <set>
#include <string>
#include <vector>

#include "base/auto_reset.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/pickle.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ui_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/render_text.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/controls/textfield/textfield_views_model.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/ime/mock_input_method.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/win/windows_version.h"
#endif

#define EXPECT_STR_EQ(ascii, utf16) EXPECT_EQ(ASCIIToUTF16(ascii), utf16)

namespace {

const char16 kHebrewLetterSamekh = 0x05E1;

// A Textfield wrapper to intercept OnKey[Pressed|Released]() ressults.
class TestTextfield : public views::Textfield {
 public:
  explicit TestTextfield(StyleFlags style)
      : Textfield(style),
        key_handled_(false),
        key_received_(false) {
  }

  virtual bool OnKeyPressed(const ui::KeyEvent& e) OVERRIDE {
    key_received_ = true;
    key_handled_ = views::Textfield::OnKeyPressed(e);
    return key_handled_;
  }

  virtual bool OnKeyReleased(const ui::KeyEvent& e) OVERRIDE {
    key_received_ = true;
    key_handled_ = views::Textfield::OnKeyReleased(e);
    return key_handled_;
  }

  bool key_handled() const { return key_handled_; }
  bool key_received() const { return key_received_; }

  void clear() { key_received_ = key_handled_ = false; }

 private:
  bool key_handled_;
  bool key_received_;

  DISALLOW_COPY_AND_ASSIGN(TestTextfield);
};

// A helper class for use with ui::TextInputClient::GetTextFromRange().
class GetTextHelper {
 public:
  GetTextHelper() {}

  void set_text(const string16& text) { text_ = text; }
  const string16& text() const { return text_; }

 private:
  string16 text_;

  DISALLOW_COPY_AND_ASSIGN(GetTextHelper);
};

// Convenience to make constructing a GestureEvent simpler.
class GestureEventForTest : public ui::GestureEvent {
 public:
  GestureEventForTest(ui::EventType type, int x, int y, float delta_x,
                      float delta_y)
      : GestureEvent(type, x, y, 0, base::TimeDelta(),
                     ui::GestureEventDetails(type, delta_x, delta_y), 0) {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GestureEventForTest);
};

}  // namespace

namespace views {

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
  }

  virtual void TearDown() {
    if (widget_)
      widget_->Close();
    ViewsTestBase::TearDown();
  }

  // TextfieldController:
  virtual void ContentsChanged(Textfield* sender,
                               const string16& new_contents) OVERRIDE {
    // Paste calls TextfieldController::ContentsChanged() explicitly even if the
    // paste action did not change the content. So |new_contents| may match
    // |last_contents_|. For more info, see http://crbug.com/79002
    last_contents_ = new_contents;
  }

  virtual bool HandleKeyEvent(Textfield* sender,
                              const ui::KeyEvent& key_event) OVERRIDE {
    // TODO(oshima): figure out how to test the keystroke.
    return false;
  }

  virtual void OnBeforeUserAction(Textfield* sender) OVERRIDE {
    ++on_before_user_action_;
  }

  virtual void OnAfterUserAction(Textfield* sender) OVERRIDE {
    ++on_after_user_action_;
  }

  void InitTextfield(Textfield::StyleFlags style) {
    InitTextfields(style, 1);
  }

  void InitTextfields(Textfield::StyleFlags style, int count) {
    ASSERT_FALSE(textfield_);
    textfield_ = new TestTextfield(style);
    textfield_->SetController(this);
    widget_ = new Widget();
    Widget::InitParams params = CreateParams(Widget::InitParams::TYPE_POPUP);
    params.bounds = gfx::Rect(100, 100, 100, 100);
    widget_->Init(params);
    View* container = new View();
    widget_->SetContentsView(container);
    container->AddChildView(textfield_);

    textfield_view_ = static_cast<NativeTextfieldViews*>(
        textfield_->GetNativeWrapperForTesting());
    DCHECK(textfield_view_);
    textfield_view_->SetBoundsRect(params.bounds);
    textfield_->set_id(1);

    for (int i = 1; i < count; i++) {
      Textfield* textfield = new Textfield(style);
      container->AddChildView(textfield);
      textfield->set_id(i + 1);
    }

    model_ = textfield_view_->model_.get();
    model_->ClearEditHistory();

    input_method_ = new MockInputMethod();
    widget_->ReplaceInputMethod(input_method_);

    // Activate the widget and focus the textfield for input handling.
    widget_->Activate();
    textfield_->RequestFocus();
  }

  ui::MenuModel* GetContextMenuModel() {
    textfield_view_->UpdateContextMenu();
    return textfield_view_->context_menu_contents_.get();
  }

  ui::TouchSelectionController* GetTouchSelectionController() {
    return textfield_view_->touch_selection_controller_.get();
  }

 protected:
  void SendKeyEvent(ui::KeyboardCode key_code,
                    bool alt,
                    bool shift,
                    bool control,
                    bool caps_lock) {
    int flags = (alt ? ui::EF_ALT_DOWN : 0) |
                (shift ? ui::EF_SHIFT_DOWN : 0) |
                (control ? ui::EF_CONTROL_DOWN : 0) |
                (caps_lock ? ui::EF_CAPS_LOCK_DOWN : 0);
    ui::KeyEvent event(ui::ET_KEY_PRESSED, key_code, flags, false);
    input_method_->DispatchKeyEvent(event);
  }

  void SendKeyEvent(ui::KeyboardCode key_code, bool shift, bool control) {
    SendKeyEvent(key_code, false, shift, control, false);
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
      ui::KeyEvent event(ui::ET_KEY_PRESSED, ui::VKEY_UNKNOWN, 0, false);
      event.set_character(ch);
      input_method_->DispatchKeyEvent(event);
    }
  }

  string16 GetClipboardText() const {
    string16 text;
    ui::Clipboard::GetForCurrentThread()->
        ReadText(ui::CLIPBOARD_TYPE_COPY_PASTE, &text);
    return text;
  }

  void SetClipboardText(const std::string& text) {
    ui::ScopedClipboardWriter clipboard_writer(
        ui::Clipboard::GetForCurrentThread(),
        ui::CLIPBOARD_TYPE_COPY_PASTE);
    clipboard_writer.WriteText(ASCIIToUTF16(text));
  }

  View* GetFocusedView() {
    return widget_->GetFocusManager()->GetFocusedView();
  }

  int GetCursorPositionX(int cursor_pos) {
    gfx::RenderText* render_text = textfield_view_->GetRenderText();
    return render_text->GetCursorBounds(
        gfx::SelectionModel(cursor_pos, gfx::CURSOR_FORWARD), false).x();
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
    gfx::Point point(bound.x() +  x_offset, bound.y() + bound.height() / 2);
    ui::MouseEvent click(ui::ET_MOUSE_PRESSED, point, point,
                         ui::EF_LEFT_MOUSE_BUTTON);
    textfield_view_->OnMousePressed(click);
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, point, point,
                           ui::EF_LEFT_MOUSE_BUTTON);
    textfield_view_->OnMouseReleased(release);
  }

  // This is to avoid double/triple click.
  void NonClientMouseClick() {
    ui::MouseEvent click(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                         ui::EF_LEFT_MOUSE_BUTTON | ui::EF_IS_NON_CLIENT);
    textfield_view_->OnMousePressed(click);
    ui::MouseEvent release(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                           ui::EF_LEFT_MOUSE_BUTTON | ui::EF_IS_NON_CLIENT);
    textfield_view_->OnMouseReleased(release);
  }

  // Wrap for visibility in test classes.
  ui::TextInputType GetTextInputType() {
    return textfield_view_->GetTextInputType();
  }

  void VerifyTextfieldContextMenuContents(bool textfield_has_selection,
                                          bool can_undo,
                                          ui::MenuModel* menu) {
    EXPECT_EQ(can_undo, menu->IsEnabledAt(0 /* UNDO */));
    EXPECT_TRUE(menu->IsEnabledAt(1 /* Separator */));
    EXPECT_EQ(textfield_has_selection, menu->IsEnabledAt(2 /* CUT */));
    EXPECT_EQ(textfield_has_selection, menu->IsEnabledAt(3 /* COPY */));
    EXPECT_NE(GetClipboardText().empty(), menu->IsEnabledAt(4 /* PASTE */));
    EXPECT_EQ(textfield_has_selection, menu->IsEnabledAt(5 /* DELETE */));
    EXPECT_TRUE(menu->IsEnabledAt(6 /* Separator */));
    EXPECT_TRUE(menu->IsEnabledAt(7 /* SELECT ALL */));
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
  textfield_->SelectAll(false);
  EXPECT_STR_EQ("this is a test", textfield_->GetSelectedText());
  EXPECT_TRUE(last_contents_.empty());
}

TEST_F(NativeTextfieldViewsTest, ModelChangesTestLowerCase) {
  // Check if |model_|'s text is properly lowercased for STYLE_LOWERCASE.
  InitTextfield(Textfield::STYLE_LOWERCASE);
  EXPECT_EQ(0U, textfield_->GetCursorPosition());

  last_contents_.clear();
  textfield_->SetText(ASCIIToUTF16("THIS IS"));
  EXPECT_EQ(7U, textfield_->GetCursorPosition());

  EXPECT_STR_EQ("this is", model_->GetText());
  EXPECT_STR_EQ("THIS IS", textfield_->text());
  EXPECT_TRUE(last_contents_.empty());

  textfield_->AppendText(ASCIIToUTF16(" A TEST"));
  EXPECT_EQ(7U, textfield_->GetCursorPosition());
  EXPECT_STR_EQ("this is a test", model_->GetText());
  EXPECT_STR_EQ("THIS IS A TEST", textfield_->text());

  EXPECT_TRUE(last_contents_.empty());
}

TEST_F(NativeTextfieldViewsTest, ModelChangesTestLowerCaseI18n) {
  // Check if lower case conversion works for non-ASCII characters.
  InitTextfield(Textfield::STYLE_LOWERCASE);
  EXPECT_EQ(0U, textfield_->GetCursorPosition());

  last_contents_.clear();
  // Zenkaku Japanese "ABCabc"
  textfield_->SetText(WideToUTF16(L"\xFF21\xFF22\xFF23\xFF41\xFF42\xFF43"));
  EXPECT_EQ(6U, textfield_->GetCursorPosition());
  // Zenkaku Japanese "abcabc"
  EXPECT_EQ(WideToUTF16(L"\xFF41\xFF42\xFF43\xFF41\xFF42\xFF43"),
            model_->GetText());
  // Zenkaku Japanese "ABCabc"
  EXPECT_EQ(WideToUTF16(L"\xFF21\xFF22\xFF23\xFF41\xFF42\xFF43"),
            textfield_->text());
  EXPECT_TRUE(last_contents_.empty());

  // Zenkaku Japanese "XYZxyz"
  textfield_->AppendText(WideToUTF16(L"\xFF38\xFF39\xFF3A\xFF58\xFF59\xFF5A"));
  EXPECT_EQ(6U, textfield_->GetCursorPosition());
  // Zenkaku Japanese "abcabcxyzxyz"
  EXPECT_EQ(WideToUTF16(L"\xFF41\xFF42\xFF43\xFF41\xFF42\xFF43"
                        L"\xFF58\xFF59\xFF5A\xFF58\xFF59\xFF5A"),
            model_->GetText());
  // Zenkaku Japanese "ABCabcXYZxyz"
  EXPECT_EQ(WideToUTF16(L"\xFF21\xFF22\xFF23\xFF41\xFF42\xFF43"
                        L"\xFF38\xFF39\xFF3A\xFF58\xFF59\xFF5A"),
            textfield_->text());
  EXPECT_TRUE(last_contents_.empty());
}

TEST_F(NativeTextfieldViewsTest, ModelChangesTestLowerCaseWithLocale) {
  // Check if lower case conversion honors locale properly.
  std::string locale = l10n_util::GetApplicationLocale("");
  base::i18n::SetICUDefaultLocale("tr");

  InitTextfield(Textfield::STYLE_LOWERCASE);
  EXPECT_EQ(0U, textfield_->GetCursorPosition());

  last_contents_.clear();
  // Turkish 'I' should be converted to dotless 'i' (U+0131).
  textfield_->SetText(WideToUTF16(L"I"));
  EXPECT_EQ(1U, textfield_->GetCursorPosition());
  EXPECT_EQ(WideToUTF16(L"\x0131"), model_->GetText());
  EXPECT_EQ(WideToUTF16(L"I"), textfield_->text());
  EXPECT_TRUE(last_contents_.empty());

  base::i18n::SetICUDefaultLocale(locale);

  // On default (en) locale, 'I' should be converted to 'i'.
  textfield_->SetText(WideToUTF16(L"I"));
  EXPECT_EQ(1U, textfield_->GetCursorPosition());
  EXPECT_EQ(WideToUTF16(L"i"), model_->GetText());
  EXPECT_EQ(WideToUTF16(L"I"), textfield_->text());
  EXPECT_TRUE(last_contents_.empty());
}

TEST_F(NativeTextfieldViewsTest, KeyTest) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  // Event flags:  key,    alt,   shift, ctrl,  caps-lock.
  SendKeyEvent(ui::VKEY_T, false, true,  false, false);
  SendKeyEvent(ui::VKEY_E, false, false, false, false);
  SendKeyEvent(ui::VKEY_X, false, true,  false, true);
  SendKeyEvent(ui::VKEY_T, false, false, false, true);
  SendKeyEvent(ui::VKEY_1, false, true,  false, false);
  SendKeyEvent(ui::VKEY_1, false, false, false, false);
  SendKeyEvent(ui::VKEY_1, false, true,  false, true);
  SendKeyEvent(ui::VKEY_1, false, false, false, true);
  EXPECT_STR_EQ("TexT!1!1", textfield_->text());
}

TEST_F(NativeTextfieldViewsTest, ControlAndSelectTest) {
  // Insert a test string in a textfield.
  InitTextfield(Textfield::STYLE_DEFAULT);
  textfield_->SetText(ASCIIToUTF16("one two three"));
  SendKeyEvent(ui::VKEY_HOME,  false /* shift */, false /* control */);
  SendKeyEvent(ui::VKEY_RIGHT, true, false);
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
  for (size_t i = 0; i < 10; i++)
    SendKeyEvent(static_cast<ui::KeyboardCode>(ui::VKEY_A + i));
  EXPECT_STR_EQ("abcdefghij", textfield_->text());

  // Test the delete and backspace keys.
  textfield_->SelectRange(gfx::Range(5));
  for (int i = 0; i < 3; i++)
    SendKeyEvent(ui::VKEY_BACK);
  EXPECT_STR_EQ("abfghij", textfield_->text());
  for (int i = 0; i < 3; i++)
    SendKeyEvent(ui::VKEY_DELETE);
  EXPECT_STR_EQ("abij", textfield_->text());

  // Select all and replace with "k".
  textfield_->SelectAll(false);
  SendKeyEvent(ui::VKEY_K);
  EXPECT_STR_EQ("k", textfield_->text());

  // Delete the previous word from cursor.
  textfield_->SetText(ASCIIToUTF16("one two three four"));
  SendKeyEvent(ui::VKEY_END);
  SendKeyEvent(ui::VKEY_BACK, false, false, true, false);
  EXPECT_STR_EQ("one two three ", textfield_->text());

  // Delete text preceeding the cursor in chromeos, do nothing in windows.
  SendKeyEvent(ui::VKEY_LEFT, false, false, true, false);
  SendKeyEvent(ui::VKEY_BACK, false, true, true, false);
#if defined(OS_WIN)
  EXPECT_STR_EQ("one two three ", textfield_->text());
#else
  EXPECT_STR_EQ("three ", textfield_->text());
#endif

  // Delete the next word from cursor.
  textfield_->SetText(ASCIIToUTF16("one two three four"));
  SendKeyEvent(ui::VKEY_HOME);
  SendKeyEvent(ui::VKEY_DELETE, false, false, true, false);
  EXPECT_STR_EQ(" two three four", textfield_->text());

  // Delete text following the cursor in chromeos, do nothing in windows.
  SendKeyEvent(ui::VKEY_RIGHT, false, false, true, false);
  SendKeyEvent(ui::VKEY_DELETE, false, true, true, false);
#if defined(OS_WIN)
  EXPECT_STR_EQ(" two three four", textfield_->text());
#else
  EXPECT_STR_EQ(" two", textfield_->text());
#endif
}

TEST_F(NativeTextfieldViewsTest, PasswordTest) {
  InitTextfield(Textfield::STYLE_OBSCURED);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, GetTextInputType());
  EXPECT_TRUE(textfield_->enabled());
  EXPECT_TRUE(textfield_->focusable());

  last_contents_.clear();
  textfield_->SetText(ASCIIToUTF16("password"));
  // Ensure text() and the callback returns the actual text instead of "*".
  EXPECT_STR_EQ("password", textfield_->text());
  EXPECT_TRUE(last_contents_.empty());
  model_->SelectAll(false);
  SetClipboardText("foo");

  // Cut and copy should be disabled.
  EXPECT_FALSE(textfield_view_->IsCommandIdEnabled(IDS_APP_CUT));
  textfield_view_->ExecuteCommand(IDS_APP_CUT, 0);
  SendKeyEvent(ui::VKEY_X, false, true);
  EXPECT_FALSE(textfield_view_->IsCommandIdEnabled(IDS_APP_COPY));
  textfield_view_->ExecuteCommand(IDS_APP_COPY, 0);
  SendKeyEvent(ui::VKEY_C, false, true);
  SendKeyEvent(ui::VKEY_INSERT, false, true);
  EXPECT_STR_EQ("foo", string16(GetClipboardText()));
  EXPECT_STR_EQ("password", textfield_->text());
  // [Shift]+[Delete] should just delete without copying text to the clipboard.
  textfield_->SelectAll(false);
  SendKeyEvent(ui::VKEY_DELETE, true, false);

  // Paste should work normally.
  EXPECT_TRUE(textfield_view_->IsCommandIdEnabled(IDS_APP_PASTE));
  textfield_view_->ExecuteCommand(IDS_APP_PASTE, 0);
  SendKeyEvent(ui::VKEY_V, false, true);
  SendKeyEvent(ui::VKEY_INSERT, true, false);
  EXPECT_STR_EQ("foo", string16(GetClipboardText()));
  EXPECT_STR_EQ("foofoofoo", textfield_->text());
}

TEST_F(NativeTextfieldViewsTest, InputTypeSetsObscured) {
  InitTextfield(Textfield::STYLE_DEFAULT);

  // Defaults to TEXT
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, GetTextInputType());

  // Setting to TEXT_INPUT_TYPE_PASSWORD also sets obscured state of textfield.
  textfield_->SetTextInputType(ui::TEXT_INPUT_TYPE_PASSWORD);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, GetTextInputType());
  EXPECT_TRUE(textfield_->IsObscured());
}

TEST_F(NativeTextfieldViewsTest, ObscuredSetsInputType) {
  InitTextfield(Textfield::STYLE_DEFAULT);

  // Defaults to TEXT
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_TEXT, GetTextInputType());

  textfield_->SetObscured(true);
  EXPECT_EQ(ui::TEXT_INPUT_TYPE_PASSWORD, GetTextInputType());

  textfield_->SetObscured(false);
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
  textfield_->clear();

  // Empty Textfield does not handle left/right.
  textfield_->SetText(string16());
  SendKeyEvent(ui::VKEY_LEFT);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_FALSE(textfield_->key_handled());
  textfield_->clear();

  SendKeyEvent(ui::VKEY_RIGHT);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_FALSE(textfield_->key_handled());
  textfield_->clear();

  // Add a char. Right key should not be handled when cursor is at the end.
  SendKeyEvent(ui::VKEY_B);
  SendKeyEvent(ui::VKEY_RIGHT);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_FALSE(textfield_->key_handled());
  textfield_->clear();

  // First left key is handled to move cursor left to the beginning.
  SendKeyEvent(ui::VKEY_LEFT);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_TRUE(textfield_->key_handled());
  textfield_->clear();

  // Now left key should not be handled.
  SendKeyEvent(ui::VKEY_LEFT);
  EXPECT_TRUE(textfield_->key_received());
  EXPECT_FALSE(textfield_->key_handled());
  textfield_->clear();
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
  ui::MouseEvent click(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EF_LEFT_MOUSE_BUTTON);
  textfield_view_->OnMousePressed(click);
  EXPECT_EQ(1, GetFocusedView()->id());
}

TEST_F(NativeTextfieldViewsTest, ContextMenuDisplayTest) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  EXPECT_TRUE(textfield_->context_menu_controller());
  textfield_->SetText(ASCIIToUTF16("hello world"));
  ui::Clipboard::GetForCurrentThread()->Clear(ui::CLIPBOARD_TYPE_COPY_PASTE);
  textfield_view_->ClearEditHistory();
  EXPECT_TRUE(GetContextMenuModel());
  VerifyTextfieldContextMenuContents(false, false, GetContextMenuModel());

  textfield_->SelectAll(false);
  VerifyTextfieldContextMenuContents(true, false, GetContextMenuModel());

  SendKeyEvent(ui::VKEY_T);
  VerifyTextfieldContextMenuContents(false, true, GetContextMenuModel());

  textfield_->SelectAll(false);
  VerifyTextfieldContextMenuContents(true, true, GetContextMenuModel());

  // Exercise the "paste enabled?" check in the verifier.
  SetClipboardText("Test");
  VerifyTextfieldContextMenuContents(true, true, GetContextMenuModel());
}

TEST_F(NativeTextfieldViewsTest, DoubleAndTripleClickTest) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  textfield_->SetText(ASCIIToUTF16("hello world"));
  ui::MouseEvent click(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                       ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent release(ui::ET_MOUSE_RELEASED, gfx::Point(), gfx::Point(),
                         ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent double_click(
      ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
      ui::EF_LEFT_MOUSE_BUTTON | ui::EF_IS_DOUBLE_CLICK);

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

  // Another click should reset back to double click.
  textfield_view_->OnMousePressed(click);
  textfield_view_->OnMouseReleased(release);
  EXPECT_STR_EQ("hello", textfield_->GetSelectedText());
}

TEST_F(NativeTextfieldViewsTest, DragToSelect) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  textfield_->SetText(ASCIIToUTF16("hello world"));
  const int kStart = GetCursorPositionX(5);
  const int kEnd = 500;
  gfx::Point start_point(kStart, 0);
  gfx::Point end_point(kEnd, 0);
  ui::MouseEvent click_a(ui::ET_MOUSE_PRESSED, start_point, start_point,
                         ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent click_b(ui::ET_MOUSE_PRESSED, end_point, end_point,
                         ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent drag_left(ui::ET_MOUSE_DRAGGED, gfx::Point(), gfx::Point(),
                           ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent drag_right(ui::ET_MOUSE_DRAGGED, end_point, end_point,
                            ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent release(ui::ET_MOUSE_RELEASED, end_point, end_point,
                         ui::EF_LEFT_MOUSE_BUTTON);
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

#if defined(OS_WIN)
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
  gfx::Point drop_point(GetCursorPositionX(6), 0);
  ui::DropTargetEvent drop(data, drop_point, drop_point,
      ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_MOVE);
  EXPECT_EQ(ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_MOVE,
            textfield_view_->OnDragUpdated(drop));
  EXPECT_EQ(ui::DragDropTypes::DRAG_COPY, textfield_view_->OnPerformDrop(drop));
  EXPECT_STR_EQ("hello string world", textfield_->text());

  // Ensure that textfields do not accept non-OSExchangeData::STRING types.
  ui::OSExchangeData bad_data;
  bad_data.SetFilename(base::FilePath(FILE_PATH_LITERAL("x")));
#if defined(OS_WIN)
  ui::OSExchangeData::CustomFormat fmt = ui::Clipboard::GetBitmapFormatType();
  bad_data.SetPickledData(fmt, Pickle());
  bad_data.SetFileContents(base::FilePath(L"x"), "x");
  bad_data.SetHtml(string16(ASCIIToUTF16("x")), GURL("x.org"));
  ui::OSExchangeData::DownloadFileInfo download(base::FilePath(), NULL);
  bad_data.SetDownloadFileInfo(download);
#endif
  EXPECT_FALSE(textfield_view_->CanDrop(bad_data));
}
#endif

TEST_F(NativeTextfieldViewsTest, DragAndDrop_InitiateDrag) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  textfield_->SetText(ASCIIToUTF16("hello string world"));

  // Ensure the textfield will provide selected text for drag data.
  string16 string;
  ui::OSExchangeData data;
  const gfx::Range kStringRange(6, 12);
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
  // Ensure that password textfields do not support drag operations.
  textfield_->SetObscured(true);
  EXPECT_EQ(ui::DragDropTypes::DRAG_NONE,
            textfield_view_->GetDragOperationsForView(NULL, kStringPoint));
  textfield_->SetObscured(false);
  // Ensure that textfields only initiate drag operations inside the selection.
  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, kStringPoint, kStringPoint,
                             ui::EF_LEFT_MOUSE_BUTTON);
  textfield_view_->OnMousePressed(press_event);
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

TEST_F(NativeTextfieldViewsTest, DragAndDrop_ToTheRight) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  textfield_->SetText(ASCIIToUTF16("hello world"));

  string16 string;
  ui::OSExchangeData data;
  int formats = 0;
  int operations = 0;
  std::set<OSExchangeData::CustomFormat> custom_formats;

  // Start dragging "ello".
  textfield_->SelectRange(gfx::Range(1, 5));
  gfx::Point point(GetCursorPositionX(3), 0);
  ui::MouseEvent click_a(ui::ET_MOUSE_PRESSED, point, point,
                         ui::EF_LEFT_MOUSE_BUTTON);
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
  ui::DropTargetEvent drop_a(data, kDropPoint, kDropPoint, operations);
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

TEST_F(NativeTextfieldViewsTest, DragAndDrop_ToTheLeft) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  textfield_->SetText(ASCIIToUTF16("hello world"));

  string16 string;
  ui::OSExchangeData data;
  int formats = 0;
  int operations = 0;
  std::set<OSExchangeData::CustomFormat> custom_formats;

  // Start dragging " worl".
  textfield_->SelectRange(gfx::Range(5, 10));
  gfx::Point point(GetCursorPositionX(7), 0);
  ui::MouseEvent click_a(ui::ET_MOUSE_PRESSED, point, point,
                         ui::EF_LEFT_MOUSE_BUTTON);
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
  gfx::Point drop_point(GetCursorPositionX(1), 0);
  ui::DropTargetEvent drop_a(data, drop_point, drop_point, operations);
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

TEST_F(NativeTextfieldViewsTest, DragAndDrop_Canceled) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  textfield_->SetText(ASCIIToUTF16("hello world"));

  // Start dragging "worl".
  textfield_->SelectRange(gfx::Range(6, 10));
  gfx::Point point(GetCursorPositionX(8), 0);
  ui::MouseEvent click(ui::ET_MOUSE_PRESSED, point, point,
                       ui::EF_LEFT_MOUSE_BUTTON);
  textfield_view_->OnMousePressed(click);
  ui::OSExchangeData data;
  textfield_view_->WriteDragDataForView(NULL, click.location(), &data);
  EXPECT_TRUE(textfield_view_->CanDrop(data));
  // Drag the text over somewhere valid, outside the current selection.
  gfx::Point drop_point(GetCursorPositionX(2), 0);
  ui::DropTargetEvent drop(data, drop_point, drop_point,
                           ui::DragDropTypes::DRAG_MOVE);
  EXPECT_EQ(ui::DragDropTypes::DRAG_MOVE, textfield_view_->OnDragUpdated(drop));
  // "Cancel" the drag, via move and release over the selection, and OnDragDone.
  gfx::Point drag_point(GetCursorPositionX(9), 0);
  ui::MouseEvent drag(ui::ET_MOUSE_DRAGGED, drag_point, drag_point,
                      ui::EF_LEFT_MOUSE_BUTTON);
  ui::MouseEvent release(ui::ET_MOUSE_RELEASED, drag_point, drag_point,
                         ui::EF_LEFT_MOUSE_BUTTON);
  textfield_view_->OnMouseDragged(drag);
  textfield_view_->OnMouseReleased(release);
  textfield_view_->OnDragDone();
  EXPECT_EQ(ASCIIToUTF16("hello world"), textfield_->text());
}

TEST_F(NativeTextfieldViewsTest, ReadOnlyTest) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  textfield_->SetText(ASCIIToUTF16("read only"));
  textfield_->SetReadOnly(true);
  EXPECT_TRUE(textfield_->enabled());
  EXPECT_TRUE(textfield_->focusable());

  SendKeyEvent(ui::VKEY_HOME);
  EXPECT_EQ(0U, textfield_->GetCursorPosition());
  SendKeyEvent(ui::VKEY_END);
  EXPECT_EQ(9U, textfield_->GetCursorPosition());

  SendKeyEvent(ui::VKEY_LEFT, false, false);
  EXPECT_EQ(8U, textfield_->GetCursorPosition());
  SendKeyEvent(ui::VKEY_LEFT, false, true);
  EXPECT_EQ(5U, textfield_->GetCursorPosition());
  SendKeyEvent(ui::VKEY_LEFT, true, true);
  EXPECT_EQ(0U, textfield_->GetCursorPosition());
  EXPECT_STR_EQ("read ", textfield_->GetSelectedText());
  textfield_->SelectAll(false);
  EXPECT_STR_EQ("read only", textfield_->GetSelectedText());

  // Cut should be disabled.
  SetClipboardText("Test");
  EXPECT_FALSE(textfield_view_->IsCommandIdEnabled(IDS_APP_CUT));
  textfield_view_->ExecuteCommand(IDS_APP_CUT, 0);
  SendKeyEvent(ui::VKEY_X, false, true);
  SendKeyEvent(ui::VKEY_DELETE, true, false);
  EXPECT_STR_EQ("Test", string16(GetClipboardText()));
  EXPECT_STR_EQ("read only", textfield_->text());

  // Paste should be disabled.
  EXPECT_FALSE(textfield_view_->IsCommandIdEnabled(IDS_APP_PASTE));
  textfield_view_->ExecuteCommand(IDS_APP_PASTE, 0);
  SendKeyEvent(ui::VKEY_V, false, true);
  SendKeyEvent(ui::VKEY_INSERT, true, false);
  EXPECT_STR_EQ("read only", textfield_->text());

  // Copy should work normally.
  SetClipboardText("Test");
  EXPECT_TRUE(textfield_view_->IsCommandIdEnabled(IDS_APP_COPY));
  textfield_view_->ExecuteCommand(IDS_APP_COPY, 0);
  EXPECT_STR_EQ("read only", string16(GetClipboardText()));
  SetClipboardText("Test");
  SendKeyEvent(ui::VKEY_C, false, true);
  EXPECT_STR_EQ("read only", string16(GetClipboardText()));
  SetClipboardText("Test");
  SendKeyEvent(ui::VKEY_INSERT, false, true);
  EXPECT_STR_EQ("read only", string16(GetClipboardText()));

  // SetText should work even in read only mode.
  textfield_->SetText(ASCIIToUTF16(" four five six "));
  EXPECT_STR_EQ(" four five six ", textfield_->text());

  textfield_->SelectAll(false);
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
  gfx::Range range;
  EXPECT_TRUE(client->GetTextRange(&range));
  EXPECT_EQ(0U, range.start());
  EXPECT_EQ(10U, range.end());

  EXPECT_TRUE(client->SetSelectionRange(gfx::Range(1, 4)));
  EXPECT_TRUE(client->GetSelectionRange(&range));
  EXPECT_EQ(gfx::Range(1, 4), range);

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
  EXPECT_EQ(gfx::Range(1, 4), range);
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

  textfield_->clear();
  textfield_->SetText(ASCIIToUTF16("0123456789"));
  EXPECT_TRUE(client->SetSelectionRange(gfx::Range(5, 5)));
  client->ExtendSelectionAndDelete(4, 2);
  EXPECT_STR_EQ("0789", textfield_->text());

  // On{Before,After}UserAction should be called by whatever user action
  // triggers clearing or setting a selection if appropriate.
  on_before_user_action_ = on_after_user_action_ = 0;
  textfield_->clear();
  textfield_->ClearSelection();
  textfield_->SelectAll(false);
  EXPECT_EQ(0, on_before_user_action_);
  EXPECT_EQ(0, on_after_user_action_);

  input_method_->Clear();
  textfield_->SetReadOnly(true);
  EXPECT_TRUE(input_method_->text_input_type_changed());
  EXPECT_FALSE(textfield_->GetTextInputClient());

  textfield_->SetReadOnly(false);
  input_method_->Clear();
  textfield_->SetObscured(true);
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
}

TEST_F(NativeTextfieldViewsTest, CutCopyPaste) {
  InitTextfield(Textfield::STYLE_DEFAULT);

  // Ensure IDS_APP_CUT cuts.
  textfield_->SetText(ASCIIToUTF16("123"));
  textfield_->SelectAll(false);
  EXPECT_TRUE(textfield_view_->IsCommandIdEnabled(IDS_APP_CUT));
  textfield_view_->ExecuteCommand(IDS_APP_CUT, 0);
  EXPECT_STR_EQ("123", string16(GetClipboardText()));
  EXPECT_STR_EQ("", textfield_->text());

  // Ensure [Ctrl]+[x] cuts and [Ctrl]+[Alt][x] does nothing.
  textfield_->SetText(ASCIIToUTF16("456"));
  textfield_->SelectAll(false);
  SendKeyEvent(ui::VKEY_X, true, false, true, false);
  EXPECT_STR_EQ("123", string16(GetClipboardText()));
  EXPECT_STR_EQ("456", textfield_->text());
  SendKeyEvent(ui::VKEY_X, false, true);
  EXPECT_STR_EQ("456", string16(GetClipboardText()));
  EXPECT_STR_EQ("", textfield_->text());

  // Ensure [Shift]+[Delete] cuts.
  textfield_->SetText(ASCIIToUTF16("123"));
  textfield_->SelectAll(false);
  SendKeyEvent(ui::VKEY_DELETE, true, false);
  EXPECT_STR_EQ("123", string16(GetClipboardText()));
  EXPECT_STR_EQ("", textfield_->text());

  // Ensure IDS_APP_COPY copies.
  textfield_->SetText(ASCIIToUTF16("789"));
  textfield_->SelectAll(false);
  EXPECT_TRUE(textfield_view_->IsCommandIdEnabled(IDS_APP_COPY));
  textfield_view_->ExecuteCommand(IDS_APP_COPY, 0);
  EXPECT_STR_EQ("789", string16(GetClipboardText()));

  // Ensure [Ctrl]+[c] copies and [Ctrl]+[Alt][c] does nothing.
  textfield_->SetText(ASCIIToUTF16("012"));
  textfield_->SelectAll(false);
  SendKeyEvent(ui::VKEY_C, true, false, true, false);
  EXPECT_STR_EQ("789", string16(GetClipboardText()));
  SendKeyEvent(ui::VKEY_C, false, true);
  EXPECT_STR_EQ("012", string16(GetClipboardText()));

  // Ensure [Ctrl]+[Insert] copies.
  textfield_->SetText(ASCIIToUTF16("345"));
  textfield_->SelectAll(false);
  SendKeyEvent(ui::VKEY_INSERT, false, true);
  EXPECT_STR_EQ("345", string16(GetClipboardText()));
  EXPECT_STR_EQ("345", textfield_->text());

  // Ensure IDS_APP_PASTE, [Ctrl]+[V], and [Shift]+[Insert] pastes;
  // also ensure that [Ctrl]+[Alt]+[V] does nothing.
  SetClipboardText("abc");
  textfield_->SetText(string16());
  EXPECT_TRUE(textfield_view_->IsCommandIdEnabled(IDS_APP_PASTE));
  textfield_view_->ExecuteCommand(IDS_APP_PASTE, 0);
  EXPECT_STR_EQ("abc", textfield_->text());
  SendKeyEvent(ui::VKEY_V, false, true);
  EXPECT_STR_EQ("abcabc", textfield_->text());
  SendKeyEvent(ui::VKEY_INSERT, true, false);
  EXPECT_STR_EQ("abcabcabc", textfield_->text());
  SendKeyEvent(ui::VKEY_V, true, false, true, false);
  EXPECT_STR_EQ("abcabcabc", textfield_->text());

  // Ensure [Ctrl]+[Shift]+[Insert] is a no-op.
  textfield_->SelectAll(false);
  SendKeyEvent(ui::VKEY_INSERT, true, true);
  EXPECT_STR_EQ("abc", string16(GetClipboardText()));
  EXPECT_STR_EQ("abcabcabc", textfield_->text());
}

TEST_F(NativeTextfieldViewsTest, OvertypeMode) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  // Overtype mode should be disabled (no-op [Insert]).
  textfield_->SetText(ASCIIToUTF16("2"));
  SendKeyEvent(ui::VKEY_HOME);
  SendKeyEvent(ui::VKEY_INSERT);
  SendKeyEvent(ui::VKEY_1, false, false);
  EXPECT_STR_EQ("12", textfield_->text());
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
  EXPECT_EQ(WideToUTF16(L"\x05E1\x5E2" L"a"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_LT(prev_x, x);
  prev_x = x;

  SendKeyEvent('b');
  EXPECT_EQ(WideToUTF16(L"\x05E1\x5E2" L"ab"), textfield_->text());
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
  EXPECT_EQ(WideToUTF16(L"\x05E1\x5E2" L"a"), textfield_->text());
  x = GetCursorBounds().x();
  EXPECT_EQ(prev_x, x);
  prev_x = x;

  SendKeyEvent('b');
  EXPECT_EQ(WideToUTF16(L"\x05E1\x5E2" L"ab"), textfield_->text());
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
  gfx::SelectionModel sel(0, gfx::CURSOR_FORWARD);
  cursor_bounds.push_back(GetCursorBounds(sel));

  sel = gfx::SelectionModel(1, gfx::CURSOR_BACKWARD);
  gfx::Rect bound = GetCursorBounds(sel);
  sel = gfx::SelectionModel(1, gfx::CURSOR_FORWARD);
  EXPECT_EQ(bound.x(), GetCursorBounds(sel).x());
  cursor_bounds.push_back(bound);

  // Check that a cursor at the end of the Latin portion of the text is at the
  // same position as a cursor placed at the end of the RTL Hebrew portion.
  sel = gfx::SelectionModel(2, gfx::CURSOR_BACKWARD);
  bound = GetCursorBounds(sel);
  sel = gfx::SelectionModel(4, gfx::CURSOR_BACKWARD);
  EXPECT_EQ(bound.x(), GetCursorBounds(sel).x());
  cursor_bounds.push_back(bound);

  sel = gfx::SelectionModel(3, gfx::CURSOR_BACKWARD);
  bound = GetCursorBounds(sel);
  sel = gfx::SelectionModel(3, gfx::CURSOR_FORWARD);
  EXPECT_EQ(bound.x(), GetCursorBounds(sel).x());
  cursor_bounds.push_back(bound);

  sel = gfx::SelectionModel(2, gfx::CURSOR_FORWARD);
  bound = GetCursorBounds(sel);
  sel = gfx::SelectionModel(4, gfx::CURSOR_FORWARD);
  EXPECT_EQ(bound.x(), GetCursorBounds(sel).x());
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
  textfield_->SetText(WideToUTF16(L"\x05E1\x5E2" L"ab"));

  SendKeyEvent(ui::VKEY_HOME);
  bound = GetCursorBounds();
  MouseClick(bound, 10);
  EXPECT_EQ(bound, GetCursorBounds());

  SendKeyEvent(ui::VKEY_END);
  bound = GetCursorBounds();
  MouseClick(bound, -10);
  EXPECT_EQ(bound, GetCursorBounds());
}

TEST_F(NativeTextfieldViewsTest, HitOutsideTextAreaInRTLTest) {
  std::string locale = l10n_util::GetApplicationLocale("");
  base::i18n::SetICUDefaultLocale("he");

  InitTextfield(Textfield::STYLE_DEFAULT);

  // RTL-LTR string in RTL context.
  textfield_->SetText(WideToUTF16(L"\x05E1\x5E2" L"ab"));
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
  MouseClick(bound, -10);
  EXPECT_EQ(bound, GetCursorBounds());

  SendKeyEvent(ui::VKEY_END);
  bound = GetCursorBounds();
  MouseClick(bound, 10);
  EXPECT_EQ(bound, GetCursorBounds());

  // Reset locale.
  base::i18n::SetICUDefaultLocale(locale);
}

TEST_F(NativeTextfieldViewsTest, OverflowTest) {
  InitTextfield(Textfield::STYLE_DEFAULT);

  string16 str;
  for (int i = 0; i < 500; ++i)
    SendKeyEvent('a');
  SendKeyEvent(kHebrewLetterSamekh);
  EXPECT_TRUE(GetDisplayRect().Contains(GetCursorBounds()));

  // Test mouse pointing.
  MouseClick(GetCursorBounds(), -1);
  EXPECT_EQ(500U, textfield_->GetCursorPosition());

  // Clear text.
  SendKeyEvent(ui::VKEY_A, false, true);
  SendKeyEvent('\n');

  for (int i = 0; i < 500; ++i)
    SendKeyEvent(kHebrewLetterSamekh);
  SendKeyEvent('a');
  EXPECT_TRUE(GetDisplayRect().Contains(GetCursorBounds()));

  MouseClick(GetCursorBounds(), -1);
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
  EXPECT_TRUE(GetDisplayRect().Contains(GetCursorBounds()));

  MouseClick(GetCursorBounds(), 1);
  EXPECT_EQ(501U, textfield_->GetCursorPosition());

  // Clear text.
  SendKeyEvent(ui::VKEY_A, false, true);
  SendKeyEvent('\n');

  for (int i = 0; i < 500; ++i)
    SendKeyEvent(kHebrewLetterSamekh);
  SendKeyEvent('a');
  EXPECT_TRUE(GetDisplayRect().Contains(GetCursorBounds()));

  MouseClick(GetCursorBounds(), 1);
  EXPECT_EQ(500U, textfield_->GetCursorPosition());

  // Reset locale.
  base::i18n::SetICUDefaultLocale(locale);
}

TEST_F(NativeTextfieldViewsTest, GetCompositionCharacterBoundsTest) {
  InitTextfield(Textfield::STYLE_DEFAULT);

  string16 str;
  const uint32 char_count = 10UL;
  ui::CompositionText composition;
  composition.text = UTF8ToUTF16("0123456789");
  ui::TextInputClient* client = textfield_->GetTextInputClient();

  // Return false if there is no composition text.
  gfx::Rect rect;
  EXPECT_FALSE(client->GetCompositionCharacterBounds(0, &rect));

  // Get each character boundary by cursor.
  gfx::Rect char_rect_in_screen_coord[char_count];
  gfx::Rect prev_cursor = GetCursorBounds();
  for (uint32 i = 0; i < char_count; ++i) {
    composition.selection = gfx::Range(0, i+1);
    client->SetCompositionText(composition);
    EXPECT_TRUE(client->HasCompositionText()) << " i=" << i;
    gfx::Rect cursor_bounds = GetCursorBounds();
    gfx::Point top_left(prev_cursor.x(), prev_cursor.y());
    gfx::Point bottom_right(cursor_bounds.x(), prev_cursor.bottom());
    views::View::ConvertPointToScreen(textfield_view_, &top_left);
    views::View::ConvertPointToScreen(textfield_view_, &bottom_right);
    char_rect_in_screen_coord[i].set_origin(top_left);
    char_rect_in_screen_coord[i].set_width(bottom_right.x() - top_left.x());
    char_rect_in_screen_coord[i].set_height(bottom_right.y() - top_left.y());
    prev_cursor = cursor_bounds;
  }

  for (uint32 i = 0; i < char_count; ++i) {
    gfx::Rect actual_rect;
    EXPECT_TRUE(client->GetCompositionCharacterBounds(i, &actual_rect))
        << " i=" << i;
    EXPECT_EQ(char_rect_in_screen_coord[i], actual_rect) << " i=" << i;
  }

  // Return false if the index is out of range.
  EXPECT_FALSE(client->GetCompositionCharacterBounds(char_count, &rect));
  EXPECT_FALSE(client->GetCompositionCharacterBounds(char_count + 1, &rect));
  EXPECT_FALSE(client->GetCompositionCharacterBounds(char_count + 100, &rect));
}

TEST_F(NativeTextfieldViewsTest, GetCompositionCharacterBounds_ComplexText) {
  InitTextfield(Textfield::STYLE_DEFAULT);

  const char16 kUtf16Chars[] = {
    // U+0020 SPACE
    0x0020,
    // U+1F408 (CAT) as surrogate pair
    0xd83d, 0xdc08,
    // U+5642 as Ideographic Variation Sequences
    0x5642, 0xDB40, 0xDD00,
    // U+260E (BLACK TELEPHONE) as Emoji Variation Sequences
    0x260E, 0xFE0F,
    // U+0020 SPACE
    0x0020,
  };
  const size_t kUtf16CharsCount = arraysize(kUtf16Chars);

  ui::CompositionText composition;
  composition.text.assign(kUtf16Chars, kUtf16Chars + kUtf16CharsCount);
  ui::TextInputClient* client = textfield_->GetTextInputClient();
  client->SetCompositionText(composition);

  // Make sure GetCompositionCharacterBounds never fails for index.
  gfx::Rect rects[kUtf16CharsCount];
  gfx::Rect prev_cursor = GetCursorBounds();
  for (uint32 i = 0; i < kUtf16CharsCount; ++i)
    EXPECT_TRUE(client->GetCompositionCharacterBounds(i, &rects[i]));

  // Here we might expect the following results but it actually depends on how
  // Uniscribe or HarfBuzz treats them with given font.
  // - rects[1] == rects[2]
  // - rects[3] == rects[4] == rects[5]
  // - rects[6] == rects[7]
}

// The word we select by double clicking should remain selected regardless of
// where we drag the mouse afterwards without releasing the left button.
TEST_F(NativeTextfieldViewsTest, KeepInitiallySelectedWord) {
  InitTextfield(Textfield::STYLE_DEFAULT);

  textfield_->SetText(ASCIIToUTF16("abc def ghi"));

  textfield_->SelectRange(gfx::Range(5, 5));
  const gfx::Rect middle_cursor = GetCursorBounds();
  textfield_->SelectRange(gfx::Range(0, 0));
  const gfx::Point beginning = GetCursorBounds().origin();

  // Double click, but do not release the left button.
  MouseClick(middle_cursor, 0);
  const gfx::Point middle(middle_cursor.x(),
                          middle_cursor.y() + middle_cursor.height() / 2);
  ui::MouseEvent press_event(ui::ET_MOUSE_PRESSED, middle, middle,
                             ui::EF_LEFT_MOUSE_BUTTON);
  textfield_view_->OnMousePressed(press_event);
  EXPECT_EQ(gfx::Range(4, 7), textfield_->GetSelectedRange());

  // Drag the mouse to the beginning of the textfield.
  ui::MouseEvent drag_event(ui::ET_MOUSE_DRAGGED, beginning, beginning,
                            ui::EF_LEFT_MOUSE_BUTTON);
  textfield_view_->OnMouseDragged(drag_event);
  EXPECT_EQ(gfx::Range(7, 0), textfield_->GetSelectedRange());
}

// Touch selection and draggin currently only works for chromeos.
#if defined(OS_CHROMEOS)
TEST_F(NativeTextfieldViewsTest, TouchSelectionAndDraggingTest) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  textfield_->SetText(ASCIIToUTF16("hello world"));
  EXPECT_FALSE(GetTouchSelectionController());
  const int eventX = GetCursorPositionX(2);
  const int eventY = 0;
  CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableTouchEditing);

  // Tapping on the textfield should turn on the TouchSelectionController.
  GestureEventForTest tap(ui::ET_GESTURE_TAP, eventX, eventY, 1.0f, 0.0f);
  textfield_view_->OnGestureEvent(&tap);
  EXPECT_TRUE(GetTouchSelectionController());

  // Un-focusing the textfield should reset the TouchSelectionController
  textfield_view_->GetFocusManager()->ClearFocus();
  EXPECT_FALSE(GetTouchSelectionController());

  // With touch editing enabled, long press should not show context menu.
  // Instead, select word and invoke TouchSelectionController.
  GestureEventForTest tap_down(ui::ET_GESTURE_TAP_DOWN, eventX, eventY, 0.0f,
      0.0f);
  textfield_view_->OnGestureEvent(&tap_down);
  GestureEventForTest long_press(ui::ET_GESTURE_LONG_PRESS, eventX, eventY,
      0.0f, 0.0f);
  textfield_view_->OnGestureEvent(&long_press);
  EXPECT_STR_EQ("hello", textfield_->GetSelectedText());
  EXPECT_TRUE(GetTouchSelectionController());

  // Long pressing again in the selecting region should not do anything since
  // touch drag drop is not yet enabled.
  textfield_view_->OnGestureEvent(&tap_down);
  textfield_view_->OnGestureEvent(&long_press);
  EXPECT_STR_EQ("hello", textfield_->GetSelectedText());
  EXPECT_TRUE(GetTouchSelectionController());
  EXPECT_TRUE(long_press.handled());

  // After enabling touch drag drop, long pressing in the selected region should
  // start a drag and remove TouchSelectionController.
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableTouchDragDrop);
  textfield_view_->OnGestureEvent(&tap_down);

  // Create a new long press event since the previous one is not marked handled.
  GestureEventForTest long_press2(ui::ET_GESTURE_LONG_PRESS, eventX, eventY,
      0.0f, 0.0f);
  textfield_view_->OnGestureEvent(&long_press2);
  EXPECT_STR_EQ("hello", textfield_->GetSelectedText());
  EXPECT_FALSE(GetTouchSelectionController());
}

TEST_F(NativeTextfieldViewsTest, TouchScrubbingSelection) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  textfield_->SetText(ASCIIToUTF16("hello world"));
  EXPECT_FALSE(GetTouchSelectionController());

  CommandLine::ForCurrentProcess()->AppendSwitch(switches::kEnableTouchEditing);

  // Simulate touch-scrubbing.
  int scrubbing_start = GetCursorPositionX(1);
  int scrubbing_end = GetCursorPositionX(6);

  GestureEventForTest tap_down(ui::ET_GESTURE_TAP_DOWN, scrubbing_start, 0,
                               0.0f, 0.0f);
  textfield_view_->OnGestureEvent(&tap_down);

  GestureEventForTest tap_cancel(ui::ET_GESTURE_TAP_CANCEL, scrubbing_start, 0,
                                 0.0f, 0.0f);
  textfield_view_->OnGestureEvent(&tap_cancel);

  GestureEventForTest scroll_begin(ui::ET_GESTURE_SCROLL_BEGIN, scrubbing_start,
                                   0, 0.0f, 0.0f);
  textfield_view_->OnGestureEvent(&scroll_begin);

  GestureEventForTest scroll_update(ui::ET_GESTURE_SCROLL_UPDATE, scrubbing_end,
                                    0, scrubbing_end - scrubbing_start, 0.0f);
  textfield_view_->OnGestureEvent(&scroll_update);

  GestureEventForTest scroll_end(ui::ET_GESTURE_SCROLL_END, scrubbing_end, 0,
                                 0.0f, 0.0f);
  textfield_view_->OnGestureEvent(&scroll_end);

  GestureEventForTest end(ui::ET_GESTURE_END, scrubbing_end, 0, 0.0f, 0.0f);
  textfield_view_->OnGestureEvent(&end);

  // In the end, part of text should have been selected and handles should have
  // appeared.
  EXPECT_STR_EQ("ello ", textfield_->GetSelectedText());
  EXPECT_TRUE(GetTouchSelectionController());
}
#endif

// Long_Press gesture in NativeTextfieldViews can initiate a drag and drop now.
TEST_F(NativeTextfieldViewsTest, TestLongPressInitiatesDragDrop) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  textfield_->SetText(ASCIIToUTF16("Hello string world"));

  // Ensure the textfield will provide selected text for drag data.
  textfield_->SelectRange(gfx::Range(6, 12));
  const gfx::Point kStringPoint(GetCursorPositionX(9), 0);

  // Enable touch-drag-drop to make long press effective.
  CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableTouchDragDrop);

  // Create a long press event in the selected region should start a drag.
  GestureEventForTest long_press(ui::ET_GESTURE_LONG_PRESS, kStringPoint.x(),
                                 kStringPoint.y(), 0.0f, 0.0f);
  textfield_view_->OnGestureEvent(&long_press);
  EXPECT_TRUE(textfield_view_->CanStartDragForView(NULL,
      kStringPoint, kStringPoint));
}

TEST_F(NativeTextfieldViewsTest, GetTextfieldBaseline_FontFallbackTest) {
  InitTextfield(Textfield::STYLE_DEFAULT);
  textfield_->SetText(UTF8ToUTF16("abc"));
  const int old_baseline = textfield_->GetBaseline();

  // Set text which may fall back to a font which has taller baseline than
  // the default font.
  textfield_->SetText(UTF8ToUTF16("\xE0\xB9\x91"));
  const int new_baseline = textfield_->GetBaseline();

  // Regardless of the text, the baseline must be the same.
  EXPECT_EQ(new_baseline, old_baseline);
}

}  // namespace views
