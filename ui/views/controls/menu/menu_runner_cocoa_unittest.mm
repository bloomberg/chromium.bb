// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/controls/menu/menu_runner_impl_cocoa.h"

#include "base/strings/utf_string_conversions.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/events/event_utils.h"
#include "ui/views/test/views_test_base.h"

namespace views {
namespace test {

class MenuRunnerCocoaTest : public ViewsTestBase {
 public:
  MenuRunnerCocoaTest() : runner_(NULL) {}
  virtual ~MenuRunnerCocoaTest() {}

  virtual void SetUp() OVERRIDE {
    ViewsTestBase::SetUp();

    menu_.reset(new ui::SimpleMenuModel(NULL));
    menu_->AddItem(0, base::ASCIIToUTF16("Menu Item"));

    runner_ = new internal::MenuRunnerImplCocoa(menu_.get());
    EXPECT_FALSE(runner_->IsRunning());
  }

  virtual void TearDown() OVERRIDE {
    if (runner_) {
      runner_->Release();
      runner_ = NULL;
    }

    ViewsTestBase::TearDown();
  }

  // Runs the menu after scheduling |block| on the run loop.
  MenuRunner::RunResult RunMenu(dispatch_block_t block) {
    CFRunLoopPerformBlock(CFRunLoopGetCurrent(), kCFRunLoopCommonModes, ^{
        EXPECT_TRUE(runner_->IsRunning());
        block();
    });
    return runner_->RunMenuAt(
        NULL, NULL, gfx::Rect(), MENU_ANCHOR_TOPLEFT, MenuRunner::CONTEXT_MENU);
  }

 protected:
  scoped_ptr<ui::SimpleMenuModel> menu_;
  internal::MenuRunnerImplCocoa* runner_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MenuRunnerCocoaTest);
};

TEST_F(MenuRunnerCocoaTest, RunMenuAndCancel) {
  base::TimeDelta min_time = ui::EventTimeForNow();

  MenuRunner::RunResult result = RunMenu(^{
      runner_->Cancel();
      EXPECT_FALSE(runner_->IsRunning());
  });

  EXPECT_EQ(MenuRunner::NORMAL_EXIT, result);
  EXPECT_FALSE(runner_->IsRunning());

  EXPECT_GE(runner_->GetClosingEventTime(), min_time);
  EXPECT_LE(runner_->GetClosingEventTime(), ui::EventTimeForNow());

  // Cancel again.
  runner_->Cancel();
  EXPECT_FALSE(runner_->IsRunning());
}

TEST_F(MenuRunnerCocoaTest, RunMenuAndDelete) {
  MenuRunner::RunResult result = RunMenu(^{
      runner_->Release();
      runner_ = NULL;
  });

  EXPECT_EQ(MenuRunner::MENU_DELETED, result);
}

TEST_F(MenuRunnerCocoaTest, RunMenuTwice) {
  for (int i = 0; i < 2; ++i) {
    MenuRunner::RunResult result = RunMenu(^{
        runner_->Cancel();
    });
    EXPECT_EQ(MenuRunner::NORMAL_EXIT, result);
    EXPECT_FALSE(runner_->IsRunning());
  }
}

TEST_F(MenuRunnerCocoaTest, CancelWithoutRunning) {
  runner_->Cancel();
  EXPECT_FALSE(runner_->IsRunning());
  EXPECT_EQ(base::TimeDelta(), runner_->GetClosingEventTime());
}

TEST_F(MenuRunnerCocoaTest, DeleteWithoutRunning) {
  runner_->Release();
  runner_ = NULL;
}

}  // namespace test
}  // namespace views
