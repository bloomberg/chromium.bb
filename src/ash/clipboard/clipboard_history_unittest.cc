// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/clipboard_history.h"

#include <list>
#include <unordered_map>

#include "ash/clipboard/clipboard_history_controller_impl.h"
#include "ash/clipboard/clipboard_history_item.h"
#include "ash/clipboard/clipboard_history_util.h"
#include "ash/clipboard/scoped_clipboard_history_pause_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/clipboard_image_model_factory.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/clipboard_buffer.h"
#include "ui/base/clipboard/custom_data_helper.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/event_generator.h"

namespace ash {

class ClipboardHistoryTest : public AshTestBase {
 public:
  ClipboardHistoryTest() = default;
  ClipboardHistoryTest(const ClipboardHistoryTest&) = delete;
  ClipboardHistoryTest& operator=(const ClipboardHistoryTest&) = delete;
  ~ClipboardHistoryTest() override = default;

  // AshTestBase:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {chromeos::features::kClipboardHistory}, {});
    AshTestBase::SetUp();
    clipboard_history_ = const_cast<ClipboardHistory*>(
        Shell::Get()->clipboard_history_controller()->history());
    event_generator_ = std::make_unique<ui::test::EventGenerator>(
        ash::Shell::GetPrimaryRootWindow());
  }

  const std::list<ClipboardHistoryItem>& GetClipboardHistoryItems() {
    return clipboard_history_->GetItems();
  }

  ui::test::EventGenerator* GetEventGenerator() {
    return event_generator_.get();
  }

  // Writes |input_strings| to the clipboard buffer and ensures that
  // |expected_strings| are retained in history. If |in_same_sequence| is true,
  // writes to the buffer will be performed in the same task sequence.
  void WriteAndEnsureTextHistory(
      const std::vector<std::u16string>& input_strings,
      const std::vector<std::u16string>& expected_strings,
      bool in_same_sequence = false) {
    for (const auto& input_string : input_strings) {
      {
        ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
        scw.WriteText(input_string);
      }
      if (!in_same_sequence)
        base::RunLoop().RunUntilIdle();
    }
    if (in_same_sequence)
      base::RunLoop().RunUntilIdle();
    EnsureTextHistory(expected_strings);
  }

  void EnsureTextHistory(const std::vector<std::u16string>& expected_strings) {
    const std::list<ClipboardHistoryItem>& items = GetClipboardHistoryItems();
    EXPECT_EQ(expected_strings.size(), items.size());

    int expected_strings_index = 0;
    for (const auto& item : items) {
      EXPECT_EQ(expected_strings[expected_strings_index++],
                base::UTF8ToUTF16(item.data().text()));
    }
  }

  // Writes |input_bitmaps| to the clipboard buffer and ensures that
  // |expected_bitmaps| are retained in history. Writes to the buffer are
  // performed in different task sequences to simulate real world behavior.
  void WriteAndEnsureBitmapHistory(std::vector<SkBitmap>& input_bitmaps,
                                   std::vector<SkBitmap>& expected_bitmaps) {
    for (const auto& input_bitmap : input_bitmaps) {
      {
        ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
        scw.WriteImage(input_bitmap);
      }
      base::RunLoop().RunUntilIdle();
    }
    const std::list<ClipboardHistoryItem>& items = GetClipboardHistoryItems();
    EXPECT_EQ(expected_bitmaps.size(), items.size());

    int expected_bitmaps_index = 0;
    for (const auto& item : items) {
      EXPECT_TRUE(gfx::BitmapsAreEqual(
          expected_bitmaps[expected_bitmaps_index++], item.data().bitmap()));
    }
  }

  // Writes |input_data| to the clipboard buffer and ensures that
  // |expected_data| is retained in history. After writing to the buffer, the
  // current task sequence is run to idle to simulate real world behavior.
  void WriteAndEnsureCustomDataHistory(
      const std::unordered_map<std::u16string, std::u16string>& input_data,
      const std::unordered_map<std::u16string, std::u16string>& expected_data) {
    base::Pickle input_data_pickle;
    ui::WriteCustomDataToPickle(input_data, &input_data_pickle);

    {
      ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
      scw.WritePickledData(input_data_pickle,
                           ui::ClipboardFormatType::WebCustomDataType());
    }
    base::RunLoop().RunUntilIdle();

    const std::list<ClipboardHistoryItem> items = GetClipboardHistoryItems();
    EXPECT_EQ(expected_data.empty() ? 0u : 1u, items.size());

    if (expected_data.empty())
      return;

    std::unordered_map<std::u16string, std::u16string> actual_data;
    ui::ReadCustomDataIntoMap(items.front().data().custom_data_data().c_str(),
                              items.front().data().custom_data_data().size(),
                              &actual_data);

    EXPECT_EQ(expected_data, actual_data);
  }

  ClipboardHistory* clipboard_history() { return clipboard_history_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;
  // Owned by ClipboardHistoryControllerImpl.
  ClipboardHistory* clipboard_history_ = nullptr;
};

// Tests that with nothing copied, nothing is shown.
TEST_F(ClipboardHistoryTest, NothingCopiedNothingSaved) {
  // When nothing is copied, nothing should be saved.
  WriteAndEnsureTextHistory(/*input_strings=*/{},
                            /*expected_strings=*/{});
}

// Tests that if one thing is copied, one thing is saved.
TEST_F(ClipboardHistoryTest, OneThingCopiedOneThingSaved) {
  std::vector<std::u16string> input_strings{u"test"};
  std::vector<std::u16string> expected_strings = input_strings;

  // Test that only one string is in history.
  WriteAndEnsureTextHistory(input_strings, expected_strings);
}

// Tests that if the same (non bitmap) thing is copied, only one of the
// duplicates is in the list.
TEST_F(ClipboardHistoryTest, DuplicateBasic) {
  std::vector<std::u16string> input_strings{u"test", u"test"};
  std::vector<std::u16string> expected_strings{u"test"};

  // Test that both things are saved.
  WriteAndEnsureTextHistory(input_strings, expected_strings);
}

// Tests that if multiple things are copied in the same task sequence, only the
// most recent thing is saved.
TEST_F(ClipboardHistoryTest, InSameSequenceBasic) {
  std::vector<std::u16string> input_strings{u"test1", u"test2", u"test3"};
  // Because |input_strings| will be copied in the same task sequence, history
  // should only retain the most recent thing.
  std::vector<std::u16string> expected_strings{u"test3"};

  // Test that only the most recent thing is saved.
  WriteAndEnsureTextHistory(input_strings, expected_strings,
                            /*in_same_sequence=*/true);
}

// Tests the ordering of history is in reverse chronological order.
TEST_F(ClipboardHistoryTest, HistoryIsReverseChronological) {
  std::vector<std::u16string> input_strings{u"test1", u"test2", u"test3",
                                            u"test4"};
  std::vector<std::u16string> expected_strings = input_strings;
  // Reverse the vector, history should match this ordering.
  std::reverse(std::begin(expected_strings), std::end(expected_strings));
  WriteAndEnsureTextHistory(input_strings, expected_strings);
}

// Tests that when a duplicate is copied, the existing duplicate item moves up
// to the front of the clipboard history.
TEST_F(ClipboardHistoryTest, DuplicatePrecedesPreviousRecord) {
  // Input holds a unique string sandwiched by a copy.
  std::vector<std::u16string> input_strings{u"test1", u"test2", u"test1",
                                            u"test3"};
  // The result should be a reversal of the copied elements. When a duplicate
  // is copied, history will have that item moved to the front instead of adding
  // a new item.
  std::vector<std::u16string> expected_strings{u"test3", u"test1", u"test2"};

  WriteAndEnsureTextHistory(input_strings, expected_strings);
}

// Tests that nothing is saved after history is cleared.
TEST_F(ClipboardHistoryTest, ClearHistoryBasic) {
  // Input holds a unique string sandwhiched by a copy.
  std::vector<std::u16string> input_strings{u"test1", u"test2", u"test1"};
  // The result should be a reversal of the last two elements. When a duplicate
  // is copied, history will show the most recent version of that duplicate.
  std::vector<std::u16string> expected_strings{};

  for (const auto& input_string : input_strings) {
    ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
    scw.WriteText(input_string);
  }

  clipboard_history()->Clear();
  EnsureTextHistory(expected_strings);
}

// Tests that there is no crash when an empty clipboard is cleared with empty
// clipboard history.
TEST_F(ClipboardHistoryTest, ClearHistoryFromClipboardNoHistory) {
  ui::Clipboard::GetForCurrentThread()->Clear(ui::ClipboardBuffer::kCopyPaste);
}

// Tests that clipboard history is cleared when the clipboard is cleared.
TEST_F(ClipboardHistoryTest, ClearHistoryFromClipboardWithHistory) {
  std::vector<std::u16string> input_strings{u"test1", u"test2"};

  std::vector<std::u16string> expected_strings_before_clear{u"test2", u"test1"};
  std::vector<std::u16string> expected_strings_after_clear{};

  for (const auto& input_string : input_strings) {
    {
      ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
      scw.WriteText(input_string);
    }
    base::RunLoop().RunUntilIdle();
  }

  EnsureTextHistory(expected_strings_before_clear);

  ui::Clipboard::GetForCurrentThread()->Clear(ui::ClipboardBuffer::kCopyPaste);

  EnsureTextHistory(expected_strings_after_clear);
}

// Tests that the limit of clipboard history is respected.
TEST_F(ClipboardHistoryTest, HistoryLimit) {
  std::vector<std::u16string> input_strings{u"test1", u"test2", u"test3",
                                            u"test4", u"test5", u"test6"};

  // The result should be a reversal of the last five elements.
  std::vector<std::u16string> expected_strings{input_strings.begin() + 1,
                                               input_strings.end()};
  std::reverse(expected_strings.begin(), expected_strings.end());
  WriteAndEnsureTextHistory(input_strings, expected_strings);
}

// Tests that pausing clipboard history results in no history collected.
TEST_F(ClipboardHistoryTest, PauseHistory) {
  std::vector<std::u16string> input_strings{u"test1", u"test2", u"test1"};
  // Because history is paused, there should be nothing stored.
  std::vector<std::u16string> expected_strings{};

  ScopedClipboardHistoryPauseImpl scoped_pause(clipboard_history());
  WriteAndEnsureTextHistory(input_strings, expected_strings);
}

// Tests that bitmaps are recorded in clipboard history.
TEST_F(ClipboardHistoryTest, BasicBitmap) {
  SkBitmap test_bitmap;
  test_bitmap.allocN32Pixels(3, 2);
  test_bitmap.eraseARGB(255, 0, 255, 0);
  std::vector<SkBitmap> input_bitmaps{test_bitmap};
  std::vector<SkBitmap> expected_bitmaps{test_bitmap};

  WriteAndEnsureBitmapHistory(input_bitmaps, expected_bitmaps);
}

// Tests that duplicate bitmaps show up in history as one item placed in
// most-recent order.
TEST_F(ClipboardHistoryTest, DuplicateBitmap) {
  SkBitmap test_bitmap_1;
  test_bitmap_1.allocN32Pixels(3, 2);
  test_bitmap_1.eraseARGB(255, 0, 255, 0);
  SkBitmap test_bitmap_2;
  test_bitmap_2.allocN32Pixels(3, 2);
  test_bitmap_2.eraseARGB(0, 255, 0, 0);

  std::vector<SkBitmap> input_bitmaps{test_bitmap_1, test_bitmap_2,
                                      test_bitmap_1};
  std::vector<SkBitmap> expected_bitmaps{test_bitmap_1, test_bitmap_2};
  WriteAndEnsureBitmapHistory(input_bitmaps, expected_bitmaps);
}

// Tests that unrecognized custom data is omitted from clipboard history.
TEST_F(ClipboardHistoryTest, BasicCustomData) {
  const std::unordered_map<std::u16string, std::u16string> input_data = {
      {u"custom-format-1", u"custom-data-1"},
      {u"custom-format-2", u"custom-data-2"}};

  // Custom data which is not recognized is omitted from history.
  WriteAndEnsureCustomDataHistory(input_data, /*expected_data=*/{});
}

// Tests that file system data is recorded in clipboard history.
TEST_F(ClipboardHistoryTest, BasicFileSystemData) {
  const std::unordered_map<std::u16string, std::u16string> input_data = {
      {u"fs/sources", u"/path/to/My%20File.txt"}};

  const std::unordered_map<std::u16string, std::u16string> expected_data =
      input_data;

  WriteAndEnsureCustomDataHistory(input_data, expected_data);
}

// Tests that the ClipboardHistoryDisplayFormat for HTML with no <img or <table
// tags is text.
TEST_F(ClipboardHistoryTest, DisplayFormatForPlainHTML) {
  ui::ClipboardData data;
  data.set_markup_data("plain html with no img or table tags");

  EXPECT_EQ(ClipboardHistoryUtil::ClipboardHistoryDisplayFormat::kText,
            ClipboardHistoryUtil::CalculateDisplayFormat(data));

  data.set_markup_data("<img> </img>");

  EXPECT_EQ(ClipboardHistoryUtil::ClipboardHistoryDisplayFormat::kHtml,
            ClipboardHistoryUtil::CalculateDisplayFormat(data));
}

// Tests that Ash.ClipboardHistory.ControlToVDelay is only recorded if
// ui::VKEY_V is pressed with only ui::VKEY_CONTROL pressed.
TEST_F(ClipboardHistoryTest, RecordControlV) {
  base::HistogramTester histogram_tester;
  auto* event_generator = GetEventGenerator();

  // Press Ctrl + V, a histogram should be emitted.
  event_generator->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
  PressAndReleaseKey(ui::VKEY_V, ui::EF_CONTROL_DOWN);

  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ControlToVDelay", 1u);

  // Press and release V again, no additional histograms should be emitted.
  PressAndReleaseKey(ui::VKEY_V, ui::EF_CONTROL_DOWN);

  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ControlToVDelay", 1u);

  // Release Control to return to no keys pressed.
  event_generator->ReleaseKey(ui::VKEY_CONTROL, ui::EF_NONE);
  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ControlToVDelay", 1u);

  // Hold shift while pressing ctrl + V, no histogram should be recorded.
  event_generator->PressKey(ui::VKEY_SHIFT, ui::EF_NONE);
  event_generator->PressKey(ui::VKEY_CONTROL, ui::EF_SHIFT_DOWN);
  PressAndReleaseKey(ui::VKEY_V, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN);

  event_generator->ReleaseKey(ui::VKEY_CONTROL, ui::EF_SHIFT_DOWN);
  event_generator->ReleaseKey(ui::VKEY_SHIFT, ui::EF_NONE);

  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ControlToVDelay", 1u);

  // Press Ctrl, then press and release a random key, then press V. A histogram
  // should be recorded.
  event_generator->PressKey(ui::VKEY_CONTROL, ui::EF_NONE);
  PressAndReleaseKey(ui::VKEY_X, ui::EF_CONTROL_DOWN);
  PressAndReleaseKey(ui::VKEY_V, ui::EF_CONTROL_DOWN);

  histogram_tester.ExpectTotalCount("Ash.ClipboardHistory.ControlToVDelay", 2u);
}

}  // namespace ash
