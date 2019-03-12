// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/editable_combobox/editable_combobox.h"

#include <memory>
#include <set>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/models/simple_combobox_model.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/test/event_generator.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/editable_combobox/editable_combobox_listener.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/test/menu_test_utils.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_utils.h"

namespace views {
namespace {

using base::ASCIIToUTF16;
using views::test::WaitForMenuClosureAnimation;

class DummyListener : public EditableComboboxListener {
 public:
  DummyListener() {}
  ~DummyListener() override = default;
  void OnContentChanged(EditableCombobox* editable_combobox) override {
    change_count_++;
  }

  int change_count() const { return change_count_; }

 private:
  int change_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(DummyListener);
};

class EditableComboboxTest : public ViewsTestBase {
 public:
  EditableComboboxTest() { views::test::DisableMenuClosureAnimations(); }

  void TearDown() override;

  // Initializes the combobox with the given number of items.
  void InitEditableCombobox(int item_count = 8,
                            bool filter_on_edit = false,
                            bool show_on_empty = true);

  // Initializes the combobox with the given items.
  void InitEditableCombobox(const std::vector<base::string16>& items,
                            bool filter_on_edit,
                            bool show_on_empty = true);

  // Initializes the widget where the combobox and the dummy control live.
  void InitWidget();

 protected:
  bool IsMenuOpen();
  void PerformMouseEvent(const gfx::Point& point, ui::EventType type);
  void PerformClick(const gfx::Point& point);
  void SendKeyEvent(ui::KeyboardCode key_code,
                    bool alt = false,
                    bool shift = false,
                    bool ctrl_cmd = false);

  // The widget where the control will appear.
  Widget* widget_ = nullptr;

  // |combobox_| and |dummy_focusable_view_| are allocated in
  // |InitEditableCombobox| and then owned by |widget_|.
  EditableCombobox* combobox_ = nullptr;
  View* dummy_focusable_view_ = nullptr;

  // Listener for our EditableCombobox.
  std::unique_ptr<DummyListener> listener_;

  std::unique_ptr<ui::test::EventGenerator> event_generator_;

 private:
  DISALLOW_COPY_AND_ASSIGN(EditableComboboxTest);
};

void EditableComboboxTest::TearDown() {
  if (widget_)
    widget_->Close();
  ViewsTestBase::TearDown();
}

// Initializes the combobox with the given number of items.
void EditableComboboxTest::InitEditableCombobox(const int item_count,
                                                const bool filter_on_edit,
                                                const bool show_on_empty) {
  std::vector<base::string16> items;
  for (int i = 0; i < item_count; ++i) {
    items.push_back(ASCIIToUTF16(base::StringPrintf("item[%i]", i)));
  }
  InitEditableCombobox(items, filter_on_edit, show_on_empty);
}

// Initializes the combobox with the given items.
void EditableComboboxTest::InitEditableCombobox(
    const std::vector<base::string16>& items,
    const bool filter_on_edit,
    const bool show_on_empty) {
  combobox_ =
      new EditableCombobox(std::make_unique<ui::SimpleComboboxModel>(items),
                           filter_on_edit, show_on_empty);
  listener_ = std::make_unique<DummyListener>();
  combobox_->set_listener(listener_.get());
  combobox_->set_id(1);
  dummy_focusable_view_ = new View();
  dummy_focusable_view_->SetFocusBehavior(View::FocusBehavior::ALWAYS);
  dummy_focusable_view_->set_id(2);

  InitWidget();
  combobox_->RequestFocus();
  combobox_->SizeToPreferredSize();
}

// Initializes the widget where the combobox and the dummy control live.
void EditableComboboxTest::InitWidget() {
  widget_ = new Widget();
  Widget::InitParams params =
      CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.bounds = gfx::Rect(200, 200, 200, 200);
  widget_->Init(params);
  View* container = new View();
  widget_->SetContentsView(container);
  container->AddChildView(combobox_);
  container->AddChildView(dummy_focusable_view_);
  widget_->Show();

  event_generator_ =
      std::make_unique<ui::test::EventGenerator>(GetRootWindow(widget_));
  event_generator_->set_target(ui::test::EventGenerator::Target::WINDOW);
}

bool EditableComboboxTest::IsMenuOpen() {
  return combobox_->GetMenuRunnerForTest() &&
         combobox_->GetMenuRunnerForTest()->IsRunning();
}

void EditableComboboxTest::PerformMouseEvent(const gfx::Point& point,
                                             const ui::EventType type) {
  ui::MouseEvent event =
      ui::MouseEvent(type, point, point, ui::EventTimeForNow(),
                     ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON);
  Widget* widget = widget_;
  // If a menu is open, we send the click event to the child widget where the
  // menu is shown. That child widget is the MenuHost object created inside
  // EditableCombobox's MenuRunner to host the menu items.
  if (combobox_->GetMenuRunnerForTest()) {
    std::set<Widget*> child_widgets;
    Widget::GetAllOwnedWidgets(widget->GetNativeView(), &child_widgets);
    ASSERT_EQ(1UL, child_widgets.size());
    widget = *child_widgets.begin();
  }
  widget->OnMouseEvent(&event);
}

void EditableComboboxTest::PerformClick(const gfx::Point& point) {
  PerformMouseEvent(point, ui::ET_MOUSE_PRESSED);
  PerformMouseEvent(point, ui::ET_MOUSE_RELEASED);
}

void EditableComboboxTest::SendKeyEvent(ui::KeyboardCode key_code,
                                        const bool alt,
                                        const bool shift,
                                        const bool ctrl_cmd) {
#if defined(OS_MACOSX)
  bool command = ctrl_cmd;
  bool control = false;
#else
  bool command = false;
  bool control = ctrl_cmd;
#endif

  int flags = (shift ? ui::EF_SHIFT_DOWN : 0) |
              (control ? ui::EF_CONTROL_DOWN : 0) |
              (alt ? ui::EF_ALT_DOWN : 0) | (command ? ui::EF_COMMAND_DOWN : 0);

  event_generator_->PressKey(key_code, flags);
}

TEST_F(EditableComboboxTest, FocusOnTextfieldOpensMenu) {
  InitEditableCombobox();
  EXPECT_FALSE(IsMenuOpen());
  combobox_->GetTextfieldForTest()->RequestFocus();
  EXPECT_TRUE(IsMenuOpen());
}

TEST_F(EditableComboboxTest, TabMovesToOtherViewAndClosesMenu) {
  InitEditableCombobox();
  combobox_->GetTextfieldForTest()->RequestFocus();
  EXPECT_TRUE(IsMenuOpen());
  EXPECT_TRUE(combobox_->GetTextfieldForTest()->HasFocus());
  SendKeyEvent(ui::VKEY_TAB);
  EXPECT_FALSE(combobox_->GetTextfieldForTest()->HasFocus());
  EXPECT_TRUE(dummy_focusable_view_->HasFocus());
  EXPECT_FALSE(IsMenuOpen());
}

TEST_F(EditableComboboxTest, LeftOrRightKeysMoveInTextfield) {
  InitEditableCombobox();
  combobox_->GetTextfieldForTest()->RequestFocus();
  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_C);
  SendKeyEvent(ui::VKEY_E);
  SendKeyEvent(ui::VKEY_LEFT);
  SendKeyEvent(ui::VKEY_LEFT);
  SendKeyEvent(ui::VKEY_B);
  SendKeyEvent(ui::VKEY_RIGHT);
  SendKeyEvent(ui::VKEY_D);
  EXPECT_EQ(ASCIIToUTF16("abcde"), combobox_->GetText());
}

TEST_F(EditableComboboxTest, UpOrDownKeysMoveInMenu) {
  InitEditableCombobox();
  combobox_->GetTextfieldForTest()->RequestFocus();
  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_B);
  SendKeyEvent(ui::VKEY_C);
  SendKeyEvent(ui::VKEY_DOWN);
  SendKeyEvent(ui::VKEY_DOWN);
  SendKeyEvent(ui::VKEY_DOWN);
  SendKeyEvent(ui::VKEY_UP);
  SendKeyEvent(ui::VKEY_RETURN);
  WaitForMenuClosureAnimation();
  EXPECT_EQ(ASCIIToUTF16("item[1]"), combobox_->GetText());
}

TEST_F(EditableComboboxTest, EndOrHomeMovesToBeginningOrEndOfText) {
  InitEditableCombobox();
  combobox_->GetTextfieldForTest()->RequestFocus();

  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_B);
  SendKeyEvent(ui::VKEY_C);
  SendKeyEvent(ui::VKEY_HOME);
  SendKeyEvent(ui::VKEY_X);
  SendKeyEvent(ui::VKEY_END);
  SendKeyEvent(ui::VKEY_Y);
  EXPECT_EQ(ASCIIToUTF16("xabcy"), combobox_->GetText());
}

#if defined(OS_MACOSX)

TEST_F(EditableComboboxTest, AltLeftOrRightMovesToNextWords) {
  InitEditableCombobox();
  combobox_->GetTextfieldForTest()->RequestFocus();

  combobox_->SetTextForTest(ASCIIToUTF16("foo bar foobar"));
  SendKeyEvent(ui::VKEY_LEFT, /*alt=*/true, /*shift=*/false,
               /*ctrl_cmd=*/false);
  SendKeyEvent(ui::VKEY_LEFT, /*alt=*/true, /*shift=*/false,
               /*ctrl_cmd=*/false);
  SendKeyEvent(ui::VKEY_X);
  SendKeyEvent(ui::VKEY_RIGHT, /*alt=*/true, /*shift=*/false,
               /*ctrl_cmd=*/false);
  SendKeyEvent(ui::VKEY_Y);
  EXPECT_EQ(ASCIIToUTF16("foo xbary foobar"), combobox_->GetText());
}

TEST_F(EditableComboboxTest, CtrlLeftOrRightMovesToBeginningOrEndOfText) {
  InitEditableCombobox();
  combobox_->GetTextfieldForTest()->RequestFocus();

  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_B);
  SendKeyEvent(ui::VKEY_C);
  SendKeyEvent(ui::VKEY_LEFT, /*alt=*/false, /*shift=*/false,
               /*ctrl_cmd=*/true);
  SendKeyEvent(ui::VKEY_X);
  SendKeyEvent(ui::VKEY_RIGHT, /*alt=*/false, /*shift=*/false,
               /*ctrl_cmd=*/true);
  SendKeyEvent(ui::VKEY_Y);
  EXPECT_EQ(ASCIIToUTF16("xabcy"), combobox_->GetText());
}

#else

TEST_F(EditableComboboxTest, AltLeftOrRightDoesNothing) {
  InitEditableCombobox();
  combobox_->GetTextfieldForTest()->RequestFocus();

  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_B);
  SendKeyEvent(ui::VKEY_C);
  SendKeyEvent(ui::VKEY_LEFT, /*alt=*/true, /*shift=*/false,
               /*ctrl_cmd=*/false);
  SendKeyEvent(ui::VKEY_X);
  SendKeyEvent(ui::VKEY_LEFT);
  SendKeyEvent(ui::VKEY_RIGHT, /*alt=*/true, /*shift=*/false,
               /*ctrl_cmd=*/false);
  SendKeyEvent(ui::VKEY_Y);
  EXPECT_EQ(ASCIIToUTF16("abcyx"), combobox_->GetText());
}

TEST_F(EditableComboboxTest, CtrlLeftOrRightMovesToNextWords) {
  InitEditableCombobox();
  combobox_->GetTextfieldForTest()->RequestFocus();

  combobox_->SetTextForTest(ASCIIToUTF16("foo bar foobar"));
  SendKeyEvent(ui::VKEY_LEFT, /*alt=*/false, /*shift=*/false,
               /*ctrl_cmd=*/true);
  SendKeyEvent(ui::VKEY_LEFT, /*alt=*/false, /*shift=*/false,
               /*ctrl_cmd=*/true);
  SendKeyEvent(ui::VKEY_X);
  SendKeyEvent(ui::VKEY_RIGHT, /*alt=*/false, /*shift=*/false,
               /*ctrl_cmd=*/true);
  SendKeyEvent(ui::VKEY_Y);
#if defined(OS_WIN)
  // Matches Windows-specific logic in
  // RenderTextHarfBuzz::AdjacentWordSelectionModel.
  EXPECT_EQ(ASCIIToUTF16("foo xbar yfoobar"), combobox_->GetText());
#else
  EXPECT_EQ(ASCIIToUTF16("foo xbary foobar"), combobox_->GetText());
#endif
}

#endif

TEST_F(EditableComboboxTest, ShiftLeftOrRightSelectsCharInTextfield) {
  InitEditableCombobox();
  combobox_->GetTextfieldForTest()->RequestFocus();

  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_B);
  SendKeyEvent(ui::VKEY_C);
  SendKeyEvent(ui::VKEY_LEFT, /*alt=*/false, /*shift=*/true,
               /*ctrl_cmd=*/false);
  SendKeyEvent(ui::VKEY_X);
  SendKeyEvent(ui::VKEY_LEFT);
  SendKeyEvent(ui::VKEY_LEFT);
  SendKeyEvent(ui::VKEY_RIGHT, /*alt=*/false, /*shift=*/true,
               /*ctrl_cmd=*/false);
  SendKeyEvent(ui::VKEY_Y);
  EXPECT_EQ(ASCIIToUTF16("ayx"), combobox_->GetText());
}

TEST_F(EditableComboboxTest, EnterClosesMenuWhileSelectingHighlightedMenuItem) {
  InitEditableCombobox();
  combobox_->GetTextfieldForTest()->RequestFocus();
  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_TRUE(IsMenuOpen());
  SendKeyEvent(ui::VKEY_RETURN);
  WaitForMenuClosureAnimation();
  EXPECT_FALSE(IsMenuOpen());
  EXPECT_EQ(ASCIIToUTF16("item[0]"), combobox_->GetText());
}

TEST_F(EditableComboboxTest, F4ClosesMenuWhileSelectingHighlightedMenuItem) {
  InitEditableCombobox();
  combobox_->GetTextfieldForTest()->RequestFocus();
  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_TRUE(IsMenuOpen());
  SendKeyEvent(ui::VKEY_F4);
  WaitForMenuClosureAnimation();
  EXPECT_FALSE(IsMenuOpen());
  EXPECT_EQ(ASCIIToUTF16("item[0]"), combobox_->GetText());
}

TEST_F(EditableComboboxTest, EscClosesMenuWithoutSelectingHighlightedMenuItem) {
  InitEditableCombobox();
  combobox_->GetTextfieldForTest()->RequestFocus();
  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_DOWN);
  EXPECT_TRUE(IsMenuOpen());
  SendKeyEvent(ui::VKEY_ESCAPE);
  EXPECT_FALSE(IsMenuOpen());
  EXPECT_EQ(ASCIIToUTF16("a"), combobox_->GetText());
}

TEST_F(EditableComboboxTest, TypingInTextfieldUnhighlightsMenuItem) {
  InitEditableCombobox();
  combobox_->GetTextfieldForTest()->RequestFocus();
  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_B);
  SendKeyEvent(ui::VKEY_DOWN);
  SendKeyEvent(ui::VKEY_C);
  SendKeyEvent(ui::VKEY_RETURN);
  EXPECT_EQ(ASCIIToUTF16("abc"), combobox_->GetText());
}

TEST_F(EditableComboboxTest, ClickOnMenuItemSelectsItAndClosesMenu) {
  InitEditableCombobox();
  combobox_->GetTextfieldForTest()->RequestFocus();

  const gfx::Point middle_of_first_item(
      combobox_->x() + combobox_->width() / 2,
      combobox_->y() + combobox_->height() + 1);
  EXPECT_TRUE(IsMenuOpen());
  PerformClick(middle_of_first_item);
  WaitForMenuClosureAnimation();
  EXPECT_FALSE(IsMenuOpen());
  EXPECT_EQ(ASCIIToUTF16("item[0]"), combobox_->GetText());
}

// This is different from the regular read-only Combobox, where SPACE
// opens/closes the menu.
TEST_F(EditableComboboxTest, SpaceIsReflectedInTextfield) {
  InitEditableCombobox();
  combobox_->GetTextfieldForTest()->RequestFocus();
  SendKeyEvent(ui::VKEY_A);
  SendKeyEvent(ui::VKEY_SPACE);
  SendKeyEvent(ui::VKEY_SPACE);
  SendKeyEvent(ui::VKEY_B);
  EXPECT_EQ(ASCIIToUTF16("a  b"), combobox_->GetText());
}

// We test that the menu can adapt to content change by using an
// EditableCombobox with |filter_on_edit| set to true, which will change the
// menu's content as the user types.
TEST_F(EditableComboboxTest, MenuCanAdaptToContentChange) {
  std::vector<base::string16> items = {ASCIIToUTF16("abc"), ASCIIToUTF16("abd"),
                                       ASCIIToUTF16("bac"),
                                       ASCIIToUTF16("bad")};
  InitEditableCombobox(items, /*filter_on_edit=*/true);
  combobox_->GetTextfieldForTest()->RequestFocus();

  SendKeyEvent(ui::VKEY_DOWN);
  SendKeyEvent(ui::VKEY_RETURN);
  WaitForMenuClosureAnimation();
  EXPECT_EQ(ASCIIToUTF16("abc"), combobox_->GetText());

  SendKeyEvent(ui::VKEY_BACK);
  SendKeyEvent(ui::VKEY_BACK);
  SendKeyEvent(ui::VKEY_BACK);
  MenuRunner* menu_runner1 = combobox_->GetMenuRunnerForTest();
  SendKeyEvent(ui::VKEY_B);
  MenuRunner* menu_runner2 = combobox_->GetMenuRunnerForTest();
  SendKeyEvent(ui::VKEY_DOWN);
  SendKeyEvent(ui::VKEY_RETURN);
  WaitForMenuClosureAnimation();
  EXPECT_EQ(ASCIIToUTF16("bac"), combobox_->GetText());

  // Even though the items shown change, the menu runner shouldn't have been
  // reset, otherwise there could be a flicker when the menu closes and reopens.
  EXPECT_EQ(menu_runner1, menu_runner2);
}

TEST_F(EditableComboboxTest, RefocusingReopensMenuBasedOnLatestContent) {
  std::vector<base::string16> items = {ASCIIToUTF16("abc"), ASCIIToUTF16("abd"),
                                       ASCIIToUTF16("bac"), ASCIIToUTF16("bad"),
                                       ASCIIToUTF16("bac2")};
  InitEditableCombobox(items, /*filter_on_edit=*/true);
  combobox_->GetTextfieldForTest()->RequestFocus();

  SendKeyEvent(ui::VKEY_B);
  ASSERT_EQ(3, combobox_->GetItemCountForTest());

  SendKeyEvent(ui::VKEY_DOWN);
  SendKeyEvent(ui::VKEY_RETURN);
  WaitForMenuClosureAnimation();
  EXPECT_FALSE(IsMenuOpen());
  EXPECT_EQ(ASCIIToUTF16("bac"), combobox_->GetText());

  // Blur then focus to make the menu reopen. It should only show completions of
  // "bac", the selected item, instead of showing completions of "b", what we
  // had typed.
  dummy_focusable_view_->RequestFocus();
  combobox_->GetTextfieldForTest()->RequestFocus();
  EXPECT_TRUE(IsMenuOpen());
  ASSERT_EQ(2, combobox_->GetItemCountForTest());
}

TEST_F(EditableComboboxTest, GetItemsWithoutFiltering) {
  std::vector<base::string16> items = {ASCIIToUTF16("item0"),
                                       ASCIIToUTF16("item1")};
  InitEditableCombobox(items, /*filter_on_edit=*/false, /*show_on_empty=*/true);

  combobox_->SetTextForTest(ASCIIToUTF16("z"));
  ASSERT_EQ(2, combobox_->GetItemCountForTest());
  ASSERT_EQ(ASCIIToUTF16("item0"), combobox_->GetItemForTest(0));
  ASSERT_EQ(ASCIIToUTF16("item1"), combobox_->GetItemForTest(1));
}

TEST_F(EditableComboboxTest, FilteringEffectOnGetItems) {
  std::vector<base::string16> items = {ASCIIToUTF16("abc"), ASCIIToUTF16("abd"),
                                       ASCIIToUTF16("bac"),
                                       ASCIIToUTF16("bad")};
  InitEditableCombobox(items, /*filter_on_edit=*/true, /*show_on_empty=*/true);

  ASSERT_EQ(4, combobox_->GetItemCountForTest());
  ASSERT_EQ(ASCIIToUTF16("abc"), combobox_->GetItemForTest(0));
  ASSERT_EQ(ASCIIToUTF16("abd"), combobox_->GetItemForTest(1));
  ASSERT_EQ(ASCIIToUTF16("bac"), combobox_->GetItemForTest(2));
  ASSERT_EQ(ASCIIToUTF16("bad"), combobox_->GetItemForTest(3));

  combobox_->SetTextForTest(ASCIIToUTF16("b"));
  ASSERT_EQ(2, combobox_->GetItemCountForTest());
  ASSERT_EQ(ASCIIToUTF16("bac"), combobox_->GetItemForTest(0));
  ASSERT_EQ(ASCIIToUTF16("bad"), combobox_->GetItemForTest(1));

  combobox_->SetTextForTest(ASCIIToUTF16("bc"));
  ASSERT_EQ(0, combobox_->GetItemCountForTest());

  combobox_->SetTextForTest(base::string16());
  ASSERT_EQ(4, combobox_->GetItemCountForTest());
  ASSERT_EQ(ASCIIToUTF16("abc"), combobox_->GetItemForTest(0));
  ASSERT_EQ(ASCIIToUTF16("abd"), combobox_->GetItemForTest(1));
  ASSERT_EQ(ASCIIToUTF16("bac"), combobox_->GetItemForTest(2));
  ASSERT_EQ(ASCIIToUTF16("bad"), combobox_->GetItemForTest(3));
}

TEST_F(EditableComboboxTest, FilteringWithMismatchedCase) {
  std::vector<base::string16> items = {
      ASCIIToUTF16("AbCd"), ASCIIToUTF16("aBcD"), ASCIIToUTF16("xyz")};
  InitEditableCombobox(items, /*filter_on_edit=*/true, /*show_on_empty=*/true);

  ASSERT_EQ(3, combobox_->GetItemCountForTest());
  ASSERT_EQ(ASCIIToUTF16("AbCd"), combobox_->GetItemForTest(0));
  ASSERT_EQ(ASCIIToUTF16("aBcD"), combobox_->GetItemForTest(1));
  ASSERT_EQ(ASCIIToUTF16("xyz"), combobox_->GetItemForTest(2));

  combobox_->SetTextForTest(ASCIIToUTF16("abcd"));
  ASSERT_EQ(2, combobox_->GetItemCountForTest());
  ASSERT_EQ(ASCIIToUTF16("AbCd"), combobox_->GetItemForTest(0));
  ASSERT_EQ(ASCIIToUTF16("aBcD"), combobox_->GetItemForTest(1));

  combobox_->SetTextForTest(ASCIIToUTF16("ABCD"));
  ASSERT_EQ(2, combobox_->GetItemCountForTest());
  ASSERT_EQ(ASCIIToUTF16("AbCd"), combobox_->GetItemForTest(0));
  ASSERT_EQ(ASCIIToUTF16("aBcD"), combobox_->GetItemForTest(1));
}

TEST_F(EditableComboboxTest, DontShowOnEmpty) {
  std::vector<base::string16> items = {ASCIIToUTF16("item0"),
                                       ASCIIToUTF16("item1")};
  InitEditableCombobox(items, /*filter_on_edit=*/false,
                       /*show_on_empty=*/false);

  ASSERT_EQ(0, combobox_->GetItemCountForTest());
  combobox_->SetTextForTest(ASCIIToUTF16("a"));
  ASSERT_EQ(2, combobox_->GetItemCountForTest());
  ASSERT_EQ(ASCIIToUTF16("item0"), combobox_->GetItemForTest(0));
  ASSERT_EQ(ASCIIToUTF16("item1"), combobox_->GetItemForTest(1));
}

TEST_F(EditableComboboxTest, NoFilteringNotifiesListener) {
  std::vector<base::string16> items = {ASCIIToUTF16("item0"),
                                       ASCIIToUTF16("item1")};
  InitEditableCombobox(items, /*filter_on_edit=*/false, /*show_on_empty=*/true);

  ASSERT_EQ(0, listener_->change_count());
  combobox_->SetTextForTest(ASCIIToUTF16("a"));
  ASSERT_EQ(1, listener_->change_count());
  combobox_->SetTextForTest(ASCIIToUTF16("ab"));
  ASSERT_EQ(2, listener_->change_count());
}

TEST_F(EditableComboboxTest, FilteringNotifiesListener) {
  std::vector<base::string16> items = {ASCIIToUTF16("item0"),
                                       ASCIIToUTF16("item1")};
  InitEditableCombobox(items, /*filter_on_edit=*/true, /*show_on_empty=*/true);

  ASSERT_EQ(0, listener_->change_count());
  combobox_->SetTextForTest(ASCIIToUTF16("i"));
  ASSERT_EQ(1, listener_->change_count());
  combobox_->SetTextForTest(ASCIIToUTF16("ix"));
  ASSERT_EQ(2, listener_->change_count());
  combobox_->SetTextForTest(ASCIIToUTF16("ixy"));
  ASSERT_EQ(3, listener_->change_count());
}

}  // namespace
}  // namespace views
