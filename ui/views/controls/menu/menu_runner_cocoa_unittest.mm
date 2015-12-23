// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/views/controls/menu/menu_runner_impl_cocoa.h"

#import <Cocoa/Cocoa.h>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#import "testing/gtest_mac.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/events/event_utils.h"
#include "ui/views/test/views_test_base.h"

namespace views {
namespace test {
namespace {

class TestModel : public ui::SimpleMenuModel {
 public:
  TestModel() : ui::SimpleMenuModel(&delegate_), delegate_(this) {}

  void set_checked_command(int command) { checked_command_ = command; }

 private:
  class Delegate : public ui::SimpleMenuModel::Delegate {
   public:
    explicit Delegate(TestModel* model) : model_(model) {}
    bool IsCommandIdChecked(int command_id) const override {
      return command_id == model_->checked_command_;
    }
    bool IsCommandIdEnabled(int command_id) const override { return true; }
    bool GetAcceleratorForCommandId(int command_id,
                                    ui::Accelerator* accelerator) override {
      return false;
    }
    void ExecuteCommand(int command_id, int event_flags) override {}

   private:
    TestModel* model_;

    DISALLOW_COPY_AND_ASSIGN(Delegate);
  };

 private:
  int checked_command_ = -1;
  Delegate delegate_;

  DISALLOW_COPY_AND_ASSIGN(TestModel);
};

}  // namespace

class MenuRunnerCocoaTest : public ViewsTestBase {
 public:
  enum {
    kWindowHeight = 200,
    kWindowOffset = 100,
  };

  MenuRunnerCocoaTest() {}
  ~MenuRunnerCocoaTest() override {}

  void SetUp() override {
    const int kWindowWidth = 300;
    ViewsTestBase::SetUp();

    menu_.reset(new TestModel());
    menu_->AddCheckItem(0, base::ASCIIToUTF16("Menu Item"));

    parent_ = new views::Widget();
    parent_->Init(CreateParams(Widget::InitParams::TYPE_WINDOW_FRAMELESS));
    parent_->SetBounds(
        gfx::Rect(kWindowOffset, kWindowOffset, kWindowWidth, kWindowHeight));
    parent_->Show();

    runner_ = new internal::MenuRunnerImplCocoa(menu_.get());
    EXPECT_FALSE(runner_->IsRunning());
  }

  void TearDown() override {
    if (runner_) {
      runner_->Release();
      runner_ = NULL;
    }

    parent_->CloseNow();
    ViewsTestBase::TearDown();
  }

  // Runs the menu after scheduling |block| on the run loop.
  MenuRunner::RunResult RunMenu(dispatch_block_t block) {
    CFRunLoopPerformBlock(CFRunLoopGetCurrent(), kCFRunLoopCommonModes, ^{
        EXPECT_TRUE(runner_->IsRunning());
        block();
    });
    return runner_->RunMenuAt(parent_, NULL, gfx::Rect(), MENU_ANCHOR_TOPLEFT,
                              MenuRunner::CONTEXT_MENU);
  }

  // Runs then cancels a combobox menu and captures the frame of the anchoring
  // view.
  MenuRunner::RunResult RunMenuAt(const gfx::Rect& anchor) {
    last_anchor_frame_ = NSZeroRect;

    // Should be one child (the compositor layer) before showing, and it should
    // go up by one (the anchor view) while the menu is shown.
    EXPECT_EQ(1u, [[parent_->GetNativeView() subviews] count]);
    CFRunLoopPerformBlock(CFRunLoopGetCurrent(), kCFRunLoopCommonModes, ^{
      NSArray* subviews = [parent_->GetNativeView() subviews];
      EXPECT_EQ(2u, [subviews count]);
      last_anchor_frame_ = [[subviews objectAtIndex:1] frame];
      runner_->Cancel();
    });
    MenuRunner::RunResult result = runner_->RunMenuAt(
        parent_, nullptr, anchor, MENU_ANCHOR_TOPLEFT, MenuRunner::COMBOBOX);

    // Ensure the anchor view is removed.
    EXPECT_EQ(1u, [[parent_->GetNativeView() subviews] count]);
    return result;
  }

 protected:
  scoped_ptr<TestModel> menu_;
  internal::MenuRunnerImplCocoa* runner_ = nullptr;
  views::Widget* parent_ = nullptr;
  NSRect last_anchor_frame_ = NSZeroRect;

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

// Tests anchoring of the menus used for toolkit-views Comboboxes.
TEST_F(MenuRunnerCocoaTest, ComboboxAnchoring) {
  // Combobox at 20,10 in the Widget.
  const gfx::Rect combobox_rect(20, 10, 80, 50);

  // Menu anchor rects are always in screen coordinates. The window is frameless
  // so offset by the bounds.
  gfx::Rect anchor_rect = combobox_rect;
  anchor_rect.Offset(kWindowOffset, kWindowOffset);
  RunMenuAt(anchor_rect);

  // Nothing is checked, so the anchor view should have no height, to ensure the
  // menu goes below the anchor rect. There should also be no x-offset since the
  // there is no need to line-up text.
  EXPECT_NSEQ(
      NSMakeRect(combobox_rect.x(), kWindowHeight - combobox_rect.bottom(),
                 combobox_rect.width(), 0),
      last_anchor_frame_);

  menu_->set_checked_command(0);
  RunMenuAt(anchor_rect);

  // Native constant used by MenuRunnerImplCocoa.
  const CGFloat kNativeCheckmarkWidth = 18;

  // There is now a checked item, so the anchor should be vertically centered
  // inside the combobox, and offset by the width of the checkmark column.
  EXPECT_EQ(combobox_rect.x() - kNativeCheckmarkWidth,
            last_anchor_frame_.origin.x);
  EXPECT_EQ(kWindowHeight - combobox_rect.CenterPoint().y(),
            NSMidY(last_anchor_frame_));
  EXPECT_EQ(combobox_rect.width(), NSWidth(last_anchor_frame_));
  EXPECT_NE(0, NSHeight(last_anchor_frame_));

  // In RTL, Cocoa messes up the positioning unless the anchor rectangle is
  // offset to the right of the view. The offset for the checkmark is also
  // skipped, to give a better match to native behavior.
  base::i18n::SetICUDefaultLocale("he");
  RunMenuAt(anchor_rect);
  EXPECT_EQ(combobox_rect.right(), last_anchor_frame_.origin.x);
}

}  // namespace test
}  // namespace views
