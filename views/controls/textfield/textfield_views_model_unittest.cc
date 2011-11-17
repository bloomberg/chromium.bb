// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/auto_reset.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/range/range.h"
#include "ui/gfx/render_text.h"
#include "ui/views/test/test_views_delegate.h"
#include "ui/views/test/views_test_base.h"
#include "views/controls/textfield/textfield.h"
#include "views/controls/textfield/textfield_views_model.h"
#include "views/views_delegate.h"

namespace {

struct WordAndCursor {
  WordAndCursor(const wchar_t* w, size_t c)
      : word(w),
        cursor(c) {
  }

  const wchar_t* word;
  size_t cursor;
};

}  // namespace

namespace views {

class TextfieldViewsModelTest : public ViewsTestBase,
                                public TextfieldViewsModel::Delegate {
 public:
  TextfieldViewsModelTest()
      : ViewsTestBase(),
        composition_text_confirmed_or_cleared_(false) {
  }

  virtual void OnCompositionTextConfirmedOrCleared() {
    composition_text_confirmed_or_cleared_ = true;
  }

 protected:
  void ResetModel(TextfieldViewsModel* model) const {
    model->SetText(ASCIIToUTF16(""));
    model->ClearEditHistory();
  }

  bool composition_text_confirmed_or_cleared_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TextfieldViewsModelTest);
};

#define EXPECT_STR_EQ(ascii, utf16) \
  EXPECT_EQ(ASCIIToWide(ascii), UTF16ToWide(utf16))
TEST_F(TextfieldViewsModelTest, EditString) {
  TextfieldViewsModel model(NULL);
  // append two strings
  model.Append(ASCIIToUTF16("HILL"));
  EXPECT_STR_EQ("HILL", model.GetText());
  model.Append(ASCIIToUTF16("WORLD"));
  EXPECT_STR_EQ("HILLWORLD", model.GetText());

  // Insert "E" to make hello
  model.MoveCursorRight(gfx::CHARACTER_BREAK, false);
  model.InsertChar('E');
  EXPECT_STR_EQ("HEILLWORLD", model.GetText());
  // Replace "I" with "L"
  model.ReplaceChar('L');
  EXPECT_STR_EQ("HELLLWORLD", model.GetText());
  model.ReplaceChar('L');
  model.ReplaceChar('O');
  EXPECT_STR_EQ("HELLOWORLD", model.GetText());

  // Delete 6th char "W", then delete 5th char O"
  EXPECT_EQ(5U, model.GetCursorPosition());
  EXPECT_TRUE(model.Delete());
  EXPECT_STR_EQ("HELLOORLD", model.GetText());
  EXPECT_TRUE(model.Backspace());
  EXPECT_EQ(4U, model.GetCursorPosition());
  EXPECT_STR_EQ("HELLORLD", model.GetText());

  // Move the cursor to start. backspace should fail.
  model.MoveCursorLeft(gfx::LINE_BREAK, false);
  EXPECT_FALSE(model.Backspace());
  EXPECT_STR_EQ("HELLORLD", model.GetText());
  // Move the cursor to the end. delete should fail.
  model.MoveCursorRight(gfx::LINE_BREAK, false);
  EXPECT_FALSE(model.Delete());
  EXPECT_STR_EQ("HELLORLD", model.GetText());
  // but backspace should work.
  EXPECT_TRUE(model.Backspace());
  EXPECT_STR_EQ("HELLORL", model.GetText());

  model.MoveCursorTo(gfx::SelectionModel(5));
  model.ReplaceText(ASCIIToUTF16(" WOR"));
  EXPECT_STR_EQ("HELLO WORL", model.GetText());
}

TEST_F(TextfieldViewsModelTest, EditString_SimpleRTL) {
  TextfieldViewsModel model(NULL);
  // Append two strings.
  model.Append(WideToUTF16(L"\x05d0\x05d1\x05d2"));
  EXPECT_EQ(WideToUTF16(L"\x05d0\x05d1\x05d2"), model.GetText());
  model.Append(WideToUTF16(L"\x05e0\x05e1\x05e2"));
  EXPECT_EQ(WideToUTF16(L"\x05d0\x05d1\x05d2\x05e0\x05e1\x05e2"),
            model.GetText());

  // Insert 0x05f0.
  model.MoveCursorTo(gfx::SelectionModel(1U));
  model.InsertChar(0x05f0);
  EXPECT_EQ(WideToUTF16(L"\x05d0\x05f0\x05d1\x05d2\x05e0\x05e1\x05e2"),
            model.GetText());

  // Replace "\x05d1" with "\x05f1".
  model.ReplaceChar(0x05f1);
  EXPECT_EQ(WideToUTF16(L"\x05d0\x05f0\x5f1\x05d2\x05e0\x05e1\x05e2"),
            model.GetText());

  // Delete and backspace.
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Delete());
  EXPECT_EQ(WideToUTF16(L"\x05d0\x05f0\x5f1\x05e0\x05e1\x05e2"),
            model.GetText());
  EXPECT_TRUE(model.Backspace());
  EXPECT_EQ(2U, model.GetCursorPosition());
  EXPECT_EQ(WideToUTF16(L"\x05d0\x05f0\x05e0\x05e1\x05e2"), model.GetText());
}

TEST_F(TextfieldViewsModelTest, EditString_ComplexScript) {
  TextfieldViewsModel model(NULL);
  // Append two Hindi strings.
  model.Append(WideToUTF16(L"\x0915\x093f\x0915\x094d\x0915"));
  EXPECT_EQ(WideToUTF16(L"\x0915\x093f\x0915\x094d\x0915"),
            model.GetText());
  model.Append(WideToUTF16(L"\x0915\x094d\x092e\x094d"));
  EXPECT_EQ(WideToUTF16(
      L"\x0915\x093f\x0915\x094d\x0915\x0915\x094d\x092e\x094d"),
      model.GetText());

  // Check it is not able to place cursor in middle of a grapheme.
  // TODO(xji): temporarily disable in platform Win since the complex script
  // characters turned into empty square due to font regression. So, not able
  // to test 2 characters belong to the same grapheme.
#if defined(OS_LINUX)
  model.MoveCursorTo(gfx::SelectionModel(1U));
  EXPECT_EQ(0U, model.GetCursorPosition());
#endif

  model.MoveCursorTo(gfx::SelectionModel(2U));
  EXPECT_EQ(2U, model.GetCursorPosition());
  model.InsertChar('a');
  EXPECT_EQ(WideToUTF16(
      L"\x0915\x093f\x0061\x0915\x094d\x0915\x0915\x094d\x092e\x094d"),
      model.GetText());

  // ReplaceChar will replace the whole grapheme.
  model.ReplaceChar('b');
  // TODO(xji): temporarily disable in platform Win since the complex script
  // characters turned into empty square due to font regression. So, not able
  // to test 2 characters belong to the same grapheme.
#if defined(OS_LINUX)
  EXPECT_EQ(WideToUTF16(
      L"\x0915\x093f\x0061\x0062\x0915\x0915\x094d\x092e\x094d"),
      model.GetText());
#endif
  EXPECT_EQ(4U, model.GetCursorPosition());

  // Delete should delete the whole grapheme.
  model.MoveCursorTo(gfx::SelectionModel(0U));
  // TODO(xji): temporarily disable in platform Win since the complex script
  // characters turned into empty square due to font regression. So, not able
  // to test 2 characters belong to the same grapheme.
#if defined(OS_LINUX)
  EXPECT_TRUE(model.Delete());
  EXPECT_EQ(WideToUTF16(L"\x0061\x0062\x0915\x0915\x094d\x092e\x094d"),
            model.GetText());
  model.MoveCursorTo(gfx::SelectionModel(model.GetText().length()));
  EXPECT_TRUE(model.Backspace());
  EXPECT_EQ(WideToUTF16(L"\x0061\x0062\x0915\x0915\x094d\x092e"),
            model.GetText());
#endif

  // Test cursor position and deletion for Hindi Virama.
  model.SetText(WideToUTF16(L"\x0D38\x0D4D\x0D15\x0D16\x0D2E"));
  model.MoveCursorTo(gfx::SelectionModel(0));
  EXPECT_EQ(0U, model.GetCursorPosition());

  // TODO(xji): temporarily disable in platform Win since the complex script
  // characters turned into empty square due to font regression. So, not able
  // to test 2 characters belong to the same grapheme.
#if defined(OS_LINUX)
  model.MoveCursorTo(gfx::SelectionModel(1));
  EXPECT_EQ(0U, model.GetCursorPosition());
#endif

  model.MoveCursorTo(gfx::SelectionModel(2));
  EXPECT_EQ(2U, model.GetCursorPosition());

  model.MoveCursorTo(gfx::SelectionModel(3));
  EXPECT_EQ(3U, model.GetCursorPosition());

  model.MoveCursorTo(gfx::SelectionModel(2));

  EXPECT_TRUE(model.Backspace());
  EXPECT_EQ(WideToUTF16(L"\x0D38\x0D15\x0D16\x0D2E"), model.GetText());

  // Test Delete/Backspace on Hebrew with non-spacing marks.
  // TODO(xji): temporarily disable in platform Win since the complex script
  // characters turned into empty square due to font regression. So, not able
  // to test 2 characters belong to the same grapheme.
#if defined(OS_LINUX)
  model.SetText(WideToUTF16(L"\x05d5\x05b7\x05D9\x05B0\x05D4\x05B4\x05D9"));
  model.MoveCursorTo(gfx::SelectionModel(0));
  EXPECT_TRUE(model.Delete());
  EXPECT_TRUE(model.Delete());
  EXPECT_TRUE(model.Delete());
  EXPECT_TRUE(model.Delete());
  EXPECT_EQ(WideToUTF16(L""), model.GetText());
#endif

  // The first 2 characters are not strong directionality characters.
  model.SetText(WideToUTF16(L"\x002C\x0020\x05D1\x05BC\x05B7\x05E9\x05BC"));
#if defined(OS_WIN)
  model.MoveCursorRight(gfx::LINE_BREAK, false);
#else
  model.MoveCursorLeft(gfx::LINE_BREAK, false);
#endif
  EXPECT_TRUE(model.Backspace());
  EXPECT_EQ(WideToUTF16(L"\x002C\x0020\x05D1\x05BC\x05B7\x05E9"),
            model.GetText());
}

TEST_F(TextfieldViewsModelTest, EmptyString) {
  TextfieldViewsModel model(NULL);
  EXPECT_EQ(string16(), model.GetText());
  EXPECT_EQ(string16(), model.GetSelectedText());
  EXPECT_EQ(string16(), model.GetVisibleText());

  model.MoveCursorLeft(gfx::CHARACTER_BREAK, true);
  EXPECT_EQ(0U, model.GetCursorPosition());
  model.MoveCursorRight(gfx::CHARACTER_BREAK, true);
  EXPECT_EQ(0U, model.GetCursorPosition());

  EXPECT_EQ(string16(), model.GetSelectedText());

  EXPECT_FALSE(model.Delete());
  EXPECT_FALSE(model.Backspace());
}

TEST_F(TextfieldViewsModelTest, Selection) {
  TextfieldViewsModel model(NULL);
  model.Append(ASCIIToUTF16("HELLO"));
  model.MoveCursorRight(gfx::CHARACTER_BREAK, false);
  model.MoveCursorRight(gfx::CHARACTER_BREAK, true);
  EXPECT_STR_EQ("E", model.GetSelectedText());
  model.MoveCursorRight(gfx::CHARACTER_BREAK, true);
  EXPECT_STR_EQ("EL", model.GetSelectedText());

  model.MoveCursorLeft(gfx::LINE_BREAK, true);
  EXPECT_STR_EQ("H", model.GetSelectedText());
  model.MoveCursorRight(gfx::LINE_BREAK, true);
  EXPECT_STR_EQ("ELLO", model.GetSelectedText());
  model.ClearSelection();
  EXPECT_EQ(string16(), model.GetSelectedText());
  model.SelectAll();
  EXPECT_STR_EQ("HELLO", model.GetSelectedText());
  // SelectAll should select towards the end.
  gfx::SelectionModel sel;
  model.GetSelectionModel(&sel);
  EXPECT_EQ(0U, sel.selection_start());
  EXPECT_EQ(5U, sel.selection_end());

  // Select and move cursor
  model.SelectRange(ui::Range(1U, 3U));
  EXPECT_STR_EQ("EL", model.GetSelectedText());
  model.MoveCursorLeft(gfx::CHARACTER_BREAK, false);
  EXPECT_EQ(1U, model.GetCursorPosition());
  model.SelectRange(ui::Range(1U, 3U));
  model.MoveCursorRight(gfx::CHARACTER_BREAK, false);
  EXPECT_EQ(3U, model.GetCursorPosition());

  // Select all and move cursor
  model.SelectAll();
  model.MoveCursorLeft(gfx::CHARACTER_BREAK, false);
  EXPECT_EQ(0U, model.GetCursorPosition());
  model.SelectAll();
  model.MoveCursorRight(gfx::CHARACTER_BREAK, false);
  EXPECT_EQ(5U, model.GetCursorPosition());
}

TEST_F(TextfieldViewsModelTest, Selection_BidiWithNonSpacingMarks) {
  // Selection is a logical operation. And it should work with the arrow
  // keys doing visual movements, while the selection is logical between
  // the (logical) start and end points. Selection is simply defined as
  // the portion of text between the logical positions of the start and end
  // caret positions.
  TextfieldViewsModel model(NULL);
  // TODO(xji): temporarily disable in platform Win since the complex script
  // characters turned into empty square due to font regression. So, not able
  // to test 2 characters belong to the same grapheme.
#if defined(OS_LINUX)
  model.Append(WideToUTF16(
      L"abc\x05E9\x05BC\x05C1\x05B8\x05E0\x05B8"L"def"));
  model.MoveCursorRight(gfx::CHARACTER_BREAK, false);
  model.MoveCursorRight(gfx::CHARACTER_BREAK, false);

  model.MoveCursorRight(gfx::CHARACTER_BREAK, true);
  EXPECT_EQ(2U, model.render_text()->GetSelectionStart());
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_EQ(WideToUTF16(L"c"), model.GetSelectedText());

  model.MoveCursorRight(gfx::CHARACTER_BREAK, true);
  EXPECT_EQ(2U, model.render_text()->GetSelectionStart());
  EXPECT_EQ(7U, model.GetCursorPosition());
  EXPECT_EQ(WideToUTF16(L"c\x05E9\x05BC\x05C1\x05B8"),
            model.GetSelectedText());

  model.MoveCursorRight(gfx::CHARACTER_BREAK, true);
  EXPECT_EQ(2U, model.render_text()->GetSelectionStart());
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_EQ(WideToUTF16(L"c"), model.GetSelectedText());

  model.MoveCursorRight(gfx::CHARACTER_BREAK, true);
  EXPECT_EQ(2U, model.render_text()->GetSelectionStart());
  EXPECT_EQ(10U, model.GetCursorPosition());
  EXPECT_EQ(WideToUTF16(L"c\x05E9\x05BC\x05C1\x05B8\x05E0\x05B8"L"d"),
            model.GetSelectedText());

  model.ClearSelection();
  EXPECT_EQ(string16(), model.GetSelectedText());
  model.SelectAll();
  EXPECT_EQ(WideToUTF16(L"abc\x05E9\x05BC\x05C1\x05B8\x05E0\x05B8"L"def"),
            model.GetSelectedText());
#endif

  // In case of "aBc", this test shows how to select "aB" or "Bc", assume 'B' is
  // an RTL character.
  model.SetText(WideToUTF16(L"a\x05E9"L"b"));
  model.MoveCursorTo(gfx::SelectionModel(0));
  model.MoveCursorRight(gfx::CHARACTER_BREAK, true);
  EXPECT_EQ(WideToUTF16(L"a"), model.GetSelectedText());

  model.MoveCursorRight(gfx::CHARACTER_BREAK, true);
  EXPECT_EQ(WideToUTF16(L"a"), model.GetSelectedText());

  model.MoveCursorRight(gfx::CHARACTER_BREAK, true);
  EXPECT_EQ(WideToUTF16(L"a\x05E9"L"b"), model.GetSelectedText());

  model.MoveCursorRight(gfx::CHARACTER_BREAK, false);
  EXPECT_EQ(3U, model.GetCursorPosition());
  model.MoveCursorLeft(gfx::CHARACTER_BREAK, true);
  EXPECT_EQ(WideToUTF16(L"b"), model.GetSelectedText());

  model.MoveCursorLeft(gfx::CHARACTER_BREAK, true);
  EXPECT_EQ(WideToUTF16(L"b"), model.GetSelectedText());

  model.MoveCursorLeft(gfx::CHARACTER_BREAK, true);
  EXPECT_EQ(WideToUTF16(L"a\x05E9"L"b"), model.GetSelectedText());

  model.MoveCursorLeft(gfx::LINE_BREAK, false);
  model.MoveCursorRight(gfx::CHARACTER_BREAK, true);
  model.MoveCursorRight(gfx::CHARACTER_BREAK, true);
  model.MoveCursorLeft(gfx::CHARACTER_BREAK, true);
  EXPECT_EQ(WideToUTF16(L"a\x05E9"), model.GetSelectedText());

  model.MoveCursorRight(gfx::LINE_BREAK, false);
  model.MoveCursorLeft(gfx::CHARACTER_BREAK, true);
  model.MoveCursorLeft(gfx::CHARACTER_BREAK, true);
  model.MoveCursorRight(gfx::CHARACTER_BREAK, true);
  EXPECT_EQ(WideToUTF16(L"\x05E9"L"b"), model.GetSelectedText());

  model.ClearSelection();
  EXPECT_EQ(string16(), model.GetSelectedText());
  model.SelectAll();
  EXPECT_EQ(WideToUTF16(L"a\x05E9"L"b"), model.GetSelectedText());
}

TEST_F(TextfieldViewsModelTest, SelectionAndEdit) {
  TextfieldViewsModel model(NULL);
  model.Append(ASCIIToUTF16("HELLO"));
  model.MoveCursorRight(gfx::CHARACTER_BREAK, false);
  model.MoveCursorRight(gfx::CHARACTER_BREAK, true);
  model.MoveCursorRight(gfx::CHARACTER_BREAK, true);  // select "EL"
  EXPECT_TRUE(model.Backspace());
  EXPECT_STR_EQ("HLO", model.GetText());

  model.Append(ASCIIToUTF16("ILL"));
  model.MoveCursorRight(gfx::CHARACTER_BREAK, true);
  model.MoveCursorRight(gfx::CHARACTER_BREAK, true);  // select "LO"
  EXPECT_TRUE(model.Delete());
  EXPECT_STR_EQ("HILL", model.GetText());
  EXPECT_EQ(1U, model.GetCursorPosition());
  model.MoveCursorRight(gfx::CHARACTER_BREAK, true);  // select "I"
  model.InsertChar('E');
  EXPECT_STR_EQ("HELL", model.GetText());
  model.MoveCursorLeft(gfx::LINE_BREAK, false);
  model.MoveCursorRight(gfx::CHARACTER_BREAK, true);  // select "H"
  model.ReplaceChar('B');
  EXPECT_STR_EQ("BELL", model.GetText());
  model.MoveCursorRight(gfx::LINE_BREAK, false);
  model.MoveCursorLeft(gfx::CHARACTER_BREAK, true);
  model.MoveCursorLeft(gfx::CHARACTER_BREAK, true);  // select "ELL"
  model.ReplaceChar('E');
  EXPECT_STR_EQ("BEE", model.GetText());
}

TEST_F(TextfieldViewsModelTest, Password) {
  TextfieldViewsModel model(NULL);
  model.set_is_password(true);
  model.Append(ASCIIToUTF16("HELLO"));
  EXPECT_STR_EQ("*****", model.GetVisibleText());
  EXPECT_STR_EQ("HELLO", model.GetText());
  EXPECT_TRUE(model.Delete());

  EXPECT_STR_EQ("****", model.GetVisibleText());
  EXPECT_STR_EQ("ELLO", model.GetText());
  EXPECT_EQ(0U, model.GetCursorPosition());

  model.SelectAll();
  EXPECT_STR_EQ("ELLO", model.GetSelectedText());
  EXPECT_EQ(4U, model.GetCursorPosition());

  model.InsertChar('X');
  EXPECT_STR_EQ("*", model.GetVisibleText());
  EXPECT_STR_EQ("X", model.GetText());
}

TEST_F(TextfieldViewsModelTest, Word) {
  TextfieldViewsModel model(NULL);
  model.Append(
      ASCIIToUTF16("The answer to Life, the Universe, and Everything"));
  model.MoveCursorRight(gfx::WORD_BREAK, false);
  EXPECT_EQ(3U, model.GetCursorPosition());
  model.MoveCursorRight(gfx::WORD_BREAK, false);
  EXPECT_EQ(10U, model.GetCursorPosition());
  model.MoveCursorRight(gfx::WORD_BREAK, false);
  model.MoveCursorRight(gfx::WORD_BREAK, false);
  EXPECT_EQ(18U, model.GetCursorPosition());

  // Should passes the non word char ','
  model.MoveCursorRight(gfx::WORD_BREAK, true);
  EXPECT_EQ(23U, model.GetCursorPosition());
  EXPECT_STR_EQ(", the", model.GetSelectedText());

  // Move to the end.
  model.MoveCursorRight(gfx::WORD_BREAK, true);
  model.MoveCursorRight(gfx::WORD_BREAK, true);
  model.MoveCursorRight(gfx::WORD_BREAK, true);
  EXPECT_STR_EQ(", the Universe, and Everything", model.GetSelectedText());
  // Should be safe to go next word at the end.
  model.MoveCursorRight(gfx::WORD_BREAK, true);
  EXPECT_STR_EQ(", the Universe, and Everything", model.GetSelectedText());
  model.InsertChar('2');
  EXPECT_EQ(19U, model.GetCursorPosition());

  // Now backwards.
  model.MoveCursorLeft(gfx::CHARACTER_BREAK, false);  // leave 2.
  model.MoveCursorLeft(gfx::WORD_BREAK, true);
  EXPECT_EQ(14U, model.GetCursorPosition());
  EXPECT_STR_EQ("Life", model.GetSelectedText());
  model.MoveCursorLeft(gfx::WORD_BREAK, true);
  EXPECT_STR_EQ("to Life", model.GetSelectedText());
  model.MoveCursorLeft(gfx::WORD_BREAK, true);
  model.MoveCursorLeft(gfx::WORD_BREAK, true);
  model.MoveCursorLeft(gfx::WORD_BREAK, true);  // Select to the begining.
  EXPECT_STR_EQ("The answer to Life", model.GetSelectedText());
  // Should be safe to go pervious word at the begining.
  model.MoveCursorLeft(gfx::WORD_BREAK, true);
  EXPECT_STR_EQ("The answer to Life", model.GetSelectedText());
  model.ReplaceChar('4');
  EXPECT_EQ(string16(), model.GetSelectedText());
  EXPECT_STR_EQ("42", model.GetVisibleText());
}

TEST_F(TextfieldViewsModelTest, SetText) {
  TextfieldViewsModel model(NULL);
  model.Append(ASCIIToUTF16("HELLO"));
  model.MoveCursorRight(gfx::LINE_BREAK, false);
  model.SetText(ASCIIToUTF16("GOODBYE"));
  EXPECT_STR_EQ("GOODBYE", model.GetText());
  // SetText won't reset the cursor posistion.
  EXPECT_EQ(5U, model.GetCursorPosition());
  model.SelectAll();
  EXPECT_STR_EQ("GOODBYE", model.GetSelectedText());
  model.MoveCursorRight(gfx::LINE_BREAK, false);
  EXPECT_EQ(7U, model.GetCursorPosition());

  model.SetText(ASCIIToUTF16("BYE"));
  // Setting shorter string moves the cursor to the end of the new string.
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_EQ(string16(), model.GetSelectedText());
  model.SetText(ASCIIToUTF16(""));
  EXPECT_EQ(0U, model.GetCursorPosition());
}

#if defined(USE_AURA) && defined(OS_LINUX)
#define MAYBE_Clipboard DISABLED_Clipboard  // http://crbug.com/97845
#else
#define MAYBE_Clipboard Clipboard
#endif
TEST_F(TextfieldViewsModelTest, MAYBE_Clipboard) {
  ui::Clipboard* clipboard
      = views::ViewsDelegate::views_delegate->GetClipboard();
  string16 initial_clipboard_text = ASCIIToUTF16("initial text");
  ui::ScopedClipboardWriter(clipboard).WriteText(initial_clipboard_text);

  string16 clipboard_text;
  TextfieldViewsModel model(NULL);
  model.Append(ASCIIToUTF16("HELLO WORLD"));
  model.MoveCursorRight(gfx::LINE_BREAK, false);

  // Test for cut: Empty selection.
  EXPECT_FALSE(model.Cut());
  clipboard->ReadText(ui::Clipboard::BUFFER_STANDARD, &clipboard_text);
  EXPECT_STR_EQ(UTF16ToUTF8(initial_clipboard_text), clipboard_text);
  EXPECT_STR_EQ("HELLO WORLD", model.GetText());
  EXPECT_EQ(11U, model.GetCursorPosition());

  // Test for cut: Non-empty selection.
  model.MoveCursorLeft(gfx::WORD_BREAK, true);
  EXPECT_TRUE(model.Cut());
  clipboard->ReadText(ui::Clipboard::BUFFER_STANDARD, &clipboard_text);
  EXPECT_STR_EQ("WORLD", clipboard_text);
  EXPECT_STR_EQ("HELLO ", model.GetText());
  EXPECT_EQ(6U, model.GetCursorPosition());

  // Test for copy: Empty selection.
  model.Copy();
  clipboard->ReadText(ui::Clipboard::BUFFER_STANDARD, &clipboard_text);
  EXPECT_STR_EQ("WORLD", clipboard_text);
  EXPECT_STR_EQ("HELLO ", model.GetText());
  EXPECT_EQ(6U, model.GetCursorPosition());

  // Test for copy: Non-empty selection.
  model.Append(ASCIIToUTF16("HELLO WORLD"));
  model.SelectAll();
  model.Copy();
  clipboard->ReadText(ui::Clipboard::BUFFER_STANDARD, &clipboard_text);
  EXPECT_STR_EQ("HELLO HELLO WORLD", clipboard_text);
  EXPECT_STR_EQ("HELLO HELLO WORLD", model.GetText());
  EXPECT_EQ(17U, model.GetCursorPosition());

  // Test for paste.
  model.ClearSelection();
  model.MoveCursorRight(gfx::LINE_BREAK, false);
  model.MoveCursorLeft(gfx::WORD_BREAK, true);
  EXPECT_TRUE(model.Paste());
  clipboard->ReadText(ui::Clipboard::BUFFER_STANDARD, &clipboard_text);
  EXPECT_STR_EQ("HELLO HELLO WORLD", clipboard_text);
  EXPECT_STR_EQ("HELLO HELLO HELLO HELLO WORLD", model.GetText());
  EXPECT_EQ(29U, model.GetCursorPosition());
}

void SelectWordTestVerifier(TextfieldViewsModel &model,
    const string16 &expected_selected_string, size_t expected_cursor_pos) {
  EXPECT_EQ(expected_selected_string, model.GetSelectedText());
  EXPECT_EQ(expected_cursor_pos, model.GetCursorPosition());
}

TEST_F(TextfieldViewsModelTest, SelectWordTest) {
  TextfieldViewsModel model(NULL);
  model.Append(ASCIIToUTF16("  HELLO  !!  WO     RLD "));

  // Test when cursor is at the beginning.
  model.MoveCursorLeft(gfx::LINE_BREAK, false);
  model.SelectWord();
  SelectWordTestVerifier(model, ASCIIToUTF16("  "), 2U);

  // Test when cursor is at the beginning of a word.
  gfx::SelectionModel selection(2U);
  model.MoveCursorTo(selection);
  model.SelectWord();
  SelectWordTestVerifier(model, ASCIIToUTF16("HELLO"), 7U);

  // Test when cursor is at the end of a word.
  selection = gfx::SelectionModel(15U);
  model.MoveCursorTo(selection);
  model.SelectWord();
  SelectWordTestVerifier(model, ASCIIToUTF16("     "), 20U);

  // Test when cursor is somewhere in a non-alpha-numeric fragment.
  for (size_t cursor_pos = 8; cursor_pos < 13U; cursor_pos++) {
    selection = gfx::SelectionModel(cursor_pos);
    model.MoveCursorTo(selection);
    model.SelectWord();
    SelectWordTestVerifier(model, ASCIIToUTF16("  !!  "), 13U);
  }

  // Test when cursor is somewhere in a whitespace fragment.
  selection = gfx::SelectionModel(17U);
  model.MoveCursorTo(selection);
  model.SelectWord();
  SelectWordTestVerifier(model, ASCIIToUTF16("     "), 20U);

  // Test when cursor is at the end.
  model.MoveCursorRight(gfx::LINE_BREAK, false);
  model.SelectWord();
  SelectWordTestVerifier(model, ASCIIToUTF16(" "), 24U);
}

// TODO(xji): temporarily disable in platform Win since the complex script
// characters and Chinese characters are turned into empty square due to font
// regression.
#if defined(OS_LINUX)
TEST_F(TextfieldViewsModelTest, SelectWordTest_MixScripts) {
  TextfieldViewsModel model(NULL);
  std::vector<WordAndCursor> word_and_cursor;
  word_and_cursor.push_back(WordAndCursor(L"a\x05d0", 2));
  word_and_cursor.push_back(WordAndCursor(L"a\x05d0", 2));
  word_and_cursor.push_back(WordAndCursor(L"\x05d1\x05d2", 5));
  word_and_cursor.push_back(WordAndCursor(L"\x05d1\x05d2", 5));
  word_and_cursor.push_back(WordAndCursor(L" ", 3));
  word_and_cursor.push_back(WordAndCursor(L"a\x05d0", 2));
  word_and_cursor.push_back(WordAndCursor(L"\x0915\x094d\x0915", 9));
  word_and_cursor.push_back(WordAndCursor(L"\x0915\x094d\x0915", 9));
  word_and_cursor.push_back(WordAndCursor(L" ", 10));
  word_and_cursor.push_back(WordAndCursor(L"\x4E2D\x56FD", 12));
  word_and_cursor.push_back(WordAndCursor(L"\x4E2D\x56FD", 12));
  word_and_cursor.push_back(WordAndCursor(L"\x82B1", 13));
  word_and_cursor.push_back(WordAndCursor(L"\x5929", 14));

  // The text consists of Ascii, Hebrew, Hindi with Virama sign, and Chinese.
  model.SetText(WideToUTF16(L"a\x05d0 \x05d1\x05d2 \x0915\x094d\x0915 "
                            L"\x4E2D\x56FD\x82B1\x5929"));
  for (size_t i = 0; i < word_and_cursor.size(); ++i) {
    model.MoveCursorLeft(gfx::LINE_BREAK, false);
    for (size_t j = 0; j < i; ++j)
      model.MoveCursorRight(gfx::CHARACTER_BREAK, false);
    model.SelectWord();
    SelectWordTestVerifier(model, WideToUTF16(word_and_cursor[i].word),
                           word_and_cursor[i].cursor);
  }
}
#endif

TEST_F(TextfieldViewsModelTest, RangeTest) {
  TextfieldViewsModel model(NULL);
  model.Append(ASCIIToUTF16("HELLO WORLD"));
  model.MoveCursorLeft(gfx::LINE_BREAK, false);
  ui::Range range;
  model.GetSelectedRange(&range);
  EXPECT_TRUE(range.is_empty());
  EXPECT_EQ(0U, range.start());
  EXPECT_EQ(0U, range.end());

  model.MoveCursorRight(gfx::WORD_BREAK, true);
  model.GetSelectedRange(&range);
  EXPECT_FALSE(range.is_empty());
  EXPECT_FALSE(range.is_reversed());
  EXPECT_EQ(0U, range.start());
  EXPECT_EQ(5U, range.end());

  model.MoveCursorLeft(gfx::CHARACTER_BREAK, true);
  model.GetSelectedRange(&range);
  EXPECT_FALSE(range.is_empty());
  EXPECT_EQ(0U, range.start());
  EXPECT_EQ(4U, range.end());

  model.MoveCursorLeft(gfx::WORD_BREAK, true);
  model.GetSelectedRange(&range);
  EXPECT_TRUE(range.is_empty());
  EXPECT_EQ(0U, range.start());
  EXPECT_EQ(0U, range.end());

  // now from the end.
  model.MoveCursorRight(gfx::LINE_BREAK, false);
  model.GetSelectedRange(&range);
  EXPECT_TRUE(range.is_empty());
  EXPECT_EQ(11U, range.start());
  EXPECT_EQ(11U, range.end());

  model.MoveCursorLeft(gfx::WORD_BREAK, true);
  model.GetSelectedRange(&range);
  EXPECT_FALSE(range.is_empty());
  EXPECT_TRUE(range.is_reversed());
  EXPECT_EQ(11U, range.start());
  EXPECT_EQ(6U, range.end());

  model.MoveCursorRight(gfx::CHARACTER_BREAK, true);
  model.GetSelectedRange(&range);
  EXPECT_FALSE(range.is_empty());
  EXPECT_TRUE(range.is_reversed());
  EXPECT_EQ(11U, range.start());
  EXPECT_EQ(7U, range.end());

  model.MoveCursorRight(gfx::WORD_BREAK, true);
  model.GetSelectedRange(&range);
  EXPECT_TRUE(range.is_empty());
  EXPECT_EQ(11U, range.start());
  EXPECT_EQ(11U, range.end());

  // Select All
  model.MoveCursorLeft(gfx::LINE_BREAK, true);
  model.GetSelectedRange(&range);
  EXPECT_FALSE(range.is_empty());
  EXPECT_TRUE(range.is_reversed());
  EXPECT_EQ(11U, range.start());
  EXPECT_EQ(0U, range.end());
}

TEST_F(TextfieldViewsModelTest, SelectRangeTest) {
  TextfieldViewsModel model(NULL);
  model.Append(ASCIIToUTF16("HELLO WORLD"));
  ui::Range range(0, 6);
  EXPECT_FALSE(range.is_reversed());
  model.SelectRange(range);
  EXPECT_STR_EQ("HELLO ", model.GetSelectedText());

  range = ui::Range(6, 1);
  EXPECT_TRUE(range.is_reversed());
  model.SelectRange(range);
  EXPECT_STR_EQ("ELLO ", model.GetSelectedText());

  range = ui::Range(2, 1000);
  EXPECT_FALSE(range.is_reversed());
  model.SelectRange(range);
  EXPECT_STR_EQ("LLO WORLD", model.GetSelectedText());

  range = ui::Range(1000, 3);
  EXPECT_TRUE(range.is_reversed());
  model.SelectRange(range);
  EXPECT_STR_EQ("LO WORLD", model.GetSelectedText());

  range = ui::Range(0, 0);
  EXPECT_TRUE(range.is_empty());
  model.SelectRange(range);
  EXPECT_TRUE(model.GetSelectedText().empty());

  range = ui::Range(3, 3);
  EXPECT_TRUE(range.is_empty());
  model.SelectRange(range);
  EXPECT_TRUE(model.GetSelectedText().empty());

  range = ui::Range(1000, 100);
  EXPECT_FALSE(range.is_empty());
  model.SelectRange(range);
  EXPECT_TRUE(model.GetSelectedText().empty());

  range = ui::Range(1000, 1000);
  EXPECT_TRUE(range.is_empty());
  model.SelectRange(range);
  EXPECT_TRUE(model.GetSelectedText().empty());
}

TEST_F(TextfieldViewsModelTest, SelectionModelTest) {
  TextfieldViewsModel model(NULL);
  model.Append(ASCIIToUTF16("HELLO WORLD"));
  model.MoveCursorLeft(gfx::LINE_BREAK, false);
  gfx::SelectionModel sel;
  model.GetSelectionModel(&sel);
  EXPECT_EQ(sel.selection_start(), sel.selection_end());
  EXPECT_EQ(0U, sel.selection_start());
  EXPECT_EQ(0U, sel.selection_end());

  model.MoveCursorRight(gfx::WORD_BREAK, true);
  model.GetSelectionModel(&sel);
  EXPECT_NE(sel.selection_start(), sel.selection_end());
  EXPECT_LE(sel.selection_start(), sel.selection_end());
  EXPECT_EQ(0U, sel.selection_start());
  EXPECT_EQ(5U, sel.selection_end());

  model.MoveCursorLeft(gfx::CHARACTER_BREAK, true);
  model.GetSelectionModel(&sel);
  EXPECT_NE(sel.selection_start(), sel.selection_end());
  EXPECT_EQ(0U, sel.selection_start());
  EXPECT_EQ(4U, sel.selection_end());

  model.MoveCursorLeft(gfx::WORD_BREAK, true);
  model.GetSelectionModel(&sel);
  EXPECT_EQ(sel.selection_start(), sel.selection_end());
  EXPECT_EQ(0U, sel.selection_start());
  EXPECT_EQ(0U, sel.selection_end());

  // now from the end.
  model.MoveCursorRight(gfx::LINE_BREAK, false);
  model.GetSelectionModel(&sel);
  EXPECT_EQ(sel.selection_start(), sel.selection_end());
  EXPECT_EQ(11U, sel.selection_start());
  EXPECT_EQ(11U, sel.selection_end());

  model.MoveCursorLeft(gfx::WORD_BREAK, true);
  model.GetSelectionModel(&sel);
  EXPECT_NE(sel.selection_start(), sel.selection_end());
  EXPECT_GT(sel.selection_start(), sel.selection_end());
  EXPECT_EQ(11U, sel.selection_start());
  EXPECT_EQ(6U, sel.selection_end());

  model.MoveCursorRight(gfx::CHARACTER_BREAK, true);
  model.GetSelectionModel(&sel);
  EXPECT_NE(sel.selection_start(), sel.selection_end());
  EXPECT_GT(sel.selection_start(), sel.selection_end());
  EXPECT_EQ(11U, sel.selection_start());
  EXPECT_EQ(7U, sel.selection_end());

  model.MoveCursorRight(gfx::WORD_BREAK, true);
  model.GetSelectionModel(&sel);
  EXPECT_EQ(sel.selection_start(), sel.selection_end());
  EXPECT_EQ(11U, sel.selection_start());
  EXPECT_EQ(11U, sel.selection_end());

  // Select All
  model.MoveCursorLeft(gfx::LINE_BREAK, true);
  model.GetSelectionModel(&sel);
  EXPECT_NE(sel.selection_start(), sel.selection_end());
  EXPECT_GT(sel.selection_start(), sel.selection_end());
  EXPECT_EQ(11U, sel.selection_start());
  EXPECT_EQ(0U, sel.selection_end());
}

TEST_F(TextfieldViewsModelTest, SelectSelectionModelTest) {
  TextfieldViewsModel model(NULL);
  model.Append(ASCIIToUTF16("HELLO WORLD"));
  model.SelectSelectionModel(gfx::SelectionModel(0, 6, 5,
      gfx::SelectionModel::TRAILING));
  EXPECT_STR_EQ("HELLO ", model.GetSelectedText());

  model.SelectSelectionModel(gfx::SelectionModel(6, 1, 1,
      gfx::SelectionModel::LEADING));
  EXPECT_STR_EQ("ELLO ", model.GetSelectedText());

  model.SelectSelectionModel(gfx::SelectionModel(2, 1000, 999,
      gfx::SelectionModel::TRAILING));
  EXPECT_STR_EQ("LLO WORLD", model.GetSelectedText());

  model.SelectSelectionModel(gfx::SelectionModel(1000, 3, 3,
      gfx::SelectionModel::LEADING));
  EXPECT_STR_EQ("LO WORLD", model.GetSelectedText());

  model.SelectSelectionModel(gfx::SelectionModel(0, 0, 0,
      gfx::SelectionModel::LEADING));
  EXPECT_TRUE(model.GetSelectedText().empty());

  model.SelectSelectionModel(gfx::SelectionModel(3, 3, 3,
      gfx::SelectionModel::LEADING));
  EXPECT_TRUE(model.GetSelectedText().empty());

  model.SelectSelectionModel(gfx::SelectionModel(1000, 100, 100,
      gfx::SelectionModel::LEADING));
  EXPECT_TRUE(model.GetSelectedText().empty());

  model.SelectSelectionModel(gfx::SelectionModel(1000, 1000, 1000,
      gfx::SelectionModel::TRAILING));
  EXPECT_TRUE(model.GetSelectedText().empty());
}

TEST_F(TextfieldViewsModelTest, CompositionTextTest) {
  TextfieldViewsModel model(this);
  model.Append(ASCIIToUTF16("1234590"));
  model.SelectRange(ui::Range(5, 5));
  EXPECT_FALSE(model.HasSelection());
  EXPECT_EQ(5U, model.GetCursorPosition());

  ui::Range range;
  model.GetTextRange(&range);
  EXPECT_EQ(0U, range.start());
  EXPECT_EQ(7U, range.end());

  ui::CompositionText composition;
  composition.text = ASCIIToUTF16("678");
  composition.underlines.push_back(ui::CompositionUnderline(0, 3, 0, false));
  composition.selection = ui::Range(2, 3);
  model.SetCompositionText(composition);
  EXPECT_TRUE(model.HasCompositionText());
  EXPECT_TRUE(model.HasSelection());

  model.GetTextRange(&range);
  EXPECT_EQ(10U, range.end());
  EXPECT_STR_EQ("1234567890", model.GetText());

  model.GetCompositionTextRange(&range);
  EXPECT_EQ(5U, range.start());
  EXPECT_EQ(8U, range.end());
  // composition text
  EXPECT_STR_EQ("456", model.GetTextFromRange(ui::Range(3, 6)));

  gfx::SelectionModel selection;
  model.GetSelectionModel(&selection);
  EXPECT_EQ(7U, selection.selection_start());
  EXPECT_EQ(8U, selection.selection_end());
  EXPECT_STR_EQ("8", model.GetSelectedText());

  EXPECT_FALSE(composition_text_confirmed_or_cleared_);
  model.CancelCompositionText();
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_FALSE(model.HasCompositionText());
  EXPECT_FALSE(model.HasSelection());
  EXPECT_EQ(5U, model.GetCursorPosition());

  model.SetCompositionText(composition);
  EXPECT_STR_EQ("1234567890", model.GetText());
  EXPECT_TRUE(model.SetText(ASCIIToUTF16("1234567890")));
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  model.MoveCursorRight(gfx::LINE_BREAK, false);

  model.SetCompositionText(composition);
  EXPECT_STR_EQ("1234567890678", model.GetText());

  model.InsertText(UTF8ToUTF16("-"));
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("1234567890-", model.GetText());
  EXPECT_FALSE(model.HasCompositionText());
  EXPECT_FALSE(model.HasSelection());

  model.MoveCursorLeft(gfx::CHARACTER_BREAK, true);
  EXPECT_STR_EQ("-", model.GetSelectedText());
  model.SetCompositionText(composition);
  EXPECT_STR_EQ("1234567890678", model.GetText());

  model.ReplaceText(UTF8ToUTF16("-"));
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("1234567890-", model.GetText());
  EXPECT_FALSE(model.HasCompositionText());
  EXPECT_FALSE(model.HasSelection());

  model.SetCompositionText(composition);
  model.Append(UTF8ToUTF16("-"));
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("1234567890-678-", model.GetText());

  model.SetCompositionText(composition);
  model.Delete();
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("1234567890-678-", model.GetText());

  model.SetCompositionText(composition);
  model.Backspace();
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("1234567890-678-", model.GetText());

  model.SetText(string16());
  model.SetCompositionText(composition);
  model.MoveCursorLeft(gfx::CHARACTER_BREAK, false);
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("678", model.GetText());
  EXPECT_EQ(2U, model.GetCursorPosition());

  model.SetCompositionText(composition);
  model.MoveCursorRight(gfx::CHARACTER_BREAK, false);
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("676788", model.GetText());
  EXPECT_EQ(6U, model.GetCursorPosition());

  model.SetCompositionText(composition);
  model.MoveCursorLeft(gfx::WORD_BREAK, false);
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("676788678", model.GetText());

  model.SetText(string16());
  model.SetCompositionText(composition);
  model.MoveCursorRight(gfx::WORD_BREAK, false);
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;

  model.SetCompositionText(composition);
  model.MoveCursorLeft(gfx::LINE_BREAK, true);
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("678678", model.GetText());

  model.SetCompositionText(composition);
  model.MoveCursorRight(gfx::LINE_BREAK, false);
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("678", model.GetText());

  model.SetCompositionText(composition);
  gfx::SelectionModel sel(model.render_text()->GetSelectionStart(),
                          0, 0, gfx::SelectionModel::LEADING);
  model.MoveCursorTo(sel);
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("678678", model.GetText());

  model.SetCompositionText(composition);
  model.SelectRange(ui::Range(0, 3));
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("678", model.GetText());

  model.SetCompositionText(composition);
  model.SelectAll();
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("678", model.GetText());

  model.SetCompositionText(composition);
  model.SelectWord();
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;
  EXPECT_STR_EQ("678", model.GetText());

  model.SetCompositionText(composition);
  model.ClearSelection();
  EXPECT_TRUE(composition_text_confirmed_or_cleared_);
  composition_text_confirmed_or_cleared_ = false;

  model.SetCompositionText(composition);
  EXPECT_FALSE(model.Cut());
  EXPECT_FALSE(composition_text_confirmed_or_cleared_);
}

TEST_F(TextfieldViewsModelTest, UndoRedo_BasicTest) {
  TextfieldViewsModel model(NULL);
  model.InsertChar('a');
  EXPECT_FALSE(model.Redo());  // nothing to redo
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("", model.GetText());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("a", model.GetText());

  // Continuous inserts are treated as one edit.
  model.InsertChar('b');
  model.InsertChar('c');
  EXPECT_STR_EQ("abc", model.GetText());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("a", model.GetText());
  EXPECT_EQ(1U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("", model.GetText());
  EXPECT_EQ(0U, model.GetCursorPosition());

  // Undoing further shouldn't change the text.
  EXPECT_FALSE(model.Undo());
  EXPECT_STR_EQ("", model.GetText());
  EXPECT_FALSE(model.Undo());
  EXPECT_STR_EQ("", model.GetText());
  EXPECT_EQ(0U, model.GetCursorPosition());

  // Redoing to the latest text.
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("a", model.GetText());
  EXPECT_EQ(1U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("abc", model.GetText());
  EXPECT_EQ(3U, model.GetCursorPosition());

  // Backspace ===============================
  EXPECT_TRUE(model.Backspace());
  EXPECT_STR_EQ("ab", model.GetText());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("abc", model.GetText());
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ab", model.GetText());
  EXPECT_EQ(2U, model.GetCursorPosition());
  // Continous backspaces are treated as one edit.
  EXPECT_TRUE(model.Backspace());
  EXPECT_TRUE(model.Backspace());
  EXPECT_STR_EQ("", model.GetText());
  // Extra backspace shouldn't affect the history.
  EXPECT_FALSE(model.Backspace());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ab", model.GetText());
  EXPECT_EQ(2U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("abc", model.GetText());
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("a", model.GetText());
  EXPECT_EQ(1U, model.GetCursorPosition());

  // Clear history
  model.ClearEditHistory();
  EXPECT_FALSE(model.Undo());
  EXPECT_FALSE(model.Redo());
  EXPECT_STR_EQ("a", model.GetText());
  EXPECT_EQ(1U, model.GetCursorPosition());

  // Delete ===============================
  model.SetText(ASCIIToUTF16("ABCDE"));
  model.ClearEditHistory();
  gfx::SelectionModel sel(2);
  model.MoveCursorTo(sel);
  EXPECT_TRUE(model.Delete());
  EXPECT_STR_EQ("ABDE", model.GetText());
  model.MoveCursorLeft(gfx::LINE_BREAK, false);
  EXPECT_TRUE(model.Delete());
  EXPECT_STR_EQ("BDE", model.GetText());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABDE", model.GetText());
  EXPECT_EQ(0U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABCDE", model.GetText());
  EXPECT_EQ(2U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ABDE", model.GetText());
  EXPECT_EQ(2U, model.GetCursorPosition());
  // Continous deletes are treated as one edit.
  EXPECT_TRUE(model.Delete());
  EXPECT_TRUE(model.Delete());
  EXPECT_STR_EQ("AB", model.GetText());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABDE", model.GetText());
  EXPECT_EQ(2U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("AB", model.GetText());
  EXPECT_EQ(2U, model.GetCursorPosition());
}

TEST_F(TextfieldViewsModelTest, UndoRedo_SetText) {
  // This is to test the undo/redo behavior of omnibox.
  TextfieldViewsModel model(NULL);
  model.InsertChar('w');
  EXPECT_STR_EQ("w", model.GetText());
  EXPECT_EQ(1U, model.GetCursorPosition());
  model.SetText(ASCIIToUTF16("www.google.com"));
  EXPECT_EQ(1U, model.GetCursorPosition());
  EXPECT_STR_EQ("www.google.com", model.GetText());
  model.SelectRange(ui::Range(14, 1));
  model.InsertChar('w');
  EXPECT_STR_EQ("ww", model.GetText());
  model.SetText(ASCIIToUTF16("www.google.com"));
  model.SelectRange(ui::Range(14, 2));
  model.InsertChar('w');
  EXPECT_STR_EQ("www", model.GetText());
  model.SetText(ASCIIToUTF16("www.google.com"));
  model.SelectRange(ui::Range(14, 3));
  model.InsertChar('.');
  EXPECT_STR_EQ("www.", model.GetText());
  model.SetText(ASCIIToUTF16("www.google.com"));
  model.SelectRange(ui::Range(14, 4));
  model.InsertChar('y');
  EXPECT_STR_EQ("www.y", model.GetText());
  model.SetText(ASCIIToUTF16("www.youtube.com"));
  EXPECT_STR_EQ("www.youtube.com", model.GetText());
  EXPECT_EQ(5U, model.GetCursorPosition());

  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("www.google.com", model.GetText());
  EXPECT_EQ(4U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("www.google.com", model.GetText());
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("www.google.com", model.GetText());
  EXPECT_EQ(2U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("www.google.com", model.GetText());
  EXPECT_EQ(1U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("", model.GetText());
  EXPECT_EQ(0U, model.GetCursorPosition());
  EXPECT_FALSE(model.Undo());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("www.google.com", model.GetText());
  EXPECT_EQ(1U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("www.google.com", model.GetText());
  EXPECT_EQ(2U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("www.google.com", model.GetText());
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("www.google.com", model.GetText());
  EXPECT_EQ(4U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("www.youtube.com", model.GetText());
  EXPECT_EQ(5U, model.GetCursorPosition());
  EXPECT_FALSE(model.Redo());
}

#if defined(USE_AURA) && defined(OS_LINUX)
// This can be re-enabled when aura on linux has clipboard support.
// http://crbug.com/97845
#define MAYBE_UndoRedo_CutCopyPasteTest DISABLED_UndoRedo_CutCopyPasteTest
#else
#define MAYBE_UndoRedo_CutCopyPasteTest UndoRedo_CutCopyPasteTest
#endif
TEST_F(TextfieldViewsModelTest, MAYBE_UndoRedo_CutCopyPasteTest) {
  TextfieldViewsModel model(NULL);
  model.SetText(ASCIIToUTF16("ABCDE"));
  EXPECT_FALSE(model.Redo());  // nothing to redo
  // Cut
  model.SelectRange(ui::Range(1, 3));
  model.Cut();
  EXPECT_STR_EQ("ADE", model.GetText());
  EXPECT_EQ(1U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABCDE", model.GetText());
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("", model.GetText());
  EXPECT_EQ(0U, model.GetCursorPosition());
  EXPECT_FALSE(model.Undo());  // no more undo
  EXPECT_STR_EQ("", model.GetText());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ABCDE", model.GetText());
  EXPECT_EQ(0U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ADE", model.GetText());
  EXPECT_EQ(1U, model.GetCursorPosition());
  EXPECT_FALSE(model.Redo());  // no more redo
  EXPECT_STR_EQ("ADE", model.GetText());

  model.Paste();
  model.Paste();
  model.Paste();
  EXPECT_STR_EQ("ABCBCBCDE", model.GetText());
  EXPECT_EQ(7U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABCBCDE", model.GetText());
  EXPECT_EQ(5U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABCDE", model.GetText());
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ADE", model.GetText());
  EXPECT_EQ(1U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABCDE", model.GetText());
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("", model.GetText());
  EXPECT_EQ(0U, model.GetCursorPosition());
  EXPECT_FALSE(model.Undo());
  EXPECT_STR_EQ("", model.GetText());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ABCDE", model.GetText());  // Redoing SetText
  EXPECT_EQ(0U, model.GetCursorPosition());

  // Redo
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ADE", model.GetText());
  EXPECT_EQ(1U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ABCDE", model.GetText());
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ABCBCDE", model.GetText());
  EXPECT_EQ(5U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ABCBCBCDE", model.GetText());
  EXPECT_EQ(7U, model.GetCursorPosition());
  EXPECT_FALSE(model.Redo());

  // with SelectRange
  model.SelectRange(ui::Range(1, 3));
  EXPECT_TRUE(model.Cut());
  EXPECT_STR_EQ("ABCBCDE", model.GetText());
  EXPECT_EQ(1U, model.GetCursorPosition());
  model.SelectRange(ui::Range(1, 1));
  EXPECT_FALSE(model.Cut());
  model.MoveCursorRight(gfx::LINE_BREAK, false);
  EXPECT_TRUE(model.Paste());
  EXPECT_STR_EQ("ABCBCDEBC", model.GetText());
  EXPECT_EQ(9U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABCBCDE", model.GetText());
  EXPECT_EQ(7U, model.GetCursorPosition());
  // empty cut shouldn't create an edit.
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABCBCBCDE", model.GetText());
  EXPECT_EQ(3U, model.GetCursorPosition());

  // Copy
  ResetModel(&model);
  model.SetText(ASCIIToUTF16("12345"));
  EXPECT_STR_EQ("12345", model.GetText());
  EXPECT_EQ(0U, model.GetCursorPosition());
  model.SelectRange(ui::Range(1, 3));
  model.Copy();  // Copy "23"
  EXPECT_STR_EQ("12345", model.GetText());
  EXPECT_EQ(3U, model.GetCursorPosition());
  model.Paste();  // Paste "23" into "23".
  EXPECT_STR_EQ("12345", model.GetText());
  EXPECT_EQ(3U, model.GetCursorPosition());
  model.Paste();
  EXPECT_STR_EQ("1232345", model.GetText());
  EXPECT_EQ(5U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("12345", model.GetText());
  EXPECT_EQ(3U, model.GetCursorPosition());
  // TODO(oshima): We need to change the return type from bool to enum.
  EXPECT_FALSE(model.Undo());  // No text change.
  EXPECT_STR_EQ("12345", model.GetText());
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("", model.GetText());
  EXPECT_FALSE(model.Undo());
  // Redo
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("12345", model.GetText());
  EXPECT_EQ(0U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("12345", model.GetText());  // For 1st paste
  EXPECT_EQ(3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("1232345", model.GetText());
  EXPECT_EQ(5U, model.GetCursorPosition());
  EXPECT_FALSE(model.Redo());
  EXPECT_STR_EQ("1232345", model.GetText());

  // Test using SelectRange
  model.SelectRange(ui::Range(1, 3));
  model.Copy();
  EXPECT_STR_EQ("1232345", model.GetText());
  model.MoveCursorRight(gfx::LINE_BREAK, false);
  EXPECT_TRUE(model.Paste());
  EXPECT_STR_EQ("123234523", model.GetText());
  EXPECT_EQ(9U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("1232345", model.GetText());
  EXPECT_EQ(7U, model.GetCursorPosition());
}

TEST_F(TextfieldViewsModelTest, UndoRedo_CursorTest) {
  TextfieldViewsModel model(NULL);
  model.InsertChar('a');
  model.MoveCursorLeft(gfx::CHARACTER_BREAK, false);
  model.MoveCursorRight(gfx::CHARACTER_BREAK, false);
  model.InsertChar('b');
  // Moving the cursor shouldn't create a new edit.
  EXPECT_STR_EQ("ab", model.GetText());
  EXPECT_FALSE(model.Redo());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("", model.GetText());
  EXPECT_FALSE(model.Undo());
  EXPECT_STR_EQ("", model.GetText());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ab", model.GetText());
  EXPECT_EQ(2U, model.GetCursorPosition());
  EXPECT_FALSE(model.Redo());
}

void RunInsertReplaceTest(TextfieldViewsModel& model) {
  gfx::SelectionModel sel;
  model.GetSelectionModel(&sel);
  bool reverse = (sel.selection_start() > sel.selection_end());

  model.InsertChar('1');
  model.InsertChar('2');
  model.InsertChar('3');
  EXPECT_STR_EQ("a123d", model.GetText());
  EXPECT_EQ(4U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("abcd", model.GetText());
  EXPECT_EQ(reverse ? 1U : 3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("", model.GetText());
  EXPECT_EQ(0U, model.GetCursorPosition());
  EXPECT_FALSE(model.Undo());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("abcd", model.GetText());
  EXPECT_EQ(0U, model.GetCursorPosition());  // By SetText
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("a123d", model.GetText());
  EXPECT_EQ(4U, model.GetCursorPosition());
  EXPECT_FALSE(model.Redo());
}

void RunOverwriteReplaceTest(TextfieldViewsModel& model) {
  gfx::SelectionModel sel;
  model.GetSelectionModel(&sel);
  bool reverse = (sel.selection_start() > sel.selection_end());

  model.ReplaceChar('1');
  model.ReplaceChar('2');
  model.ReplaceChar('3');
  model.ReplaceChar('4');
  EXPECT_STR_EQ("a1234", model.GetText());
  EXPECT_EQ(5U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("abcd", model.GetText());
  EXPECT_EQ(reverse ? 1U : 3U, model.GetCursorPosition());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("", model.GetText());
  EXPECT_EQ(0U, model.GetCursorPosition());
  EXPECT_FALSE(model.Undo());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("abcd", model.GetText());
  EXPECT_EQ(0U, model.GetCursorPosition());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("a1234", model.GetText());
  EXPECT_EQ(5U, model.GetCursorPosition());
  EXPECT_FALSE(model.Redo());
}

TEST_F(TextfieldViewsModelTest, UndoRedo_ReplaceTest) {
  // By Cursor
  {
    SCOPED_TRACE("forward & insert by cursor");
    TextfieldViewsModel model(NULL);
    model.SetText(ASCIIToUTF16("abcd"));
    model.SelectRange(ui::Range(1, 3));
    RunInsertReplaceTest(model);
  }
  {
    SCOPED_TRACE("backward & insert by cursor");
    TextfieldViewsModel model(NULL);
    model.SetText(ASCIIToUTF16("abcd"));
    model.SelectRange(ui::Range(3, 1));
    RunInsertReplaceTest(model);
  }
  {
    SCOPED_TRACE("forward & overwrite by cursor");
    TextfieldViewsModel model(NULL);
    model.SetText(ASCIIToUTF16("abcd"));
    model.SelectRange(ui::Range(1, 3));
    RunOverwriteReplaceTest(model);
  }
  {
    SCOPED_TRACE("backward & overwrite by cursor");
    TextfieldViewsModel model(NULL);
    model.SetText(ASCIIToUTF16("abcd"));
    model.SelectRange(ui::Range(3, 1));
    RunOverwriteReplaceTest(model);
  }
  // By SelectRange API
  {
    SCOPED_TRACE("forward & insert by SelectRange");
    TextfieldViewsModel model(NULL);
    model.SetText(ASCIIToUTF16("abcd"));
    model.SelectRange(ui::Range(1, 3));
    RunInsertReplaceTest(model);
  }
  {
    SCOPED_TRACE("backward & insert by SelectRange");
    TextfieldViewsModel model(NULL);
    model.SetText(ASCIIToUTF16("abcd"));
    model.SelectRange(ui::Range(3, 1));
    RunInsertReplaceTest(model);
  }
  {
    SCOPED_TRACE("forward & overwrite by SelectRange");
    TextfieldViewsModel model(NULL);
    model.SetText(ASCIIToUTF16("abcd"));
    model.SelectRange(ui::Range(1, 3));
    RunOverwriteReplaceTest(model);
  }
  {
    SCOPED_TRACE("backward & overwrite by SelectRange");
    TextfieldViewsModel model(NULL);
    model.SetText(ASCIIToUTF16("abcd"));
    model.SelectRange(ui::Range(3, 1));
    RunOverwriteReplaceTest(model);
  }
}

TEST_F(TextfieldViewsModelTest, UndoRedo_CompositionText) {
  TextfieldViewsModel model(NULL);

  ui::CompositionText composition;
  composition.text = ASCIIToUTF16("abc");
  composition.underlines.push_back(ui::CompositionUnderline(0, 3, 0, false));
  composition.selection = ui::Range(2, 3);

  model.SetText(ASCIIToUTF16("ABCDE"));
  model.MoveCursorRight(gfx::LINE_BREAK, false);
  model.InsertChar('x');
  EXPECT_STR_EQ("ABCDEx", model.GetText());
  EXPECT_TRUE(model.Undo());  // set composition should forget undone edit.
  model.SetCompositionText(composition);
  EXPECT_TRUE(model.HasCompositionText());
  EXPECT_TRUE(model.HasSelection());
  EXPECT_STR_EQ("ABCDEabc", model.GetText());

  // Accepting composition
  model.ConfirmCompositionText();
  EXPECT_STR_EQ("ABCDEabc", model.GetText());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABCDE", model.GetText());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("", model.GetText());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ABCDE", model.GetText());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ABCDEabc", model.GetText());
  EXPECT_FALSE(model.Redo());

  // Canceling composition
  model.MoveCursorLeft(gfx::LINE_BREAK, false);
  model.SetCompositionText(composition);
  EXPECT_STR_EQ("abcABCDEabc", model.GetText());
  model.CancelCompositionText();
  EXPECT_STR_EQ("ABCDEabc", model.GetText());
  EXPECT_FALSE(model.Redo());
  EXPECT_STR_EQ("ABCDEabc", model.GetText());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABCDE", model.GetText());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ABCDEabc", model.GetText());
  EXPECT_FALSE(model.Redo());

  // SetText with the same text as the result.
  ResetModel(&model);
  model.SetText(ASCIIToUTF16("ABCDE"));
  model.MoveCursorRight(gfx::LINE_BREAK, false);
  model.SetCompositionText(composition);
  EXPECT_STR_EQ("ABCDEabc", model.GetText());
  model.SetText(ASCIIToUTF16("ABCDEabc"));
  EXPECT_STR_EQ("ABCDEabc", model.GetText());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABCDE", model.GetText());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("ABCDEabc", model.GetText());
  EXPECT_FALSE(model.Redo());

  // SetText with the different text than the result should not
  // remember composition text.
  ResetModel(&model);
  model.SetText(ASCIIToUTF16("ABCDE"));
  model.MoveCursorRight(gfx::LINE_BREAK, false);
  model.SetCompositionText(composition);
  EXPECT_STR_EQ("ABCDEabc", model.GetText());
  model.SetText(ASCIIToUTF16("1234"));
  EXPECT_STR_EQ("1234", model.GetText());
  EXPECT_TRUE(model.Undo());
  EXPECT_STR_EQ("ABCDE", model.GetText());
  EXPECT_TRUE(model.Redo());
  EXPECT_STR_EQ("1234", model.GetText());
  EXPECT_FALSE(model.Redo());

  // TODO(oshima): We need MockInputMethod to test the behavior with IME.
}

}  // namespace views
