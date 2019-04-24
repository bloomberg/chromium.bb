// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/seat.h"

#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_scheduler/task_scheduler.h"
#include "components/exo/data_source.h"
#include "components/exo/data_source_delegate.h"
#include "components/exo/seat_observer.h"
#include "components/exo/test/exo_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"

namespace exo {
namespace {

using SeatTest = test::ExoTestBase;

class MockSeatObserver : public SeatObserver {
 public:
  int on_surface_focused_count() { return on_surface_focused_count_; }

  // Overridden from SeatObserver:
  void OnSurfaceFocusing(Surface* gaining_focus) override {
    ASSERT_EQ(on_surface_focused_count_, on_surface_pre_focused_count_);
    on_surface_pre_focused_count_++;
  }
  void OnSurfaceFocused(Surface* gained_focus) override {
    on_surface_focused_count_++;
    ASSERT_EQ(on_surface_focused_count_, on_surface_pre_focused_count_);
  }

 private:
  int on_surface_pre_focused_count_ = 0;
  int on_surface_focused_count_ = 0;
};

class TestDataSourceDelegate : public DataSourceDelegate {
 public:
  TestDataSourceDelegate() {}
  bool cancelled() const { return cancelled_; }

  // Overridden from DataSourceDelegate:
  void OnDataSourceDestroying(DataSource* device) override {}
  void OnTarget(const std::string& mime_type) override {}
  void OnSend(const std::string& mime_type, base::ScopedFD fd) override {
    std::string test_data = "TestData";
    ASSERT_TRUE(base::WriteFileDescriptor(fd.get(), test_data.data(),
                                          test_data.size()));
  }
  void OnCancelled() override { cancelled_ = true; }
  void OnDndDropPerformed() override {}
  void OnDndFinished() override {}
  void OnAction(DndAction dnd_action) override {}

 private:
  bool cancelled_ = false;

  DISALLOW_COPY_AND_ASSIGN(TestDataSourceDelegate);
};

void RunReadingTask() {
  base::TaskScheduler::GetInstance()->FlushForTesting();
  base::RunLoop().RunUntilIdle();
}

TEST_F(SeatTest, OnSurfaceFocused) {
  Seat seat;
  MockSeatObserver observer;

  seat.AddObserver(&observer);
  seat.OnWindowFocused(nullptr, nullptr);
  ASSERT_EQ(1, observer.on_surface_focused_count());

  seat.RemoveObserver(&observer);
  seat.OnWindowFocused(nullptr, nullptr);
  ASSERT_EQ(1, observer.on_surface_focused_count());
}

TEST_F(SeatTest, SetSelection) {
  Seat seat;

  TestDataSourceDelegate delegate;
  DataSource source(&delegate);
  source.Offer("text/plain;charset=utf-8");
  seat.SetSelection(&source);

  RunReadingTask();

  std::string clipboard;
  ui::Clipboard::GetForCurrentThread()->ReadAsciiText(
      ui::CLIPBOARD_TYPE_COPY_PASTE, &clipboard);

  EXPECT_EQ(clipboard, std::string("TestData"));
}

TEST_F(SeatTest, SetSelection_TwiceSame) {
  Seat seat;

  TestDataSourceDelegate delegate;
  DataSource source(&delegate);

  seat.SetSelection(&source);
  RunReadingTask();
  seat.SetSelection(&source);
  RunReadingTask();

  EXPECT_FALSE(delegate.cancelled());
}

TEST_F(SeatTest, SetSelection_TwiceDifferent) {
  Seat seat;

  TestDataSourceDelegate delegate1;
  DataSource source1(&delegate1);
  seat.SetSelection(&source1);
  RunReadingTask();

  EXPECT_FALSE(delegate1.cancelled());

  TestDataSourceDelegate delegate2;
  DataSource source2(&delegate2);
  seat.SetSelection(&source2);
  RunReadingTask();

  EXPECT_TRUE(delegate1.cancelled());
}

TEST_F(SeatTest, SetSelection_ClipboardChangedDuringSetSelection) {
  Seat seat;

  TestDataSourceDelegate delegate;
  DataSource source(&delegate);
  seat.SetSelection(&source);

  {
    ui::ScopedClipboardWriter writer(ui::CLIPBOARD_TYPE_COPY_PASTE);
    writer.WriteText(base::UTF8ToUTF16("New data"));
  }

  RunReadingTask();

  // The previous source should be cancelled.
  EXPECT_TRUE(delegate.cancelled());

  std::string clipboard;
  ui::Clipboard::GetForCurrentThread()->ReadAsciiText(
      ui::CLIPBOARD_TYPE_COPY_PASTE, &clipboard);
  EXPECT_EQ(clipboard, "New data");
}

TEST_F(SeatTest, SetSelection_ClipboardChangedAfterSetSelection) {
  Seat seat;

  TestDataSourceDelegate delegate;
  DataSource source(&delegate);
  seat.SetSelection(&source);
  RunReadingTask();

  {
    ui::ScopedClipboardWriter writer(ui::CLIPBOARD_TYPE_COPY_PASTE);
    writer.WriteText(base::UTF8ToUTF16("New data"));
  }

  // The previous source should be cancelled.
  EXPECT_TRUE(delegate.cancelled());

  std::string clipboard;
  ui::Clipboard::GetForCurrentThread()->ReadAsciiText(
      ui::CLIPBOARD_TYPE_COPY_PASTE, &clipboard);
  EXPECT_EQ(clipboard, "New data");
}

TEST_F(SeatTest, SetSelection_SourceDestroyedDuringSetSelection) {
  Seat seat;

  {
    ui::ScopedClipboardWriter writer(ui::CLIPBOARD_TYPE_COPY_PASTE);
    writer.WriteText(base::UTF8ToUTF16("Original data"));
  }

  {
    TestDataSourceDelegate delegate;
    DataSource source(&delegate);
    seat.SetSelection(&source);
    // source destroyed here.
  }

  RunReadingTask();

  std::string clipboard;
  ui::Clipboard::GetForCurrentThread()->ReadAsciiText(
      ui::CLIPBOARD_TYPE_COPY_PASTE, &clipboard);
  EXPECT_EQ(clipboard, "Original data");
}

TEST_F(SeatTest, SetSelection_SourceDestroyedAfterSetSelection) {
  Seat seat;

  TestDataSourceDelegate delegate1;
  {
    DataSource source(&delegate1);
    seat.SetSelection(&source);
    RunReadingTask();
    // source destroyed here.
  }

  RunReadingTask();

  {
    TestDataSourceDelegate delegate2;
    DataSource source(&delegate2);
    seat.SetSelection(&source);
    RunReadingTask();
    // source destroyed here.
  }

  RunReadingTask();

  // delegate1 should not receive cancel request because the first data source
  // has already been destroyed.
  EXPECT_FALSE(delegate1.cancelled());
}

TEST_F(SeatTest, SetSelection_NullSource) {
  Seat seat;

  TestDataSourceDelegate delegate;
  DataSource source(&delegate);
  source.Offer("text/plain;charset=utf-8");
  seat.SetSelection(&source);

  RunReadingTask();

  // Should clear the clipboard.
  seat.SetSelection(nullptr);

  ASSERT_TRUE(delegate.cancelled());

  std::string clipboard;
  ui::Clipboard::GetForCurrentThread()->ReadAsciiText(
      ui::CLIPBOARD_TYPE_COPY_PASTE, &clipboard);
  EXPECT_EQ(clipboard, "");
}

TEST_F(SeatTest, PressedKeys) {
  Seat seat;
  ui::KeyEvent press_a(ui::ET_KEY_PRESSED, ui::VKEY_A, ui::DomCode::US_A, 0);
  ui::KeyEvent release_a(ui::ET_KEY_RELEASED, ui::VKEY_A, ui::DomCode::US_A, 0);
  ui::KeyEvent press_b(ui::ET_KEY_PRESSED, ui::VKEY_B, ui::DomCode::US_B, 0);
  ui::KeyEvent release_b(ui::ET_KEY_RELEASED, ui::VKEY_B, ui::DomCode::US_B, 0);

  // Press A, it should be in the map.
  seat.WillProcessEvent(&press_a);
  seat.OnKeyEvent(press_a.AsKeyEvent());
  seat.DidProcessEvent(&press_a);
  base::flat_map<ui::DomCode, ui::DomCode> pressed_keys;
  pressed_keys[ui::CodeFromNative(&press_a)] = press_a.code();
  EXPECT_EQ(pressed_keys, seat.pressed_keys());

  // Press B, then A & B should be in the map.
  seat.WillProcessEvent(&press_b);
  seat.OnKeyEvent(press_b.AsKeyEvent());
  seat.DidProcessEvent(&press_b);
  pressed_keys[ui::CodeFromNative(&press_b)] = press_b.code();
  EXPECT_EQ(pressed_keys, seat.pressed_keys());

  // Release A, with the normal order where DidProcessEvent is after OnKeyEvent,
  // only B should be in the map.
  seat.WillProcessEvent(&release_a);
  seat.OnKeyEvent(release_a.AsKeyEvent());
  seat.DidProcessEvent(&release_a);
  pressed_keys.erase(ui::CodeFromNative(&press_a));
  EXPECT_EQ(pressed_keys, seat.pressed_keys());

  // Release B, do it out of order so DidProcessEvent is before OnKeyEvent, the
  // map should then be empty.
  seat.WillProcessEvent(&release_b);
  seat.DidProcessEvent(&release_b);
  seat.OnKeyEvent(release_b.AsKeyEvent());
  EXPECT_TRUE(seat.pressed_keys().empty());
}

}  // namespace
}  // namespace exo
