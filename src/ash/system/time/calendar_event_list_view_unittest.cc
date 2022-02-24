// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_event_list_view.h"

#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/time/calendar_event_list_item_view.h"
#include "ash/system/time/calendar_unittest_utils.h"
#include "ash/system/time/calendar_view_controller.h"
#include "ash/test/ash_test_base.h"
#include "base/metrics/histogram.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"

namespace ash {

namespace {

std::unique_ptr<google_apis::calendar::EventList> CreateMockEventList() {
  auto event_list = std::make_unique<google_apis::calendar::EventList>();
  event_list->set_time_zone("Greenwich Mean Time");
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_0", "summary_0", "18 Nov 2021 8:30 GMT", "18 Nov 2021 9:30 GMT"));
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_1", "summary_1", "18 Nov 2021 8:15 GMT", "18 Nov 2021 11:30 GMT"));
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_2", "summary_2", "18 Nov 2021 11:30 GMT", "18 Nov 2021 12:30 GMT"));
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_3", "", "19 Nov 2021 8:30 GMT", "19 Nov 2021 10:30 GMT"));
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_4", "summary_4", "21 Nov 2021 8:30 GMT", "21 Nov 2021 9:30 GMT"));
  event_list->InjectItemForTesting(calendar_test_utils::CreateEvent(
      "id_5", "summary_5", "21 Nov 2021 10:30 GMT", "21 Nov 2021 11:30 GMT"));

  return event_list;
}

}  // namespace

class CalendarViewEventListViewTest : public AshTestBase {
 public:
  CalendarViewEventListViewTest() = default;
  CalendarViewEventListViewTest(const CalendarViewEventListViewTest&) = delete;
  CalendarViewEventListViewTest& operator=(
      const CalendarViewEventListViewTest&) = delete;
  ~CalendarViewEventListViewTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    controller_ = std::make_unique<CalendarViewController>();
  }

  void TearDown() override {
    event_list_view_.reset();
    controller_.reset();
    AshTestBase::TearDown();
  }

  void CreateEventListView(base::Time date) {
    event_list_view_.reset();
    controller_->UpdateMonth(date);
    Shell::Get()->system_tray_model()->calendar_model()->InsertEvents(
        CreateMockEventList().get());
    controller_->selected_date_ = date;
    event_list_view_ =
        std::make_unique<CalendarEventListView>(controller_.get());
  }

  void SetSelectedDate(base::Time date) {
    controller_->selected_date_ = date;
    controller_->ShowEventListView(date, /*row_index=*/0);
  }

  CalendarEventListView* event_list_view() { return event_list_view_.get(); }
  views::View* content_view() { return event_list_view_->content_view_; }
  CalendarViewController* controller() { return controller_.get(); }

  views::Label* GetSummary(int child_index) {
    return static_cast<views::Label*>(
        static_cast<CalendarEventListItemView*>(
            content_view()->children()[child_index])
            ->summary_);
  }

  std::u16string GetEmptyLabel() {
    return static_cast<views::LabelButton*>(
               content_view()->children()[0]->children()[0])
        ->GetText();
  }

 private:
  std::unique_ptr<CalendarEventListView> event_list_view_;
  std::unique_ptr<CalendarViewController> controller_;
};

TEST_F(CalendarViewEventListViewTest, ShowEvents) {
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));

  CreateEventListView(date - base::Days(1));

  // No events on 17 Nov 2021, so we see the empty list default.
  EXPECT_EQ(1u, content_view()->children().size());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_CALENDAR_NO_EVENTS),
            GetEmptyLabel());

  SetSelectedDate(date);

  // 3 events on 18 Nov 2021. And they should be sorted by the start time.
  EXPECT_EQ(3u, content_view()->children().size());
  EXPECT_EQ(u"summary_1", GetSummary(0)->GetText());
  EXPECT_EQ(u"summary_0", GetSummary(1)->GetText());
  EXPECT_EQ(u"summary_2", GetSummary(2)->GetText());

  SetSelectedDate(date + base::Days(1));

  // 1 event on 19 Nov 2021. For no title mettings, shows "No title" as the
  // meeting summary.
  EXPECT_EQ(1u, content_view()->children().size());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_CALENDAR_NO_TITLE),
            GetSummary(0)->GetText());

  SetSelectedDate(date + base::Days(2));

  // 0 event on 20 Nov 2021.
  EXPECT_EQ(1u, content_view()->children().size());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_ASH_CALENDAR_NO_EVENTS),
            GetEmptyLabel());

  SetSelectedDate(date + base::Days(3));

  // 2 events on 21 Nov 2021.
  EXPECT_EQ(2u, content_view()->children().size());
  EXPECT_EQ(u"summary_4", GetSummary(0)->GetText());
  EXPECT_EQ(u"summary_5", GetSummary(1)->GetText());
}

TEST_F(CalendarViewEventListViewTest, LaunchEmptyList) {
  base::HistogramTester histogram_tester;
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));
  CreateEventListView(date - base::Days(1));

  // No events, so we see the empty list by default.
  EXPECT_EQ(1u, content_view()->children().size());
  views::Button* empty_list_button =
      static_cast<views::Button*>(content_view()->children()[0]->children()[0]);
  empty_list_button->AcceleratorPressed(
      ui::Accelerator(ui::KeyboardCode::VKEY_SPACE, /*modifiers=*/0));

  histogram_tester.ExpectTotalCount(
      "Ash.Calendar.UserJourneyTime.EventLaunched", 1);
}

TEST_F(CalendarViewEventListViewTest, LaunchItem) {
  base::HistogramTester histogram_tester;
  base::Time date;
  ASSERT_TRUE(base::Time::FromString("18 Nov 2021 10:00 GMT", &date));
  CreateEventListView(date);

  SetSelectedDate(date);
  EXPECT_EQ(3u, content_view()->children().size());

  // Launch the first item.
  views::Button* first_item =
      static_cast<views::Button*>(content_view()->children()[0]);
  first_item->AcceleratorPressed(
      ui::Accelerator(ui::KeyboardCode::VKEY_SPACE, /*modifiers=*/0));

  histogram_tester.ExpectTotalCount(
      "Ash.Calendar.UserJourneyTime.EventLaunched", 1);
}

}  // namespace ash
