// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/cocoa/text_services_context_menu.h"

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#import "ui/gfx/test/ui_cocoa_test_helper.h"
#include "ui/strings/grit/ui_strings.h"

namespace ui {

namespace {

// To reduce the chance that the API finished speaking before the test is
// completed, we want to use a large string to stall the process.
const base::string16 kSpeechText = base::ASCIIToUTF16(
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Aliquam "
    "faucibus vulputate dui ut molestie. Curabitur varius, tortor consectetur "
    "ultrices blandit, eros arcu efficitur nisl, eget cursus odio purus id "
    "augue. Nunc velit urna, porttitor et condimentum nec, imperdiet mattis "
    "tortor. Aliquam vitae mattis urna. Mauris vulputate hendrerit nulla, "
    "quis rutrum urna egestas eu. Vivamus porttitor ligula lectus, non "
    "dapibus arcu rutrum eu. Curabitur placerat tincidunt sem, vel rutrum "
    "nibh porta non. Nunc lacinia, turpis et maximus commodo, odio est "
    "viverra ligula, finibus pharetra tellus lacus eget ante. Integer "
    "venenatis hendrerit tellus eget tincidunt. Proin tempor quam ut purus "
    "vulputate tempor. Donec commodo urna ut tortor congue, ut gravida ligula."
    "The yellow boxfish is a species of boxfish. It is box-shaped. It is "
    "bright yellow as a juvenile. As it ages, the brightness fades. Very "
    "old specimens will have blue-grey colouration with faded yellow. It "
    "feeds mainly on algae, but will also feed on sponges and shellfish."
    "Testing testing testing testing testing testing testing testing "
    "testing testing testing testing testing testing testing testing "
    "testing testing testing testing testing testing testing testing "
    "testing testing testing testing testing testing testing testing "
    "and some more testing.");

}  // namespace

class TextServicesContextMenuTest : public CocoaTest,
                                    public TextServicesContextMenu::Delegate,
                                    public SimpleMenuModel::Delegate {
 public:
  TextServicesContextMenuTest()
      : menu_(this),
        menu_model_(this),
        text_direction_(base::i18n::UNKNOWN_DIRECTION) {}

  // TextServicesContextMenu::Delegate:
  base::string16 GetSelectedText() const override { return base::string16(); }

  bool IsTextDirectionEnabled(
      base::i18n::TextDirection direction) const override {
    return true;
  }

  bool IsTextDirectionChecked(
      base::i18n::TextDirection direction) const override {
    return text_direction_ == direction;
  }

  void UpdateTextDirection(base::i18n::TextDirection direction) override {}

  // SimpleMenuModel::Delegate:
  bool IsCommandIdChecked(int command_id) const override { return false; }

  bool IsCommandIdEnabled(int command_id) const override { return true; }

  void ExecuteCommand(int command_id, int event_flags) override {}

 protected:
  ui::TextServicesContextMenu menu_;

  ui::SimpleMenuModel menu_model_;

  base::i18n::TextDirection text_direction_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TextServicesContextMenuTest);
};

// Tests to see if the BiDi and Speech menu gets appended.
TEST_F(TextServicesContextMenuTest, AppendMenu) {
  menu_model_.AddItem(0, base::ASCIIToUTF16("test item"));

  menu_.AppendToContextMenu(&menu_model_);
  menu_.AppendEditableItems(&menu_model_);

  int menu_index = 1;

  // Separator item.
  EXPECT_TRUE(menu_model_.IsEnabledAt(menu_index));
  EXPECT_EQ(MenuModel::ItemType::TYPE_SEPARATOR,
            menu_model_.GetTypeAt(menu_index));
  menu_index++;

  // Speech Submenu item.
  EXPECT_TRUE(menu_model_.IsEnabledAt(menu_index));
  EXPECT_EQ(MenuModel::ItemType::TYPE_SUBMENU,
            menu_model_.GetTypeAt(menu_index));
  MenuModel* speech_menu = menu_model_.GetSubmenuModelAt(menu_index);
  ASSERT_TRUE(speech_menu);
  menu_index++;

  // Check each item in the Speech submenu.
  EXPECT_EQ(speech_menu->GetItemCount(), 2);
  EXPECT_EQ(IDS_SPEECH_START_SPEAKING_MAC, speech_menu->GetCommandIdAt(0));
  EXPECT_EQ(IDS_SPEECH_STOP_SPEAKING_MAC, speech_menu->GetCommandIdAt(1));

  // BiDi Submenu item.
  EXPECT_TRUE(menu_model_.IsEnabledAt(menu_index));
  EXPECT_EQ(MenuModel::ItemType::TYPE_SUBMENU,
            menu_model_.GetTypeAt(menu_index));
  MenuModel* bidi_menu = menu_model_.GetSubmenuModelAt(menu_index);
  ASSERT_TRUE(bidi_menu);

  // Check each item in the BiDi submenu.
  EXPECT_EQ(bidi_menu->GetItemCount(), 3);
  EXPECT_EQ(IDS_CONTENT_CONTEXT_WRITING_DIRECTION_DEFAULT,
            bidi_menu->GetCommandIdAt(0));
  EXPECT_EQ(IDS_CONTENT_CONTEXT_WRITING_DIRECTION_LTR,
            bidi_menu->GetCommandIdAt(1));
  EXPECT_EQ(IDS_CONTENT_CONTEXT_WRITING_DIRECTION_RTL,
            bidi_menu->GetCommandIdAt(2));
}

// Tests to see if the Speech API works
TEST_F(TextServicesContextMenuTest, SpeechApi) {
  menu_.SpeakText(kSpeechText);
  EXPECT_TRUE(menu_.IsSpeaking());
  menu_.StopSpeaking();
}

// Tests to see if the Speech submenu items are correct.
TEST_F(TextServicesContextMenuTest, SpeechSubmenu) {
  const int kStartSpeakingIndex = 0;
  const int kStopSpeakingIndex = 1;

  menu_.AppendToContextMenu(&menu_model_);

  // Get and check the Speech submenu.
  EXPECT_TRUE(menu_model_.IsEnabledAt(0));
  EXPECT_EQ(MenuModel::ItemType::TYPE_SUBMENU, menu_model_.GetTypeAt(0));
  MenuModel* speech_menu = menu_model_.GetSubmenuModelAt(0);
  ASSERT_TRUE(speech_menu);

  // Check each item in the Speech submenu.
  EXPECT_EQ(speech_menu->GetItemCount(), 2);
  EXPECT_EQ(IDS_SPEECH_START_SPEAKING_MAC,
            speech_menu->GetCommandIdAt(kStartSpeakingIndex));
  EXPECT_EQ(IDS_SPEECH_STOP_SPEAKING_MAC,
            speech_menu->GetCommandIdAt(kStopSpeakingIndex));
  EXPECT_TRUE(speech_menu->IsEnabledAt(kStartSpeakingIndex));
  EXPECT_FALSE(speech_menu->IsEnabledAt(kStopSpeakingIndex));

  menu_.SpeakText(kSpeechText);
  EXPECT_TRUE(menu_.IsSpeaking());

  EXPECT_TRUE(speech_menu->IsEnabledAt(kStartSpeakingIndex));
  EXPECT_TRUE(speech_menu->IsEnabledAt(kStopSpeakingIndex));

  menu_.StopSpeaking();
  EXPECT_TRUE(speech_menu->IsEnabledAt(kStartSpeakingIndex));
  EXPECT_FALSE(speech_menu->IsEnabledAt(kStopSpeakingIndex));
}

// Tests to see if the BiDi submenu items are correct.
TEST_F(TextServicesContextMenuTest, BiDiSubmenu) {
  const int kTextDirectionDefaultIndex = 0;
  const int kTextDirectionLTRIndex = 1;
  const int kTextDirectionRTLIndex = 2;

  menu_.AppendEditableItems(&menu_model_);

  // Get and check the BiDi Submenu.
  EXPECT_TRUE(menu_model_.IsEnabledAt(0));
  EXPECT_EQ(MenuModel::ItemType::TYPE_SUBMENU, menu_model_.GetTypeAt(0));
  MenuModel* bidi_menu = menu_model_.GetSubmenuModelAt(0);
  ASSERT_TRUE(bidi_menu);

  // Check each item in the BiDi submenu.
  EXPECT_EQ(bidi_menu->GetItemCount(), 3);
  EXPECT_EQ(IDS_CONTENT_CONTEXT_WRITING_DIRECTION_DEFAULT,
            bidi_menu->GetCommandIdAt(kTextDirectionDefaultIndex));
  EXPECT_EQ(IDS_CONTENT_CONTEXT_WRITING_DIRECTION_LTR,
            bidi_menu->GetCommandIdAt(kTextDirectionLTRIndex));
  EXPECT_EQ(IDS_CONTENT_CONTEXT_WRITING_DIRECTION_RTL,
            bidi_menu->GetCommandIdAt(kTextDirectionRTLIndex));

  text_direction_ = base::i18n::UNKNOWN_DIRECTION;
  EXPECT_TRUE(bidi_menu->IsItemCheckedAt(kTextDirectionDefaultIndex));
  EXPECT_FALSE(bidi_menu->IsItemCheckedAt(kTextDirectionLTRIndex));
  EXPECT_FALSE(bidi_menu->IsItemCheckedAt(kTextDirectionRTLIndex));

  text_direction_ = base::i18n::LEFT_TO_RIGHT;
  EXPECT_FALSE(bidi_menu->IsItemCheckedAt(kTextDirectionDefaultIndex));
  EXPECT_TRUE(bidi_menu->IsItemCheckedAt(kTextDirectionLTRIndex));
  EXPECT_FALSE(bidi_menu->IsItemCheckedAt(kTextDirectionRTLIndex));

  text_direction_ = base::i18n::RIGHT_TO_LEFT;
  EXPECT_FALSE(bidi_menu->IsItemCheckedAt(kTextDirectionDefaultIndex));
  EXPECT_FALSE(bidi_menu->IsItemCheckedAt(kTextDirectionLTRIndex));
  EXPECT_TRUE(bidi_menu->IsItemCheckedAt(kTextDirectionRTLIndex));
}

}  // namespace
