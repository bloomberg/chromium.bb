// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/pagination_model.h"

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/app_list/pagination_model_observer.h"

namespace app_list {
namespace test {

class TestPaginationModelObserver : public PaginationModelObserver {
 public:
  TestPaginationModelObserver()
      : model_(NULL),
        expected_page_selection_(0),
        expected_transition_start_(0),
        expected_transition_end_(0) {
    Reset();
  }
  virtual ~TestPaginationModelObserver() {}

  void Reset() {
    selection_count_ = 0;
    transition_start_count_ = 0;
    transition_end_count_ = 0;
    selected_pages_.clear();
  }

  void set_model(PaginationModel* model) { model_ = model; }

  void set_expected_page_selection(int expected_page_selection) {
    expected_page_selection_ = expected_page_selection;
  }
  void set_expected_transition_start(int expected_transition_start) {
    expected_transition_start_ = expected_transition_start;
  }
  void set_expected_transition_end(int expected_transition_end) {
    expected_transition_end_ = expected_transition_end;
  }

  const std::string& selected_pages() const { return selected_pages_; }
  int selection_count() const { return selection_count_; }
  int transition_start_count() const { return transition_start_count_; }
  int transition_end_count() const { return transition_end_count_; }

 private:
  void AppendSelectedPage(int page) {
    if (selected_pages_.length())
      selected_pages_.append(std::string(" "));
    selected_pages_.append(StringPrintf("%d", page));
  }

  // PaginationModelObserver overrides:
  virtual void TotalPagesChanged() OVERRIDE {}
  virtual void SelectedPageChanged(int old_selected,
                                   int new_selected) OVERRIDE {
    AppendSelectedPage(new_selected);
    ++selection_count_;
    if (expected_page_selection_ &&
        selection_count_ == expected_page_selection_) {
      MessageLoop::current()->Quit();
    }
  }
  virtual void TransitionChanged() OVERRIDE {
    if (model_->transition().progress == 0)
      ++transition_start_count_;
    if (model_->transition().progress == 1)
      ++transition_end_count_;

    if ((expected_transition_start_ &&
         transition_start_count_ == expected_transition_start_) ||
        (expected_transition_end_ &&
         transition_end_count_ == expected_transition_end_)) {
      MessageLoop::current()->Quit();
    }
  }

  PaginationModel* model_;

  int expected_page_selection_;
  int expected_transition_start_;
  int expected_transition_end_;

  int selection_count_;
  int transition_start_count_;
  int transition_end_count_;

  std::string selected_pages_;

  DISALLOW_COPY_AND_ASSIGN(TestPaginationModelObserver);
};

class PaginationModelTest : public testing::Test {
 public:
  PaginationModelTest() {}
  virtual ~PaginationModelTest() {}

  // testing::Test overrides:
  virtual void SetUp() OVERRIDE {
    pagination_.SetTotalPages(5);
    pagination_.SetTransitionDuration(1);
    observer_.set_model(&pagination_);
    pagination_.AddObserver(&observer_);
  }
  virtual void TearDown() OVERRIDE {
    pagination_.RemoveObserver(&observer_);
    observer_.set_model(NULL);
  }

 protected:
  void SetStartPageAndExpects(int start_page,
                              int expected_selection,
                              int expected_transition_start,
                              int expected_transition_end) {
    observer_.set_expected_page_selection(0);
    pagination_.SelectPage(start_page, false /* animate */);
    observer_.Reset();
    observer_.set_expected_page_selection(expected_selection);
    observer_.set_expected_transition_start(expected_transition_start);
    observer_.set_expected_transition_end(expected_transition_end);
  }

  PaginationModel pagination_;
  TestPaginationModelObserver observer_;

 private:
  MessageLoopForUI message_loop_;

  DISALLOW_COPY_AND_ASSIGN(PaginationModelTest);
};

TEST_F(PaginationModelTest, SelectPage) {
  pagination_.SelectPage(0, false /* animate */);
  pagination_.SelectPage(2, false /* animate */);
  pagination_.SelectPage(4, false /* animate */);
  pagination_.SelectPage(3, false /* animate */);
  pagination_.SelectPage(1, false /* animate */);

  EXPECT_EQ(0, observer_.transition_start_count());
  EXPECT_EQ(0, observer_.transition_end_count());
  EXPECT_EQ(5, observer_.selection_count());
  EXPECT_EQ(std::string("0 2 4 3 1"), observer_.selected_pages());

  // Nothing happens if select the same page.
  pagination_.SelectPage(1, false /* animate */);
  EXPECT_EQ(5, observer_.selection_count());
  EXPECT_EQ(std::string("0 2 4 3 1"), observer_.selected_pages());
}

TEST_F(PaginationModelTest, SelectPageAnimated) {
  const int kStartPage = 0;

  // One transition.
  SetStartPageAndExpects(kStartPage, 1, 0, 0);
  pagination_.SelectPage(1, true /* animate */);
  MessageLoop::current()->Run();
  EXPECT_EQ(1, observer_.transition_start_count());
  EXPECT_EQ(1, observer_.transition_end_count());
  EXPECT_EQ(1, observer_.selection_count());
  EXPECT_EQ(std::string("1"), observer_.selected_pages());

  // Two transitions in a row.
  SetStartPageAndExpects(kStartPage, 2, 0, 0);
  pagination_.SelectPage(1, true /* animate */);
  pagination_.SelectPage(3, true /* animate */);
  MessageLoop::current()->Run();
  EXPECT_EQ(2, observer_.transition_start_count());
  EXPECT_EQ(2, observer_.transition_end_count());
  EXPECT_EQ(2, observer_.selection_count());
  EXPECT_EQ(std::string("1 3"), observer_.selected_pages());

  // Transition to same page twice and only one should happen.
  SetStartPageAndExpects(kStartPage, 1, 0, 0);
  pagination_.SelectPage(1, true /* animate */);
  pagination_.SelectPage(1, true /* animate */);  // Ignored.
  MessageLoop::current()->Run();
  EXPECT_EQ(1, observer_.transition_start_count());
  EXPECT_EQ(1, observer_.transition_end_count());
  EXPECT_EQ(1, observer_.selection_count());
  EXPECT_EQ(std::string("1"), observer_.selected_pages());

  // More than two transitions and only the first and last would happen.
  SetStartPageAndExpects(kStartPage, 2, 0, 0);
  pagination_.SelectPage(1, true /* animate */);
  pagination_.SelectPage(3, true /* animate */);  // Ignored
  pagination_.SelectPage(0, true /* animate */);  // Ignored
  pagination_.SelectPage(2, true /* animate */);
  MessageLoop::current()->Run();
  EXPECT_EQ(2, observer_.transition_start_count());
  EXPECT_EQ(2, observer_.transition_end_count());
  EXPECT_EQ(2, observer_.selection_count());
  EXPECT_EQ(std::string("1 2"), observer_.selected_pages());
}

TEST_F(PaginationModelTest, SimpleScroll) {
  const int kStartPage = 2;

  // Scroll to the next page (negative delta) and end at more than 0.5
  SetStartPageAndExpects(kStartPage, 1, 0, 0);
  pagination_.StartScroll();
  pagination_.UpdateScroll(-0.1);
  EXPECT_EQ(kStartPage + 1, pagination_.transition().target_page);
  pagination_.UpdateScroll(-0.5);
  pagination_.EndScroll();
  // More than 0.5 transition will be finished with a page select.
  EXPECT_GT(pagination_.transition().progress, 0.5);
  MessageLoop::current()->Run();
  EXPECT_EQ(1, observer_.selection_count());

  // Scroll to the previous page (positive delta) and end at more than 0.5
  SetStartPageAndExpects(kStartPage, 1, 0, 0);
  pagination_.StartScroll();
  pagination_.UpdateScroll(0.1);
  EXPECT_EQ(kStartPage - 1, pagination_.transition().target_page);
  pagination_.UpdateScroll(0.5);
  pagination_.EndScroll();
  // More than 0.5 transition will be finished with a page select.
  EXPECT_GT(pagination_.transition().progress, 0.5);
  MessageLoop::current()->Run();
  EXPECT_EQ(1, observer_.selection_count());

  // Scroll to the next page (negative delta) and end at less than 0.5
  SetStartPageAndExpects(kStartPage, 0, 1, 0);
  pagination_.StartScroll();
  pagination_.UpdateScroll(-0.1);
  EXPECT_EQ(kStartPage + 1, pagination_.transition().target_page);
  pagination_.EndScroll();
  // Less than 0.5 transition will be reverted with no page select.
  EXPECT_LT(pagination_.transition().progress, 0.5);
  MessageLoop::current()->Run();
  EXPECT_EQ(0, observer_.selection_count());

  // Scroll to the previous page (position delta) and end at less than 0.5
  SetStartPageAndExpects(kStartPage, 0, 1, 0);
  pagination_.StartScroll();
  pagination_.UpdateScroll(0.1);
  EXPECT_EQ(kStartPage - 1, pagination_.transition().target_page);
  pagination_.EndScroll();
  // Less than 0.5 transition will be reverted with no page select.
  EXPECT_LT(pagination_.transition().progress, 0.5);
  MessageLoop::current()->Run();
  EXPECT_EQ(0, observer_.selection_count());
}

TEST_F(PaginationModelTest, ScrollWithTransition) {
  const int kStartPage = 2;

  // Scroll to the next page (negative delta) with a transition in the same
  // direction.
  SetStartPageAndExpects(kStartPage, 1, 0, 0);
  pagination_.SetTransition(PaginationModel::Transition(kStartPage + 1, 0.5));
  pagination_.StartScroll();
  pagination_.UpdateScroll(-0.1);
  EXPECT_EQ(kStartPage + 1, pagination_.transition().target_page);
  EXPECT_EQ(0.6, pagination_.transition().progress);
  pagination_.EndScroll();
  MessageLoop::current()->Run();
  EXPECT_EQ(1, observer_.selection_count());

  // Scroll to the next page (negative delta) with a transition in a different
  // direction.
  SetStartPageAndExpects(kStartPage, 0, 1, 0);
  pagination_.SetTransition(PaginationModel::Transition(kStartPage - 1, 0.5));
  pagination_.StartScroll();
  pagination_.UpdateScroll(-0.1);
  EXPECT_EQ(kStartPage - 1, pagination_.transition().target_page);
  EXPECT_EQ(0.4, pagination_.transition().progress);
  pagination_.EndScroll();
  MessageLoop::current()->Run();
  EXPECT_EQ(0, observer_.selection_count());

  // Scroll to the previous page (positive delta) with a transition in the same
  // direction.
  SetStartPageAndExpects(kStartPage, 1, 0, 0);
  pagination_.SetTransition(PaginationModel::Transition(kStartPage - 1, 0.5));
  pagination_.StartScroll();
  pagination_.UpdateScroll(0.1);
  EXPECT_EQ(kStartPage - 1, pagination_.transition().target_page);
  EXPECT_EQ(0.6, pagination_.transition().progress);
  pagination_.EndScroll();
  MessageLoop::current()->Run();
  EXPECT_EQ(1, observer_.selection_count());

  // Scroll to the previous page (positive delta) with a transition in a
  // different direction.
  SetStartPageAndExpects(kStartPage, 0, 1, 0);
  pagination_.SetTransition(PaginationModel::Transition(kStartPage + 1, 0.5));
  pagination_.StartScroll();
  pagination_.UpdateScroll(0.1);
  EXPECT_EQ(kStartPage + 1, pagination_.transition().target_page);
  EXPECT_EQ(0.4, pagination_.transition().progress);
  pagination_.EndScroll();
  MessageLoop::current()->Run();
  EXPECT_EQ(0, observer_.selection_count());
}

TEST_F(PaginationModelTest, LongScroll) {
  const int kStartPage = 2;

  // Scroll to the next page (negative delta) with a transition in the same
  // direction. And scroll enough to change page twice.
  SetStartPageAndExpects(kStartPage, 2, 0, 0);
  pagination_.SetTransition(PaginationModel::Transition(kStartPage + 1, 0.5));
  pagination_.StartScroll();
  pagination_.UpdateScroll(-0.1);
  EXPECT_EQ(kStartPage + 1, pagination_.transition().target_page);
  EXPECT_EQ(0.6, pagination_.transition().progress);
  pagination_.UpdateScroll(-0.5);
  EXPECT_EQ(1, observer_.selection_count());
  pagination_.UpdateScroll(-0.5);
  EXPECT_EQ(kStartPage + 2, pagination_.transition().target_page);
  pagination_.EndScroll();
  MessageLoop::current()->Run();
  EXPECT_EQ(2, observer_.selection_count());

  // Scroll to the next page (negative delta) with a transition in a different
  // direction. And scroll enough to revert it and switch page once.
  SetStartPageAndExpects(kStartPage, 1, 0, 0);
  pagination_.SetTransition(PaginationModel::Transition(kStartPage - 1, 0.5));
  pagination_.StartScroll();
  pagination_.UpdateScroll(-0.1);
  EXPECT_EQ(kStartPage - 1, pagination_.transition().target_page);
  EXPECT_EQ(0.4, pagination_.transition().progress);
  pagination_.UpdateScroll(-0.5);  // This clears the transition.
  pagination_.UpdateScroll(-0.5);  // This starts a new transition.
  EXPECT_EQ(kStartPage + 1, pagination_.transition().target_page);
  pagination_.EndScroll();
  MessageLoop::current()->Run();
  EXPECT_EQ(1, observer_.selection_count());

  // Similar cases as above but in the opposite direction.
  // Scroll to the previous page (positive delta) with a transition in the same
  // direction. And scroll enough to change page twice.
  SetStartPageAndExpects(kStartPage, 2, 0, 0);
  pagination_.SetTransition(PaginationModel::Transition(kStartPage - 1, 0.5));
  pagination_.StartScroll();
  pagination_.UpdateScroll(0.1);
  EXPECT_EQ(kStartPage - 1, pagination_.transition().target_page);
  EXPECT_EQ(0.6, pagination_.transition().progress);
  pagination_.UpdateScroll(0.5);
  EXPECT_EQ(1, observer_.selection_count());
  pagination_.UpdateScroll(0.5);
  EXPECT_EQ(kStartPage - 2, pagination_.transition().target_page);
  pagination_.EndScroll();
  MessageLoop::current()->Run();
  EXPECT_EQ(2, observer_.selection_count());

  // Scroll to the previous page (positive delta) with a transition in a
  // different direction. And scroll enough to revert it and switch page once.
  SetStartPageAndExpects(kStartPage, 1, 0, 0);
  pagination_.SetTransition(PaginationModel::Transition(kStartPage + 1, 0.5));
  pagination_.StartScroll();
  pagination_.UpdateScroll(0.1);
  EXPECT_EQ(kStartPage + 1, pagination_.transition().target_page);
  EXPECT_EQ(0.4, pagination_.transition().progress);
  pagination_.UpdateScroll(0.5);  // This clears the transition.
  pagination_.UpdateScroll(0.5);  // This starts a new transition.
  EXPECT_EQ(kStartPage - 1, pagination_.transition().target_page);
  pagination_.EndScroll();
  MessageLoop::current()->Run();
  EXPECT_EQ(1, observer_.selection_count());
}

}  // namespace test
}  // namespace app_list
