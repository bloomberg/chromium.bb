// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/mus/drag_drop_controller_mus.h"

#include <memory>

#include "base/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/aura/client/drag_drop_client_observer.h"
#include "ui/aura/mus/drag_drop_controller_host.h"
#include "ui/aura/test/aura_mus_test_base.h"
#include "ui/aura/test/mus/test_window_tree.h"
#include "ui/events/event_utils.h"

namespace aura {
namespace {

class DragDropControllerMusTest : public test::AuraMusWmTestBase {
 public:
  DragDropControllerMusTest() = default;

  // test::AuraMusWmTestBase
  void SetUp() override {
    AuraMusWmTestBase::SetUp();
    drag_drop_controller_ = base::MakeUnique<DragDropControllerMus>(
        &drag_drop_controller_host_, window_tree());
  }

  void TearDown() override {
    drag_drop_controller_.reset();
    AuraMusWmTestBase::TearDown();
  }

 protected:
  std::unique_ptr<DragDropControllerMus> drag_drop_controller_;

 private:
  class TestDragDropControllerHost : public DragDropControllerHost {
   public:
    TestDragDropControllerHost() : serial_(0u) {}

    // DragDropControllerHost
    uint32_t CreateChangeIdForDrag(WindowMus* window) override {
      return serial_++;
    }

   private:
    uint32_t serial_;

  } drag_drop_controller_host_;

  DISALLOW_COPY_AND_ASSIGN(DragDropControllerMusTest);
};

class TestObserver : public client::DragDropClientObserver {
 public:
  enum class State { kNotInvoked, kDragStartInvoked, kDragEndInvoked };

  TestObserver(DragDropControllerMus* controller)
      : controller_(controller), state_(State::kNotInvoked) {
    controller_->AddObserver(this);
  }

  ~TestObserver() override { controller_->RemoveObserver(this); }

  State state() const { return state_; }

  void OnInternalLoopRun() {
    EXPECT_EQ(State::kDragStartInvoked, state_);
    controller_->OnPerformDragDropCompleted(0);
  }

  // Overrides from client::DragDropClientObserver:
  void OnDragStarted() override {
    EXPECT_EQ(State::kNotInvoked, state_);
    state_ = State::kDragStartInvoked;
  }

  void OnDragEnded() override {
    EXPECT_EQ(State::kDragStartInvoked, state_);
    state_ = State::kDragEndInvoked;
  }

 private:
  DragDropControllerMus* controller_;
  State state_;

  DISALLOW_COPY_AND_ASSIGN(TestObserver);
};

TEST_F(DragDropControllerMusTest, DragStartedAndEndedEvents) {
  TestObserver observer(drag_drop_controller_.get());
  std::unique_ptr<aura::Window> window(
      CreateNormalWindow(0, root_window(), nullptr));

  ASSERT_TRUE(base::ThreadTaskRunnerHandle::IsSet());

  // Posted task will be run when the inner loop runs in StartDragAndDrop.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&TestObserver::OnInternalLoopRun,
                                base::Unretained(&observer)));

  drag_drop_controller_->StartDragAndDrop(
      ui::OSExchangeData(), window->GetRootWindow(), window.get(),
      gfx::Point(5, 5), ui::DragDropTypes::DRAG_MOVE,
      ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE);

  EXPECT_EQ(TestObserver::State::kDragEndInvoked, observer.state());
}

}  // namespace
}  // namespace aura
