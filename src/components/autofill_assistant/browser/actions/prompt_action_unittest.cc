// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/actions/prompt_action.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/timer/timer.h"
#include "components/autofill_assistant/browser/actions/mock_action_delegate.h"
#include "components/autofill_assistant/browser/wait_for_dom_observer.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::IsNull;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::SaveArgPointee;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::UnorderedElementsAre;
using ::testing::WithArgs;

class PromptActionTest : public testing::Test {
 public:
  PromptActionTest()
      : task_env_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    ON_CALL(mock_web_controller_, FindElement(_, _, _))
        .WillByDefault(WithArgs<2>([](auto&& callback) {
          std::move(callback).Run(ClientStatus(ELEMENT_RESOLUTION_FAILED),
                                  std::make_unique<ElementFinder::Result>());
        }));
    EXPECT_CALL(mock_action_delegate_, WaitForDom(_, _, _, _, _))
        .WillRepeatedly(Invoke(this, &PromptActionTest::FakeWaitForDom));
    ON_CALL(mock_action_delegate_, Prompt(_, _, _, _, _))
        .WillByDefault(
            [this](std::unique_ptr<std::vector<UserAction>> user_actions,
                   bool disable_force_expand_sheet,
                   base::OnceCallback<void()> callback, bool browse_mode,
                   bool browse_mode_invisible) {
              user_actions_ = std::move(user_actions);
            });
    prompt_proto_ = proto_.mutable_prompt();
  }

 protected:
  // Fakes ActionDelegate::WaitForDom.
  //
  // This simulates a WaitForDom that calls |check_elements_| every seconds
  // until it gets a successful callback, then calls done_waiting_callback.
  void FakeWaitForDom(
      base::TimeDelta max_wait_time,
      bool allow_interrupt,
      WaitForDomObserver* observer,
      base::RepeatingCallback<
          void(BatchElementChecker*,
               base::OnceCallback<void(const ClientStatus&)>)> check_elements,
      base::OnceCallback<void(const ClientStatus&, base::TimeDelta)>
          done_waiting_callback) {
    fake_wait_for_dom_done_ = std::move(done_waiting_callback);
    RunFakeWaitForDom(check_elements);
  }

  void RunFakeWaitForDom(
      base::RepeatingCallback<
          void(BatchElementChecker*,
               base::OnceCallback<void(const ClientStatus&)>)> check_elements) {
    if (!fake_wait_for_dom_done_)
      return;

    checker_ = std::make_unique<BatchElementChecker>();
    has_check_elements_result_ = false;
    check_elements.Run(checker_.get(),
                       base::BindOnce(&PromptActionTest::OnCheckElementsDone,
                                      base::Unretained(this)));
    task_env_.FastForwardBy(base::Milliseconds(fake_check_time_));
    checker_->AddAllDoneCallback(
        base::BindOnce(&PromptActionTest::OnWaitForDomDone,
                       base::Unretained(this), check_elements));
    checker_->Run(&mock_web_controller_);
  }

  // Called from the check_elements callback passed to FakeWaitForDom.
  void OnCheckElementsDone(const ClientStatus& result) {
    ASSERT_FALSE(has_check_elements_result_);  // Duplicate calls
    has_check_elements_result_ = true;
    check_elements_result_ = result;
  }

  // Called by |checker_| once it's done and either ends the WaitForDom or
  // schedule another run.
  void OnWaitForDomDone(
      base::RepeatingCallback<
          void(BatchElementChecker*,
               base::OnceCallback<void(const ClientStatus&)>)> check_elements) {
    ASSERT_TRUE(
        has_check_elements_result_);  // OnCheckElementsDone() not called

    if (!fake_wait_for_dom_done_)
      return;

    if (check_elements_result_.ok()) {
      std::move(fake_wait_for_dom_done_)
          .Run(check_elements_result_, base::Milliseconds(fake_wait_time_));
    } else {
      wait_for_dom_timer_ = std::make_unique<base::OneShotTimer>();
      wait_for_dom_timer_->Start(
          FROM_HERE, base::Seconds(1),
          base::BindOnce(&PromptActionTest::RunFakeWaitForDom,
                         base::Unretained(this), check_elements));
    }
  }

  // task_env_ must be first to guarantee other field
  // creation run in that environment.
  base::test::TaskEnvironment task_env_;

  MockActionDelegate mock_action_delegate_;
  MockWebController mock_web_controller_;
  base::MockCallback<Action::ProcessActionCallback> callback_;
  base::OnceCallback<void(const ClientStatus&, base::TimeDelta)>
      fake_wait_for_dom_done_;
  ActionProto proto_;
  raw_ptr<PromptProto> prompt_proto_;
  std::unique_ptr<std::vector<UserAction>> user_actions_;
  std::unique_ptr<BatchElementChecker> checker_;
  bool has_check_elements_result_ = false;
  ClientStatus check_elements_result_;
  std::unique_ptr<base::OneShotTimer> wait_for_dom_timer_;
  int fake_wait_time_ = 0;
  int fake_check_time_ = 0;
};

TEST_F(PromptActionTest, ChoicesMissing) {
  EXPECT_CALL(
      callback_,
      Run(Pointee(Property(&ProcessedActionProto::status, INVALID_ACTION))));
  PromptAction action(&mock_action_delegate_, proto_);
  action.ProcessAction(callback_.Get());
}

TEST_F(PromptActionTest, SelectButtons) {
  auto* ok_proto = prompt_proto_->add_choices();
  auto* chip = ok_proto->mutable_chip();
  chip->set_text("Ok");
  chip->set_type(HIGHLIGHTED_ACTION);
  ok_proto->set_server_payload("ok");

  auto* cancel_proto = prompt_proto_->add_choices();
  cancel_proto->mutable_chip()->set_text("Cancel");
  cancel_proto->mutable_chip()->set_type(NORMAL_ACTION);
  cancel_proto->set_server_payload("cancel");

  PromptAction action(&mock_action_delegate_, proto_);
  action.ProcessAction(callback_.Get());

  ASSERT_THAT(user_actions_, Pointee(SizeIs(2)));

  EXPECT_EQ("Ok", (*user_actions_)[0].chip().text);
  EXPECT_EQ(HIGHLIGHTED_ACTION, (*user_actions_)[0].chip().type);

  EXPECT_EQ("Cancel", (*user_actions_)[1].chip().text);
  EXPECT_EQ(NORMAL_ACTION, (*user_actions_)[1].chip().type);

  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(&ProcessedActionProto::prompt_choice,
                   Property(&PromptProto::Result::navigation_ended, false)),
          Property(&ProcessedActionProto::prompt_choice,
                   Property(&PromptProto::Result::server_payload, "ok"))))));
  EXPECT_TRUE((*user_actions_)[0].HasCallback());
  (*user_actions_)[0].RunCallback();
}

TEST_F(PromptActionTest, ShowOnlyIfElementExists) {
  auto* ok_proto = prompt_proto_->add_choices();
  ok_proto->mutable_chip()->set_text("Ok");
  ok_proto->mutable_chip()->set_type(HIGHLIGHTED_ACTION);
  ok_proto->set_server_payload("ok");
  *ok_proto->mutable_show_only_when()->mutable_match() =
      ToSelectorProto("element");

  PromptAction action(&mock_action_delegate_, proto_);
  action.ProcessAction(callback_.Get());

  ASSERT_THAT(user_actions_, Pointee(IsEmpty()));

  EXPECT_CALL(mock_web_controller_, FindElement(Selector({"element"}), _, _))
      .WillRepeatedly(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinder::Result>());
      }));
  task_env_.FastForwardBy(base::Seconds(1));
  ASSERT_THAT(user_actions_, Pointee(SizeIs(1)));

  EXPECT_CALL(mock_web_controller_, FindElement(Selector({"element"}), _, _))
      .WillRepeatedly(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(ClientStatus(ELEMENT_RESOLUTION_FAILED),
                                std::make_unique<ElementFinder::Result>());
      }));
  task_env_.FastForwardBy(base::Seconds(1));
  ASSERT_THAT(user_actions_, Pointee(IsEmpty()));
}

TEST_F(PromptActionTest, TimingStatsUserAction) {
  auto* ok_proto = prompt_proto_->add_choices();
  ok_proto->mutable_chip()->set_text("Ok");
  ok_proto->mutable_chip()->set_type(HIGHLIGHTED_ACTION);
  ok_proto->set_server_payload("ok");
  *ok_proto->mutable_show_only_when()->mutable_match() =
      ToSelectorProto("element");

  EXPECT_CALL(mock_web_controller_, FindElement(Selector({"element"}), _, _))
      .WillOnce(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(ClientStatus(ELEMENT_RESOLUTION_FAILED),
                                std::make_unique<ElementFinder::Result>());
      }))
      .WillOnce(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(ClientStatus(ELEMENT_RESOLUTION_FAILED),
                                std::make_unique<ElementFinder::Result>());
      }))
      .WillRepeatedly(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinder::Result>());
      }));

  fake_check_time_ = 200;
  PromptAction action(&mock_action_delegate_, proto_);
  action.ProcessAction(callback_.Get());

  task_env_.FastForwardBy(base::Seconds(3));
  ASSERT_THAT(user_actions_, Pointee(SizeIs(1)));

  ProcessedActionProto capture;
  EXPECT_CALL(callback_, Run(_)).WillOnce(SaveArgPointee<0>(&capture));
  EXPECT_TRUE((*user_actions_)[0].HasCallback());
  (*user_actions_)[0].RunCallback();
  EXPECT_EQ(capture.timing_stats().active_time_ms(), 700);
  EXPECT_EQ(capture.timing_stats().wait_time_ms(), 2500);
}

TEST_F(PromptActionTest, DisabledUnlessElementExists) {
  auto* ok_proto = prompt_proto_->add_choices();
  ok_proto->mutable_chip()->set_text("Ok");
  ok_proto->mutable_chip()->set_type(HIGHLIGHTED_ACTION);
  ok_proto->set_server_payload("ok");
  ok_proto->set_allow_disabling(true);
  *ok_proto->mutable_show_only_when()->mutable_match() =
      ToSelectorProto("element");

  PromptAction action(&mock_action_delegate_, proto_);
  action.ProcessAction(callback_.Get());

  EXPECT_CALL(mock_web_controller_, FindElement(Selector({"element"}), _, _))
      .WillRepeatedly(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinder::Result>());
      }));
  task_env_.FastForwardBy(base::Seconds(1));
  ASSERT_THAT(user_actions_, Pointee(SizeIs(1)));
  EXPECT_TRUE((*user_actions_)[0].enabled());

  EXPECT_CALL(mock_web_controller_, FindElement(Selector({"element"}), _, _))
      .WillRepeatedly(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(ClientStatus(ELEMENT_RESOLUTION_FAILED),
                                std::make_unique<ElementFinder::Result>());
      }));
  task_env_.FastForwardBy(base::Seconds(1));
  ASSERT_THAT(user_actions_, Pointee(SizeIs(1)));
  EXPECT_FALSE((*user_actions_)[0].enabled());
  EXPECT_TRUE((*user_actions_)[0].HasCallback());
}

TEST_F(PromptActionTest, AutoSelectWhenElementExists) {
  auto* choice_proto = prompt_proto_->add_choices();
  choice_proto->set_server_payload("auto-select");
  *choice_proto->mutable_auto_select_when()->mutable_match() =
      ToSelectorProto("element");

  PromptAction action(&mock_action_delegate_, proto_);
  action.ProcessAction(callback_.Get());
  EXPECT_THAT(user_actions_, Pointee(SizeIs(0)));

  EXPECT_CALL(mock_web_controller_, FindElement(Selector({"element"}), _, _))
      .WillRepeatedly(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinder::Result>());
      }));

  EXPECT_CALL(mock_action_delegate_, CleanUpAfterPrompt());
  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(Property(&ProcessedActionProto::status, ACTION_APPLIED),
                        Property(&ProcessedActionProto::prompt_choice,
                                 Property(&PromptProto::Result::server_payload,
                                          "auto-select"))))));
  task_env_.FastForwardBy(base::Seconds(1));
}

TEST_F(PromptActionTest, TimingStatsAutoSelect) {
  auto* choice_proto = prompt_proto_->add_choices();
  choice_proto->set_server_payload("auto-select");
  *choice_proto->mutable_auto_select_when()->mutable_match() =
      ToSelectorProto("element");

  fake_wait_time_ = 500;
  PromptAction action(&mock_action_delegate_, proto_);
  action.ProcessAction(callback_.Get());
  EXPECT_THAT(user_actions_, Pointee(SizeIs(0)));

  EXPECT_CALL(mock_web_controller_, FindElement(Selector({"element"}), _, _))
      .WillRepeatedly(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinder::Result>());
      }));

  EXPECT_CALL(mock_action_delegate_, CleanUpAfterPrompt());
  ProcessedActionProto capture;
  EXPECT_CALL(callback_, Run(_)).WillOnce(SaveArgPointee<0>(&capture));
  task_env_.FastForwardBy(base::Seconds(1));
  EXPECT_EQ(capture.timing_stats().active_time_ms(), 500);
  EXPECT_EQ(capture.timing_stats().wait_time_ms(), 500);
}

TEST_F(PromptActionTest, AutoSelectWithButton) {
  auto* ok_proto = prompt_proto_->add_choices();
  ok_proto->mutable_chip()->set_text("Ok");
  ok_proto->mutable_chip()->set_type(HIGHLIGHTED_ACTION);
  ok_proto->set_server_payload("ok");

  auto* choice_proto = prompt_proto_->add_choices();
  choice_proto->set_server_payload("auto-select");
  *choice_proto->mutable_auto_select_when()->mutable_match() =
      ToSelectorProto("element");

  PromptAction action(&mock_action_delegate_, proto_);
  action.ProcessAction(callback_.Get());

  ASSERT_THAT(user_actions_, Pointee(SizeIs(1)));

  EXPECT_CALL(mock_web_controller_, FindElement(Selector({"element"}), _, _))
      .WillRepeatedly(WithArgs<2>([](auto&& callback) {
        std::move(callback).Run(OkClientStatus(),
                                std::make_unique<ElementFinder::Result>());
      }));
  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(Property(&ProcessedActionProto::status, ACTION_APPLIED),
                        Property(&ProcessedActionProto::prompt_choice,
                                 Property(&PromptProto::Result::server_payload,
                                          "auto-select"))))));
  task_env_.FastForwardBy(base::Seconds(1));
}

TEST_F(PromptActionTest, Terminate) {
  auto* ok_proto = prompt_proto_->add_choices();
  ok_proto->mutable_chip()->set_text("Ok");
  ok_proto->mutable_chip()->set_type(HIGHLIGHTED_ACTION);
  ok_proto->set_server_payload("ok");
  {
    PromptAction action(&mock_action_delegate_, proto_);
    action.ProcessAction(callback_.Get());
  }

  // Chips pointing to a deleted action do nothing.
  ASSERT_THAT(user_actions_, Pointee(SizeIs(1)));
  EXPECT_TRUE((*user_actions_)[0].HasCallback());
  (*user_actions_)[0].RunCallback();
}

TEST_F(PromptActionTest, NoMessageSet) {
  auto* ok_proto = prompt_proto_->add_choices();
  ok_proto->mutable_chip()->set_text("Ok");
  ok_proto->mutable_chip()->set_type(HIGHLIGHTED_ACTION);
  ok_proto->set_server_payload("ok");

  EXPECT_CALL(mock_action_delegate_, SetStatusMessage(_)).Times(0);

  PromptAction action(&mock_action_delegate_, proto_);
  action.ProcessAction(callback_.Get());
}

TEST_F(PromptActionTest, EmptyMessage) {
  prompt_proto_->set_message("");
  auto* ok_proto = prompt_proto_->add_choices();
  ok_proto->mutable_chip()->set_text("Ok");
  ok_proto->mutable_chip()->set_type(HIGHLIGHTED_ACTION);
  ok_proto->set_server_payload("ok");

  EXPECT_CALL(mock_action_delegate_, SetStatusMessage(StrEq("")));

  PromptAction action(&mock_action_delegate_, proto_);
  action.ProcessAction(callback_.Get());
}

TEST_F(PromptActionTest, NormalMessageSet) {
  prompt_proto_->set_message(" test message ");
  auto* ok_proto = prompt_proto_->add_choices();
  ok_proto->mutable_chip()->set_text("Ok");
  ok_proto->mutable_chip()->set_type(HIGHLIGHTED_ACTION);
  ok_proto->set_server_payload("ok");

  EXPECT_CALL(mock_action_delegate_, SetStatusMessage(StrEq(" test message ")));

  PromptAction action(&mock_action_delegate_, proto_);
  action.ProcessAction(callback_.Get());
}

TEST_F(PromptActionTest, ForceExpandSheetDefault) {
  auto* ok_proto = prompt_proto_->add_choices();
  ok_proto->mutable_chip()->set_text("Ok");
  ok_proto->mutable_chip()->set_type(HIGHLIGHTED_ACTION);
  ok_proto->set_server_payload("ok");

  EXPECT_CALL(mock_action_delegate_, Prompt(_, false, _, false, false));
  PromptAction action(&mock_action_delegate_, proto_);
  action.ProcessAction(callback_.Get());
}

TEST_F(PromptActionTest, ForceExpandSheetDisable) {
  auto* ok_proto = prompt_proto_->add_choices();
  ok_proto->mutable_chip()->set_text("Ok");
  ok_proto->mutable_chip()->set_type(HIGHLIGHTED_ACTION);
  ok_proto->set_server_payload("ok");

  prompt_proto_->set_disable_force_expand_sheet(true);
  EXPECT_CALL(mock_action_delegate_, Prompt(_, true, _, false, false));
  PromptAction action(&mock_action_delegate_, proto_);
  action.ProcessAction(callback_.Get());
}

TEST_F(PromptActionTest, RunPromptInBrowseMode) {
  auto* ok_proto = prompt_proto_->add_choices();
  ok_proto->mutable_chip()->set_text("Ok");
  ok_proto->mutable_chip()->set_type(HIGHLIGHTED_ACTION);
  ok_proto->set_server_payload("ok");

  prompt_proto_->set_browse_mode(true);
  EXPECT_CALL(mock_action_delegate_, Prompt(_, false, _, true, false));
  PromptAction action(&mock_action_delegate_, proto_);
  action.ProcessAction(callback_.Get());
}

TEST_F(PromptActionTest, RunPromptInInvisibleBrowseMode) {
  auto* ok_proto = prompt_proto_->add_choices();
  ok_proto->mutable_chip()->set_text("Ok");
  ok_proto->mutable_chip()->set_type(HIGHLIGHTED_ACTION);
  ok_proto->set_server_payload("ok");

  prompt_proto_->set_browse_mode(true);
  prompt_proto_->set_browse_mode_invisible(true);
  EXPECT_CALL(mock_action_delegate_, Prompt(_, false, _, true, true));
  PromptAction action(&mock_action_delegate_, proto_);
  action.ProcessAction(callback_.Get());
}

TEST_F(PromptActionTest, ForwardInterruptFailure) {
  prompt_proto_->set_allow_interrupt(true);
  auto* choice_proto = prompt_proto_->add_choices();
  choice_proto->set_server_payload("auto-select");
  *choice_proto->mutable_auto_select_when()->mutable_match() =
      ToSelectorProto("element");

  PromptAction action(&mock_action_delegate_, proto_);
  action.ProcessAction(callback_.Get());
  EXPECT_THAT(user_actions_, Pointee(SizeIs(0)));

  // First round of element checks: element doesn't exist.
  task_env_.FastForwardBy(base::Seconds(1));

  // Second round of element checks: an interrupt ran and failed. No choice was
  // selected.
  EXPECT_CALL(
      callback_,
      Run(AllOf(
          Pointee(Property(&ProcessedActionProto::status, INTERRUPT_FAILED)),
          Pointee(
              Property(&ProcessedActionProto::prompt_choice,
                       Property(&PromptProto::Result::server_payload, ""))))));
  ASSERT_TRUE(fake_wait_for_dom_done_);
  std::move(fake_wait_for_dom_done_)
      .Run(ClientStatus(INTERRUPT_FAILED), base::Seconds(0));
}

TEST_F(PromptActionTest, EndActionOnNavigation) {
  EXPECT_CALL(mock_action_delegate_, Prompt(_, _, _, _, _))
      .WillOnce([this](std::unique_ptr<std::vector<UserAction>> user_actions,
                       bool disable_force_expand_sheet,
                       base::OnceCallback<void()> callback, bool browse_mode,
                       bool browse_mode_invisible) {
        user_actions_ = std::move(user_actions);
        std::move(callback).Run();
      });

  prompt_proto_->set_end_on_navigation(true);
  prompt_proto_->add_choices()->mutable_chip()->set_text("ok");

  PromptAction action(&mock_action_delegate_, proto_);

  // Set new expectations for when the navigation event arrives.
  EXPECT_CALL(mock_action_delegate_, CleanUpAfterPrompt());
  EXPECT_CALL(
      callback_,
      Run(Pointee(AllOf(
          Property(&ProcessedActionProto::status, ACTION_APPLIED),
          Property(&ProcessedActionProto::prompt_choice,
                   Property(&PromptProto::Result::navigation_ended, true))))));

  action.ProcessAction(callback_.Get());
}

TEST_F(PromptActionTest, TimingStatsEndActionOnNavigation) {
  auto timer = std::make_unique<base::OneShotTimer>();
  EXPECT_CALL(mock_action_delegate_, Prompt(_, _, _, _, _))
      .WillOnce(
          [this, &timer](std::unique_ptr<std::vector<UserAction>> user_actions,
                         bool disable_force_expand_sheet,
                         base::OnceCallback<void()> callback, bool browse_mode,
                         bool browse_mode_invisible) {
            user_actions_ = std::move(user_actions);
            timer->Start(FROM_HERE, base::Seconds(1), std::move(callback));
          });

  prompt_proto_->set_end_on_navigation(true);
  PromptProto_Choice* ok_proto = prompt_proto_->add_choices();
  ok_proto->mutable_chip()->set_text("ok");

  PromptAction action(&mock_action_delegate_, proto_);

  // Set new expectations for when the navigation event arrives.
  EXPECT_CALL(mock_action_delegate_, CleanUpAfterPrompt());
  ProcessedActionProto capture;
  EXPECT_CALL(callback_, Run(_)).WillOnce(SaveArgPointee<0>(&capture));
  action.ProcessAction(callback_.Get());
  EXPECT_TRUE(task_env_.NextTaskIsDelayed());
  task_env_.DescribeCurrentTasks();
  task_env_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(capture.timing_stats().wait_time_ms(), 1000);
}

}  // namespace
}  // namespace autofill_assistant
