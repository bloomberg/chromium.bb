// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/message_center_impl.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification_types.h"

namespace message_center {
namespace {

class MessageCenterImplTest : public testing::Test,
                              public MessageCenterObserver {
 public:
  MessageCenterImplTest() {}

  virtual void SetUp() OVERRIDE {
    MessageCenter::Initialize();
    message_center_ = MessageCenter::Get();
    loop_.reset(new base::MessageLoop(base::MessageLoop::TYPE_DEFAULT));
    run_loop_.reset(new base::RunLoop());
    closure_ = run_loop_->QuitClosure();
  }

  virtual void TearDown() OVERRIDE {
    run_loop_.reset();
    loop_.reset();
    message_center_ = NULL;
    MessageCenter::Shutdown();
  }

  MessageCenter* message_center() const { return message_center_; }
  base::RunLoop* run_loop() const { return run_loop_.get(); }
  base::Closure closure() const { return closure_; }

 private:
  MessageCenter* message_center_;
  scoped_ptr<base::MessageLoop> loop_;
  scoped_ptr<base::RunLoop> run_loop_;
  base::Closure closure_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterImplTest);
};

}  // namespace

namespace internal {

class MockPopupTimersController : public PopupTimersController {
 public:
  MockPopupTimersController(MessageCenter* message_center,
                            base::Closure quit_closure)
      : PopupTimersController(message_center),
        timer_finished_(false),
        quit_closure_(quit_closure) {}
  virtual ~MockPopupTimersController() {}

  virtual void TimerFinished(const std::string& id) OVERRIDE {
    LOG(INFO) << "In timer finished for id " << id;
    base::MessageLoop::current()->PostTask(FROM_HERE, quit_closure_);
    timer_finished_ = true;
    last_id_ = id;
  }

  bool timer_finished() const { return timer_finished_; }
  const std::string& last_id() const { return last_id_; }

 private:
  bool timer_finished_;
  std::string last_id_;
  base::Closure quit_closure_;
};

TEST_F(MessageCenterImplTest, PopupTimersEmptyController) {
  scoped_ptr<PopupTimersController> popup_timers_controller =
      make_scoped_ptr(new PopupTimersController(message_center()));

  // Test that all functions succed without any timers created.
  popup_timers_controller->PauseAll();
  popup_timers_controller->StartAll();
  popup_timers_controller->CancelAll();
  popup_timers_controller->TimerFinished("unknown");
  popup_timers_controller->PauseTimer("unknown");
  popup_timers_controller->CancelTimer("unknown");
}

TEST_F(MessageCenterImplTest, PopupTimersControllerStartTimer) {
  scoped_ptr<MockPopupTimersController> popup_timers_controller =
      make_scoped_ptr(
          new MockPopupTimersController(message_center(), closure()));
  popup_timers_controller->StartTimer("test",
                                      base::TimeDelta::FromMilliseconds(1));
  run_loop()->Run();
  EXPECT_TRUE(popup_timers_controller->timer_finished());
}

TEST_F(MessageCenterImplTest, PopupTimersControllerPauseTimer) {
  scoped_ptr<MockPopupTimersController> popup_timers_controller =
      make_scoped_ptr(
          new MockPopupTimersController(message_center(), closure()));
  popup_timers_controller->StartTimer("test",
                                      base::TimeDelta::FromMilliseconds(1));
  popup_timers_controller->PauseTimer("test");
  run_loop()->RunUntilIdle();

  EXPECT_FALSE(popup_timers_controller->timer_finished());
}

TEST_F(MessageCenterImplTest, PopupTimersControllerCancelTimer) {
  scoped_ptr<MockPopupTimersController> popup_timers_controller =
      make_scoped_ptr(
          new MockPopupTimersController(message_center(), closure()));
  popup_timers_controller->StartTimer("test",
                                      base::TimeDelta::FromMilliseconds(1));
  popup_timers_controller->CancelTimer("test");
  run_loop()->RunUntilIdle();

  EXPECT_FALSE(popup_timers_controller->timer_finished());
}

TEST_F(MessageCenterImplTest, PopupTimersControllerPauseAllTimers) {
  scoped_ptr<MockPopupTimersController> popup_timers_controller =
      make_scoped_ptr(
          new MockPopupTimersController(message_center(), closure()));
  popup_timers_controller->StartTimer("test",
                                      base::TimeDelta::FromMilliseconds(1));
  popup_timers_controller->PauseAll();
  run_loop()->RunUntilIdle();

  EXPECT_FALSE(popup_timers_controller->timer_finished());
}

TEST_F(MessageCenterImplTest, PopupTimersControllerStartAllTimers) {
  scoped_ptr<MockPopupTimersController> popup_timers_controller =
      make_scoped_ptr(
          new MockPopupTimersController(message_center(), closure()));
  popup_timers_controller->StartTimer("test",
                                      base::TimeDelta::FromMilliseconds(1));
  popup_timers_controller->PauseAll();
  popup_timers_controller->StartAll();
  run_loop()->Run();

  EXPECT_TRUE(popup_timers_controller->timer_finished());
}

TEST_F(MessageCenterImplTest, PopupTimersControllerStartMultipleTimers) {
  scoped_ptr<MockPopupTimersController> popup_timers_controller =
      make_scoped_ptr(
          new MockPopupTimersController(message_center(), closure()));
  popup_timers_controller->StartTimer("test",
                                      base::TimeDelta::FromMilliseconds(5));
  popup_timers_controller->StartTimer("test2",
                                      base::TimeDelta::FromMilliseconds(1));
  popup_timers_controller->StartTimer("test3",
                                      base::TimeDelta::FromMilliseconds(3));
  popup_timers_controller->PauseAll();
  popup_timers_controller->StartAll();
  run_loop()->Run();

  EXPECT_EQ(popup_timers_controller->last_id(), "test2");
  EXPECT_TRUE(popup_timers_controller->timer_finished());
}

TEST_F(MessageCenterImplTest, PopupTimersControllerStartMultipleTimersPause) {
  scoped_ptr<MockPopupTimersController> popup_timers_controller =
      make_scoped_ptr(
          new MockPopupTimersController(message_center(), closure()));
  popup_timers_controller->StartTimer("test",
                                      base::TimeDelta::FromMilliseconds(5));
  popup_timers_controller->StartTimer("test2",
                                      base::TimeDelta::FromMilliseconds(1));
  popup_timers_controller->StartTimer("test3",
                                      base::TimeDelta::FromMilliseconds(3));
  popup_timers_controller->PauseTimer("test2");

  run_loop()->Run();

  EXPECT_EQ(popup_timers_controller->last_id(), "test3");
  EXPECT_TRUE(popup_timers_controller->timer_finished());
}

TEST_F(MessageCenterImplTest, PopupTimersControllerResetTimer) {
  scoped_ptr<MockPopupTimersController> popup_timers_controller =
      make_scoped_ptr(
          new MockPopupTimersController(message_center(), closure()));
  popup_timers_controller->StartTimer("test",
                                      base::TimeDelta::FromMilliseconds(5));
  popup_timers_controller->StartTimer("test2",
                                      base::TimeDelta::FromMilliseconds(1));
  popup_timers_controller->StartTimer("test3",
                                      base::TimeDelta::FromMilliseconds(3));
  popup_timers_controller->PauseTimer("test2");
  popup_timers_controller->ResetTimer("test",
                                      base::TimeDelta::FromMilliseconds(2));

  run_loop()->Run();

  EXPECT_EQ(popup_timers_controller->last_id(), "test");
  EXPECT_TRUE(popup_timers_controller->timer_finished());
}

}  // namespace internal

}  // namespace message_center
