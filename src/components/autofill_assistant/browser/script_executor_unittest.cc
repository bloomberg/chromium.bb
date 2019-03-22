// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_executor.h"

#include <map>
#include <utility>

#include "base/strings/strcat.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_task_environment.h"
#include "components/autofill_assistant/browser/client_memory.h"
#include "components/autofill_assistant/browser/mock_run_once_callback.h"
#include "components/autofill_assistant/browser/mock_service.h"
#include "components/autofill_assistant/browser/mock_ui_controller.h"
#include "components/autofill_assistant/browser/mock_web_controller.h"
#include "components/autofill_assistant/browser/service.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Not;
using ::testing::Pair;
using ::testing::ReturnRef;
using ::testing::SaveArg;
using ::testing::SizeIs;
using ::testing::StrEq;
using ::testing::StrictMock;

const char* kScriptPath = "script_path";

class ScriptExecutorTest : public testing::Test,
                           public ScriptExecutorDelegate,
                           public ScriptExecutor::Listener {
 public:
  void SetUp() override {
    executor_ = std::make_unique<ScriptExecutor>(
        kScriptPath,
        /* global_payload= */ "initial global payload",
        /* script_payload= */ "initial payload",
        /* listener= */ this, &scripts_state_, &ordered_interrupts_,
        /* delegate= */ this);
    url_ = GURL("http://example.com/");

    // In this test, "tell" actions always succeed and "click" actions always
    // fail. The following makes a click action fail immediately
    ON_CALL(mock_web_controller_, OnClickOrTapElement(_, _))
        .WillByDefault(RunOnceCallback<1>(false));

    ON_CALL(mock_web_controller_, OnElementCheck(_, _, _))
        .WillByDefault(RunOnceCallback<2>(true));
    ON_CALL(mock_web_controller_, OnFocusElement(_, _))
        .WillByDefault(RunOnceCallback<1>(true));
    ON_CALL(mock_web_controller_, GetUrl()).WillByDefault(ReturnRef(url_));
    ON_CALL(mock_ui_controller_, ShowOverlay()).WillByDefault(Invoke([this]() {
      overlay_ = true;
    }));
    ON_CALL(mock_ui_controller_, HideOverlay()).WillByDefault(Invoke([this]() {
      overlay_ = false;
    }));
  }

 protected:
  ScriptExecutorTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::MOCK_TIME),
        overlay_(false) {}

  // Implements ScriptExecutorDelegate
  Service* GetService() override { return &mock_service_; }

  UiController* GetUiController() override { return &mock_ui_controller_; }

  WebController* GetWebController() override { return &mock_web_controller_; }

  ClientMemory* GetClientMemory() override { return &memory_; }

  void SetTouchableElementArea(const std::vector<Selector>& elements) {}

  const std::map<std::string, std::string>& GetParameters() override {
    return parameters_;
  }

  autofill::PersonalDataManager* GetPersonalDataManager() override {
    return nullptr;
  }

  content::WebContents* GetWebContents() override { return nullptr; }

  // Implements ScriptExecutor::Listener
  void OnServerPayloadChanged(const std::string& global_payload,
                              const std::string& script_payload) override {
    last_global_payload_ = global_payload;
    last_script_payload_ = script_payload;
  }

  void OnScriptListChanged(
      std::vector<std::unique_ptr<Script>> scripts) override {
    should_update_scripts_ = true;
    scripts_update_ = std::move(scripts);
    ++scripts_update_count_;
  }

  std::string Serialize(const google::protobuf::MessageLite& message) {
    std::string output;
    message.SerializeToString(&output);
    return output;
  }

  // Creates a script that contains a wait_for_dom allow_interrupt=true followed
  // by a tell. It will succeed if |element| eventually becomes visible.
  void SetupInterruptibleScript(const std::string& path,
                                const std::string& element) {
    ActionsResponseProto interruptible;
    interruptible.set_global_payload("main script global payload");
    interruptible.set_script_payload("main script payload");
    auto* wait_action = interruptible.add_actions()->mutable_wait_for_dom();
    wait_action->add_selectors(element);
    wait_action->set_allow_interrupt(true);
    interruptible.add_actions()->mutable_tell()->set_message(path);
    EXPECT_CALL(mock_service_, OnGetActions(StrEq(path), _, _, _, _, _))
        .WillOnce(RunOnceCallback<5>(true, Serialize(interruptible)));
  }

  // Creates an interrupt that contains a tell. It will always succeed.
  void SetupInterrupt(const std::string& path, const std::string& trigger) {
    RegisterInterrupt(path, trigger);

    ActionsResponseProto interrupt_actions;
    interrupt_actions.set_global_payload(
        base::StrCat({"global payload for ", path}));
    interrupt_actions.set_script_payload(base::StrCat({"payload for ", path}));
    interrupt_actions.add_actions()->mutable_tell()->set_message(path);
    EXPECT_CALL(mock_service_, OnGetActions(StrEq(path), _, _, _, _, _))
        .WillRepeatedly(RunOnceCallback<5>(true, Serialize(interrupt_actions)));
  }

  // Registers an interrupt, but do not define actions for it.
  void RegisterInterrupt(const std::string& path, const std::string& trigger) {
    auto interrupt = std::make_unique<Script>();
    interrupt->handle.path = path;
    ScriptPreconditionProto interrupt_preconditions;
    interrupt_preconditions.add_elements_exist()->add_selectors(trigger);
    auto* run_once = interrupt_preconditions.add_script_status_match();
    run_once->set_script(path);
    run_once->set_status(SCRIPT_STATUS_NOT_RUN);
    interrupt->precondition =
        ScriptPrecondition::FromProto(path, interrupt_preconditions);

    ordered_interrupts_.push_back(interrupt.get());
    interrupts_.emplace_back(std::move(interrupt));
  }

  // scoped_task_environment_ must be first to guarantee other field
  // creation run in that environment.
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  Script script_;
  ClientMemory memory_;
  StrictMock<MockService> mock_service_;
  NiceMock<MockWebController> mock_web_controller_;
  NiceMock<MockUiController> mock_ui_controller_;
  std::map<std::string, ScriptStatusProto> scripts_state_;

  // An owner for the pointers in |ordered_interrupts_|
  std::vector<std::unique_ptr<Script>> interrupts_;
  std::vector<Script*> ordered_interrupts_;
  std::string last_global_payload_;
  std::string last_script_payload_;
  bool should_update_scripts_ = false;
  std::vector<std::unique_ptr<Script>> scripts_update_;
  int scripts_update_count_ = 0;
  std::unique_ptr<ScriptExecutor> executor_;
  std::map<std::string, std::string> parameters_;
  StrictMock<base::MockCallback<ScriptExecutor::RunScriptCallback>>
      executor_callback_;
  GURL url_;
  bool overlay_;
};

TEST_F(ScriptExecutorTest, GetActionsFails) {
  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(false, ""));
  EXPECT_CALL(executor_callback_,
              Run(AllOf(Field(&ScriptExecutor::Result::success, false),
                        Field(&ScriptExecutor::Result::at_end,
                              ScriptExecutor::CONTINUE))));
  executor_->Run(executor_callback_.Get());
}

TEST_F(ScriptExecutorTest, ForwardParameters) {
  parameters_["param1"] = "value1";
  parameters_["param2"] = "value2";
  EXPECT_CALL(mock_service_,
              OnGetActions(StrEq(kScriptPath), _,
                           AllOf(Contains(Pair("param1", "value1")),
                                 Contains(Pair("param2", "value2"))),
                           _, _, _))
      .WillOnce(RunOnceCallback<5>(true, ""));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(executor_callback_.Get());
}

TEST_F(ScriptExecutorTest, RunOneActionReportAndReturn) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()
      ->mutable_click()
      ->mutable_element_to_click()
      ->add_selectors("will fail");

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));

  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _))
      .WillOnce(DoAll(SaveArg<2>(&processed_actions_capture),
                      RunOnceCallback<3>(true, "")));
  EXPECT_CALL(executor_callback_,
              Run(AllOf(Field(&ScriptExecutor::Result::success, true),
                        Field(&ScriptExecutor::Result::at_end,
                              ScriptExecutor::CONTINUE))));
  executor_->Run(executor_callback_.Get());

  ASSERT_EQ(1u, processed_actions_capture.size());
  EXPECT_EQ(OTHER_ACTION_STATUS, processed_actions_capture[0].status());
}

TEST_F(ScriptExecutorTest, RunMultipleActions) {
  ActionsResponseProto initial_actions_response;
  initial_actions_response.add_actions()->mutable_tell()->set_message("1");
  initial_actions_response.add_actions()->mutable_tell()->set_message("2");
  EXPECT_CALL(mock_service_, OnGetActions(StrEq(kScriptPath), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(initial_actions_response)));

  ActionsResponseProto next_actions_response;
  next_actions_response.add_actions()->mutable_tell()->set_message("3");
  std::vector<ProcessedActionProto> processed_actions1_capture;
  std::vector<ProcessedActionProto> processed_actions2_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _))
      .WillOnce(
          DoAll(SaveArg<2>(&processed_actions1_capture),
                RunOnceCallback<3>(true, Serialize(next_actions_response))))
      .WillOnce(DoAll(SaveArg<2>(&processed_actions2_capture),
                      RunOnceCallback<3>(true, "")));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(executor_callback_.Get());

  EXPECT_EQ(2u, processed_actions1_capture.size());
  EXPECT_EQ(1u, processed_actions2_capture.size());
}

TEST_F(ScriptExecutorTest, UnsupportedAction) {
  ActionsResponseProto actions_response;
  actions_response.add_actions();  // action definition missing

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));

  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _))
      .WillOnce(DoAll(SaveArg<2>(&processed_actions_capture),
                      RunOnceCallback<3>(true, "")));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(executor_callback_.Get());

  ASSERT_EQ(1u, processed_actions_capture.size());
  EXPECT_EQ(UNKNOWN_ACTION_STATUS, processed_actions_capture[0].status());
}

TEST_F(ScriptExecutorTest, StopAfterEnd) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_stop();

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));

  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _))
      .WillOnce(RunOnceCallback<3>(true, ""));
  EXPECT_CALL(executor_callback_,
              Run(AllOf(Field(&ScriptExecutor::Result::success, true),
                        Field(&ScriptExecutor::Result::at_end,
                              ScriptExecutor::SHUTDOWN))));
  executor_->Run(executor_callback_.Get());
}

TEST_F(ScriptExecutorTest, ResetAfterEnd) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_reset();

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));

  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _))
      .WillOnce(RunOnceCallback<3>(true, ""));
  EXPECT_CALL(executor_callback_,
              Run(AllOf(Field(&ScriptExecutor::Result::success, true),
                        Field(&ScriptExecutor::Result::at_end,
                              ScriptExecutor::RESTART))));
  executor_->Run(executor_callback_.Get());
}

TEST_F(ScriptExecutorTest, InterruptActionListOnError) {
  ActionsResponseProto initial_actions_response;
  initial_actions_response.add_actions()->mutable_tell()->set_message(
      "will pass");
  initial_actions_response.add_actions()
      ->mutable_click()
      ->mutable_element_to_click()
      ->add_selectors("will fail");
  initial_actions_response.add_actions()->mutable_tell()->set_message(
      "never run");

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(initial_actions_response)));

  ActionsResponseProto next_actions_response;
  next_actions_response.add_actions()->mutable_tell()->set_message(
      "will run after error");
  std::vector<ProcessedActionProto> processed_actions1_capture;
  std::vector<ProcessedActionProto> processed_actions2_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _))
      .WillOnce(
          DoAll(SaveArg<2>(&processed_actions1_capture),
                RunOnceCallback<3>(true, Serialize(next_actions_response))))
      .WillOnce(DoAll(SaveArg<2>(&processed_actions2_capture),
                      RunOnceCallback<3>(true, "")));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(executor_callback_.Get());

  ASSERT_EQ(2u, processed_actions1_capture.size());
  EXPECT_EQ(ACTION_APPLIED, processed_actions1_capture[0].status());
  EXPECT_EQ(OTHER_ACTION_STATUS, processed_actions1_capture[1].status());

  ASSERT_EQ(1u, processed_actions2_capture.size());
  EXPECT_EQ(ACTION_APPLIED, processed_actions2_capture[0].status());
  // make sure "never run" wasn't the one that was run.
  EXPECT_EQ("will run after error",
            processed_actions2_capture[0].action().tell().message());
}

TEST_F(ScriptExecutorTest, RunDelayedAction) {
  ActionsResponseProto actions_response;
  ActionProto* action = actions_response.add_actions();
  action->mutable_tell()->set_message("delayed");
  action->set_action_delay_ms(1000);

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));

  std::vector<ProcessedActionProto> processed_actions_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _))
      .WillOnce(DoAll(SaveArg<2>(&processed_actions_capture),
                      RunOnceCallback<3>(true, "")));

  // executor_callback_.Run() not expected to be run just yet, as the action is
  // delayed.
  executor_->Run(executor_callback_.Get());
  EXPECT_TRUE(scoped_task_environment_.MainThreadHasPendingTask());

  // Moving forward in time triggers action execution.
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  scoped_task_environment_.FastForwardBy(
      base::TimeDelta::FromMilliseconds(1000));
  EXPECT_FALSE(scoped_task_environment_.MainThreadHasPendingTask());
}

TEST_F(ScriptExecutorTest, HideDetailsWhenFinished) {
  ActionsResponseProto actions_response;
  ActionProto click_with_clean_contextual_ui;
  click_with_clean_contextual_ui.set_clean_contextual_ui(true);
  click_with_clean_contextual_ui.mutable_tell()->set_message("clean");

  *actions_response.add_actions() = click_with_clean_contextual_ui;

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _))
      .WillOnce(RunOnceCallback<3>(true, ""));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  EXPECT_CALL(mock_ui_controller_, HideDetails());
  executor_->Run(executor_callback_.Get());
}

TEST_F(ScriptExecutorTest, DontHideDetailsIfOtherActionsAreLeft) {
  ActionsResponseProto actions_response;
  ActionProto click_with_clean_contextual_ui;
  click_with_clean_contextual_ui.set_clean_contextual_ui(true);
  click_with_clean_contextual_ui.mutable_tell()->set_message("clean");
  *actions_response.add_actions() = click_with_clean_contextual_ui;
  actions_response.add_actions()->mutable_tell()->set_message("Wait no!");

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _))
      .WillOnce(RunOnceCallback<3>(true, ""));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));

  EXPECT_CALL(mock_ui_controller_, HideDetails()).Times(0);

  executor_->Run(executor_callback_.Get());
}

TEST_F(ScriptExecutorTest, HideDetailsOnError) {
  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_tell()->set_message("Hello");
  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _))
      .WillOnce(RunOnceCallback<3>(false, ""));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, false)));

  EXPECT_CALL(mock_ui_controller_, HideDetails());

  executor_->Run(executor_callback_.Get());
}

TEST_F(ScriptExecutorTest, UpdateScriptStateWhileRunning) {
  // OnGetNextActions never calls the callback, so Run() returns immediately
  // without doing anything.
  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _));

  EXPECT_THAT(scripts_state_, IsEmpty());
  executor_->Run(executor_callback_.Get());
  EXPECT_THAT(scripts_state_,
              Contains(Pair(kScriptPath, SCRIPT_STATUS_RUNNING)));
}

TEST_F(ScriptExecutorTest, UpdateScriptStateOnError) {
  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(false, ""));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, false)));
  executor_->Run(executor_callback_.Get());

  EXPECT_THAT(scripts_state_,
              Contains(Pair(kScriptPath, SCRIPT_STATUS_FAILURE)));
}

TEST_F(ScriptExecutorTest, UpdateScriptStateOnSuccess) {
  ActionsResponseProto initial_actions_response;
  initial_actions_response.add_actions()->mutable_tell()->set_message("ok");
  EXPECT_CALL(mock_service_, OnGetActions(StrEq(kScriptPath), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(initial_actions_response)));
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _))
      .WillOnce(RunOnceCallback<3>(true, ""));
  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(executor_callback_.Get());

  EXPECT_THAT(scripts_state_,
              Contains(Pair(kScriptPath, SCRIPT_STATUS_SUCCESS)));
}

TEST_F(ScriptExecutorTest, ForwardLastPayloadOnSuccess) {
  ActionsResponseProto actions_response;
  actions_response.set_global_payload("actions global payload");
  actions_response.set_script_payload("actions payload");
  actions_response.add_actions()->mutable_tell()->set_message("ok");

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, "initial global payload",
                                          "initial payload", _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));

  ActionsResponseProto next_actions_response;
  next_actions_response.set_global_payload("last global payload");
  next_actions_response.set_script_payload("last payload");
  EXPECT_CALL(mock_service_, OnGetNextActions("actions global payload",
                                              "actions payload", _, _))
      .WillOnce(RunOnceCallback<3>(true, Serialize(next_actions_response)));

  EXPECT_CALL(executor_callback_, Run(_));
  executor_->Run(executor_callback_.Get());

  EXPECT_EQ("last global payload", last_global_payload_);
  EXPECT_EQ("last payload", last_script_payload_);
}

TEST_F(ScriptExecutorTest, ForwardLastPayloadOnError) {
  ActionsResponseProto actions_response;
  actions_response.set_global_payload("actions global payload");
  actions_response.set_script_payload("actions payload");
  actions_response.add_actions()->mutable_tell()->set_message("ok");

  EXPECT_CALL(mock_service_, OnGetActions(_, _, _, "initial global payload",
                                          "initial payload", _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));

  EXPECT_CALL(mock_service_, OnGetNextActions("actions global payload",
                                              "actions payload", _, _))
      .WillOnce(RunOnceCallback<3>(false, ""));

  EXPECT_CALL(executor_callback_, Run(_));
  executor_->Run(executor_callback_.Get());

  EXPECT_EQ("actions global payload", last_global_payload_);
  EXPECT_EQ("actions payload", last_script_payload_);
}

TEST_F(ScriptExecutorTest, RunInterrupt) {
  // All elements exist, so first the interrupt should be run, then the element
  // should be reported as found.
  SetupInterruptibleScript(kScriptPath, "element");
  SetupInterrupt("interrupt", "interrupt_trigger");

  // Both scripts ends after the first set of actions. Capture the results.
  std::vector<ProcessedActionProto> processed_actions1_capture;
  std::vector<ProcessedActionProto> processed_actions2_capture;
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _))
      .WillOnce(DoAll(SaveArg<2>(&processed_actions1_capture),
                      RunOnceCallback<3>(true, "")))
      .WillOnce(DoAll(SaveArg<2>(&processed_actions2_capture),
                      RunOnceCallback<3>(true, "")));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(executor_callback_.Get());

  EXPECT_THAT(scripts_state_,
              Contains(Pair(kScriptPath, SCRIPT_STATUS_SUCCESS)));
  EXPECT_THAT(scripts_state_,
              Contains(Pair("interrupt", SCRIPT_STATUS_SUCCESS)));

  // The first script to call OnGetNextActions is the interrupt, which starts
  // with a tell.
  ASSERT_THAT(processed_actions1_capture, Not(IsEmpty()));
  EXPECT_EQ(ActionProto::ActionInfoCase::kTell,
            processed_actions1_capture[0].action().action_info_case());
  EXPECT_EQ(ACTION_APPLIED, processed_actions1_capture[0].status());

  // The second script to call OnGetNextActions is the main script, with starts
  // with a wait_for_dom
  ASSERT_THAT(processed_actions2_capture, Not(IsEmpty()));
  EXPECT_EQ(ActionProto::ActionInfoCase::kWaitForDom,
            processed_actions2_capture[0].action().action_info_case());
  EXPECT_EQ(ACTION_APPLIED, processed_actions2_capture[0].status());
}

TEST_F(ScriptExecutorTest, RunMultipleInterruptInOrder) {
  // All elements exist. The two interrupts should run, in order, then the
  // element should be reported as found.
  SetupInterruptibleScript(kScriptPath, "element");
  SetupInterrupt("interrupt1", "interrupt_trigger1");
  SetupInterrupt("interrupt2", "interrupt_trigger2");

  {
    testing::InSequence seq;
    EXPECT_CALL(mock_service_,
                OnGetNextActions(_, "payload for interrupt1", _, _))
        .WillOnce(RunOnceCallback<3>(true, ""));
    EXPECT_CALL(mock_service_,
                OnGetNextActions(_, "payload for interrupt2", _, _))
        .WillOnce(RunOnceCallback<3>(true, ""));
    EXPECT_CALL(mock_service_, OnGetNextActions(_, "main script payload", _, _))
        .WillOnce(RunOnceCallback<3>(true, ""));
  }

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(executor_callback_.Get());

  EXPECT_THAT(scripts_state_,
              Contains(Pair(kScriptPath, SCRIPT_STATUS_SUCCESS)));
  EXPECT_THAT(scripts_state_,
              Contains(Pair("interrupt1", SCRIPT_STATUS_SUCCESS)));
  EXPECT_THAT(scripts_state_,
              Contains(Pair("interrupt2", SCRIPT_STATUS_SUCCESS)));
}

TEST_F(ScriptExecutorTest, ForwardMainScriptPayloadWhenInterruptRuns) {
  SetupInterruptibleScript(kScriptPath, "element");
  SetupInterrupt("interrupt", "interrupt_trigger");

  ActionsResponseProto next_interrupt_actions_response;
  next_interrupt_actions_response.set_global_payload(
      "last global payload from interrupt");
  next_interrupt_actions_response.set_script_payload(
      "last payload from interrupt");
  EXPECT_CALL(mock_service_, OnGetNextActions("global payload for interrupt",
                                              "payload for interrupt", _, _))
      .WillOnce(
          RunOnceCallback<3>(true, Serialize(next_interrupt_actions_response)));

  ActionsResponseProto next_main_actions_response;
  next_main_actions_response.set_global_payload(
      "last global payload from main");
  next_main_actions_response.set_script_payload("last payload from main");
  EXPECT_CALL(mock_service_,
              OnGetNextActions("last global payload from interrupt",
                               "main script payload", _, _))
      .WillOnce(
          RunOnceCallback<3>(true, Serialize(next_main_actions_response)));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(executor_callback_.Get());

  EXPECT_EQ("last global payload from main", last_global_payload_);
  EXPECT_EQ("last payload from main", last_script_payload_);
}

TEST_F(ScriptExecutorTest, ForwardMainScriptPayloadWhenInterruptFails) {
  SetupInterruptibleScript(kScriptPath, "element");
  SetupInterrupt("interrupt", "interrupt_trigger");

  EXPECT_CALL(mock_service_, OnGetNextActions("global payload for interrupt",
                                              "payload for interrupt", _, _))
      .WillOnce(RunOnceCallback<3>(false, ""));

  EXPECT_CALL(executor_callback_, Run(_));
  executor_->Run(executor_callback_.Get());

  EXPECT_EQ("global payload for interrupt", last_global_payload_);
  EXPECT_EQ("main script payload", last_script_payload_);
}

TEST_F(ScriptExecutorTest, DoNotRunInterruptIfPreconditionsDontMatch) {
  // interrupt_trigger does not exist, but element does, so wait_for_dom will
  // succeed without calling the interrupt.
  SetupInterruptibleScript(kScriptPath, "element");
  SetupInterrupt("interrupt", "interrupt_trigger");

  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(_, Eq(Selector({"element"})), _))
      .WillRepeatedly(RunOnceCallback<2>(true));
  EXPECT_CALL(mock_web_controller_,
              OnElementCheck(_, Eq(Selector({"interrupt_trigger"})), _))
      .WillRepeatedly(RunOnceCallback<2>(false));

  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _))
      .WillRepeatedly(RunOnceCallback<3>(true, ""));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(executor_callback_.Get());

  EXPECT_THAT(scripts_state_,
              Contains(Pair(kScriptPath, SCRIPT_STATUS_SUCCESS)));
  EXPECT_THAT(scripts_state_, Not(Contains(Pair(StrEq("interrupt"), _))));
}

TEST_F(ScriptExecutorTest, DoNotRunInterruptIfNotInterruptible) {
  // The main script has a wait_for_dom, but it is not interruptible.
  ActionsResponseProto interruptible;
  auto* wait_action = interruptible.add_actions()->mutable_wait_for_dom();
  wait_action->add_selectors("element");
  // allow_interrupt is not set
  EXPECT_CALL(mock_service_, OnGetActions(StrEq(kScriptPath), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(interruptible)));

  // The interrupt would trigger, since interrupt_trigger exits, but it's not
  // given an opportunity to.
  SetupInterrupt("interrupt", "interrupt_trigger");

  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _))
      .WillRepeatedly(RunOnceCallback<3>(true, ""));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(executor_callback_.Get());

  EXPECT_THAT(scripts_state_,
              Contains(Pair(kScriptPath, SCRIPT_STATUS_SUCCESS)));
  EXPECT_THAT(scripts_state_, Not(Contains(Pair(StrEq("interrupt"), _))));
}

TEST_F(ScriptExecutorTest, InterruptFailsMainScript) {
  // The interrupt is run and fails. Failure should cascade.
  SetupInterruptibleScript(kScriptPath, "element");
  SetupInterrupt("interrupt", "interrupt_trigger");

  EXPECT_CALL(mock_service_, OnGetNextActions(_, "payload for interrupt", _, _))
      .WillOnce(RunOnceCallback<3>(false, ""));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, false)));
  executor_->Run(executor_callback_.Get());

  EXPECT_THAT(scripts_state_,
              Contains(Pair(kScriptPath, SCRIPT_STATUS_FAILURE)));
  EXPECT_THAT(scripts_state_,
              Contains(Pair("interrupt", SCRIPT_STATUS_FAILURE)));
}

TEST_F(ScriptExecutorTest, InterruptReturnsShutdown) {
  // The interrupt succeeds, but executes the stop action. This should stop the
  // execution of the main script and make it return result.at_end=SHUTDOWN
  SetupInterruptibleScript(kScriptPath, "element");

  RegisterInterrupt("interrupt", "interrupt_trigger");
  ActionsResponseProto interrupt_actions;
  interrupt_actions.add_actions()->mutable_stop();

  EXPECT_CALL(mock_service_, OnGetActions(StrEq("interrupt"), _, _, _, _, _))
      .WillRepeatedly(RunOnceCallback<5>(true, Serialize(interrupt_actions)));

  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _))
      .WillOnce(RunOnceCallback<3>(true, ""));

  EXPECT_CALL(executor_callback_,
              Run(AllOf(Field(&ScriptExecutor::Result::success, true),
                        Field(&ScriptExecutor::Result::at_end,
                              ScriptExecutor::SHUTDOWN))));
  executor_->Run(executor_callback_.Get());

  EXPECT_THAT(scripts_state_,
              Contains(Pair(kScriptPath, SCRIPT_STATUS_SUCCESS)));
  EXPECT_THAT(scripts_state_,
              Contains(Pair("interrupt", SCRIPT_STATUS_SUCCESS)));
}

TEST_F(ScriptExecutorTest, UpdateScriptListGetNext) {
  should_update_scripts_ = false;
  scripts_update_.clear();
  scripts_update_count_ = 0;

  ActionsResponseProto initial_actions_response;
  initial_actions_response.add_actions()->mutable_tell()->set_message("1");
  EXPECT_CALL(mock_service_, OnGetActions(StrEq(kScriptPath), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(initial_actions_response)));

  ActionsResponseProto next_actions_response;
  next_actions_response.add_actions()->mutable_tell()->set_message("2");
  auto* script =
      next_actions_response.mutable_update_script_list()->add_scripts();
  script->set_path("path");
  auto* presentation = script->mutable_presentation();
  presentation->set_name("name");
  presentation->mutable_precondition();

  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _))
      .WillOnce(RunOnceCallback<3>(true, Serialize(next_actions_response)))
      .WillOnce(RunOnceCallback<3>(true, ""));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(executor_callback_.Get());

  EXPECT_TRUE(should_update_scripts_);
  EXPECT_THAT(scripts_update_, SizeIs(1));
  EXPECT_THAT(scripts_update_count_, Eq(1));
  EXPECT_THAT("path", scripts_update_[0]->handle.path);
  EXPECT_THAT("name", scripts_update_[0]->handle.name);
}

TEST_F(ScriptExecutorTest, UpdateScriptListShouldNotifyMultipleTimes) {
  should_update_scripts_ = false;
  scripts_update_.clear();
  scripts_update_count_ = 0;

  ActionsResponseProto actions_response;
  actions_response.add_actions()->mutable_tell()->set_message("hi");
  auto* script = actions_response.mutable_update_script_list()->add_scripts();
  script->set_path("path");
  auto* presentation = script->mutable_presentation();
  presentation->set_name("name");
  presentation->mutable_precondition();

  EXPECT_CALL(mock_service_, OnGetActions(StrEq(kScriptPath), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(actions_response)));

  script->set_path("path2");
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _))
      .WillOnce(RunOnceCallback<3>(true, Serialize(actions_response)))
      .WillOnce(RunOnceCallback<3>(true, ""));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(executor_callback_.Get());

  EXPECT_TRUE(should_update_scripts_);
  EXPECT_THAT(scripts_update_count_, Eq(2));
  EXPECT_THAT(scripts_update_, SizeIs(1));
  EXPECT_THAT("path2", scripts_update_[0]->handle.path);
}

TEST_F(ScriptExecutorTest, UpdateScriptListFromInterrupt) {
  should_update_scripts_ = false;
  scripts_update_.clear();
  scripts_update_count_ = 0;

  SetupInterruptibleScript(kScriptPath, "element");

  RegisterInterrupt("interrupt", "interrupt_trigger");
  ActionsResponseProto interrupt_actions;
  interrupt_actions.add_actions()->mutable_tell()->set_message("abc");

  EXPECT_CALL(mock_service_, OnGetActions(StrEq("interrupt"), _, _, _, _, _))
      .WillOnce(RunOnceCallback<5>(true, Serialize(interrupt_actions)));

  auto* script = interrupt_actions.mutable_update_script_list()->add_scripts();
  script->set_path("path");
  auto* presentation = script->mutable_presentation();
  presentation->set_name("update_from_interrupt");
  presentation->mutable_precondition();

  // We expect a call from the interrupt which will update the script list and a
  // second call from the interrupt to terminate. Then a call from the main
  // script which will finish without running any actions.
  EXPECT_CALL(mock_service_, OnGetNextActions(_, _, _, _))
      .Times(3)
      .WillOnce(RunOnceCallback<3>(true, Serialize(interrupt_actions)))
      .WillRepeatedly(RunOnceCallback<3>(true, ""));

  EXPECT_CALL(executor_callback_,
              Run(Field(&ScriptExecutor::Result::success, true)));
  executor_->Run(executor_callback_.Get());

  EXPECT_THAT(scripts_state_,
              Contains(Pair(kScriptPath, SCRIPT_STATUS_SUCCESS)));
  EXPECT_THAT(scripts_state_,
              Contains(Pair("interrupt", SCRIPT_STATUS_SUCCESS)));

  EXPECT_TRUE(should_update_scripts_);
  EXPECT_THAT(scripts_update_, SizeIs(1));
  EXPECT_THAT(scripts_update_count_, Eq(1));
  EXPECT_THAT("path", scripts_update_[0]->handle.path);
  EXPECT_THAT("update_from_interrupt", scripts_update_[0]->handle.name);
}

}  // namespace
}  // namespace autofill_assistant
