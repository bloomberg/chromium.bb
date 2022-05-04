// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/js_flow_executor.h"

#include <iosfwd>
#include <memory>
#include <string>
#include <type_traits>

#include "base/base64.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/json/json_reader.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "base/values.h"
#include "components/autofill_assistant/browser/base_browsertest.h"
#include "components/autofill_assistant/browser/client_status.h"
#include "components/autofill_assistant/browser/fake_script_executor_delegate.h"
#include "components/autofill_assistant/browser/fake_script_executor_ui_delegate.h"
#include "components/autofill_assistant/browser/js_flow_executor_impl.h"
#include "components/autofill_assistant/browser/mock_script_executor_delegate.h"
#include "components/autofill_assistant/browser/model.pb.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/script_executor.h"
#include "components/autofill_assistant/browser/service.pb.h"
#include "components/autofill_assistant/browser/service/mock_service.h"
#include "components/autofill_assistant/browser/trigger_context.h"
#include "components/autofill_assistant/browser/web/mock_web_controller.h"
#include "content/public/test/browser_test.h"
#include "content/shell/browser/shell.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill_assistant {
namespace {

using ::base::test::RunOnceCallback;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::Ne;
using ::testing::NiceMock;
using ::testing::Pair;
using ::testing::Pointee;
using ::testing::Property;
using ::testing::SizeIs;
using ::testing::StrictMock;
using ::testing::WithArg;

class MockJsFlowExecutorImplDelegate : public JsFlowExecutorImpl::Delegate {
 public:
  MockJsFlowExecutorImplDelegate() = default;
  ~MockJsFlowExecutorImplDelegate() override = default;

  MOCK_METHOD(
      void,
      RunNativeAction,
      (int,
       const std::string&,
       base::OnceCallback<void(const ClientStatus& result_status,
                               std::unique_ptr<base::Value> result_value)>
           callback),
      (override));
};

class JsFlowExecutorImplTest : public BaseBrowserTest {
 public:
  void SetUpOnMainThread() override {
    BaseBrowserTest::SetUpOnMainThread();

    flow_executor_ = std::make_unique<JsFlowExecutorImpl>(
        shell()->web_contents(), &mock_delegate_);
  }

  // Overload, ignore result value, just return the client status.
  ClientStatus RunTest(const std::string& js_flow) {
    std::unique_ptr<base::Value> ignored_result;
    return RunTest(js_flow, ignored_result);
  }

  ClientStatus RunTest(const std::string& js_flow,
                       std::unique_ptr<base::Value>& result_value) {
    ClientStatus status;
    base::RunLoop run_loop;
    flow_executor_->Start(
        js_flow, base::BindOnce(&JsFlowExecutorImplTest::OnFlowFinished,
                                base::Unretained(this), run_loop.QuitClosure(),
                                &status, std::ref(result_value)));
    run_loop.Run();
    return status;
  }

  void OnFlowFinished(base::OnceClosure done_callback,
                      ClientStatus* status_output,
                      std::unique_ptr<base::Value>& result_output,
                      const ClientStatus& status,
                      std::unique_ptr<base::Value> result_value) {
    *status_output = status;
    result_output = std::move(result_value);
    std::move(done_callback).Run();
  }

 protected:
  NiceMock<MockJsFlowExecutorImplDelegate> mock_delegate_;
  std::unique_ptr<JsFlowExecutorImpl> flow_executor_;
};

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest, SmokeTest) {
  EXPECT_THAT(RunTest(std::string()),
              Property(&ClientStatus::proto_status, ACTION_APPLIED));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest, InvalidJs) {
  EXPECT_THAT(RunTest("Not valid Javascript"),
              Property(&ClientStatus::proto_status, UNEXPECTED_JS_ERROR));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest, RunNativeActionWithReturnValue) {
  std::unique_ptr<base::Value> native_return_value =
      std::make_unique<base::Value>(std::move(*base::JSONReader::Read(
          R"(
          {
            "keyA":12345,
            "keyB":"Hello world",
            "keyC": ["array", "of", "strings"],
            "keyD": {
              "keyE": "nested",
              "keyF": true,
              "keyG": 123.45,
              "keyH": null
            },
            "keyI": [1,2,3,4]
          }
        )")));

  EXPECT_CALL(mock_delegate_, RunNativeAction)
      .WillOnce([&](int action_id, const std::string& action, auto callback) {
        EXPECT_EQ(12, action_id);
        EXPECT_EQ("test", action);
        std::move(callback).Run(ClientStatus(ACTION_APPLIED),
                                std::move(native_return_value));
      });

  std::unique_ptr<base::Value> js_return_value;
  EXPECT_THAT(RunTest(R"(
                        let [status, value] = await runNativeAction(
                            12, "dGVzdA==" /*test*/);
                        if (status != 2) { // ACTION_APPLIED
                          return status;
                        }

                        // Remove non-allowed types from return value.
                        delete value.keyB;
                        delete value.keyC;
                        delete value.keyD.keyE;

                        // Make some changes just to check that this propagates.
                        value.keyA += 3;
                        value.keyD.keyF = false;
                        value.keyI.push(5);
                        return value;
                      )",
                      js_return_value),
              Property(&ClientStatus::proto_status, ACTION_APPLIED));
  EXPECT_EQ(*js_return_value, *base::JSONReader::Read(R"(
    {
      "keyA": 12348,
      "keyD": {
        "keyF": false,
        "keyG": 123.45,
        "keyH": null
      },
      "keyI": [1,2,3,4,5]
    }
    )"));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest, RunNativeActionAsBase64String) {
  EXPECT_CALL(mock_delegate_, RunNativeAction)
      .WillOnce([&](int action_id, const std::string& action, auto callback) {
        EXPECT_EQ(12, action_id);
        EXPECT_EQ("test", action);
        std::move(callback).Run(ClientStatus(ACTION_APPLIED), nullptr);
      });

  std::unique_ptr<base::Value> result;
  EXPECT_THAT(RunTest(R"(
      let [status, value] = await runNativeAction(12, "dGVzdA==" /*test*/);
      return status;
  )",
                      result),
              Property(&ClientStatus::proto_status, ACTION_APPLIED));
  // ACTION_APPLIED == 2.
  EXPECT_EQ(*result, base::Value(2));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest,
                       RunNativeActionAsSerializedProto) {
  EXPECT_CALL(mock_delegate_, RunNativeAction)
      .WillOnce([&](int action_id, const std::string& action, auto callback) {
        EXPECT_EQ(ActionProto::kTell, action_id);
        TellProto tell;
        EXPECT_TRUE(tell.ParseFromString(action));
        EXPECT_EQ(tell.message(), "my message");
        std::move(callback).Run(ClientStatus(ACTION_APPLIED), nullptr);
      });

  std::unique_ptr<base::Value> result;
  EXPECT_THAT(RunTest(R"(
      let [status, value] = await runNativeAction(
          11, ["aa.msg", "my message"]);
      return status;
  )",
                      result),
              Property(&ClientStatus::proto_status, ACTION_APPLIED));
  EXPECT_EQ(*result, base::Value(2));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest, RunMultipleNativeActions) {
  EXPECT_CALL(mock_delegate_, RunNativeAction)
      .WillOnce([&](int action_id, const std::string& action, auto callback) {
        EXPECT_EQ(1, action_id);
        EXPECT_EQ("test1", action);
        std::move(callback).Run(ClientStatus(ACTION_APPLIED), nullptr);
      })
      .WillOnce([&](int action_id, const std::string& action, auto callback) {
        EXPECT_EQ(2, action_id);
        EXPECT_EQ("test2", action);
        std::move(callback).Run(ClientStatus(OTHER_ACTION_STATUS), nullptr);
      });

  // Note: the overall flow should report ACTION_APPLIED since the flow
  // completed successfully, but the return value should hold
  // OTHER_ACTION_STATUS, i.e., 3.
  std::unique_ptr<base::Value> result;
  EXPECT_THAT(RunTest(R"(
                        let [status, value] = await runNativeAction(
                            1, "dGVzdDE=" /*test1*/);
                        if (status == 2) { // ACTION_APPLIED
                          [status, value] = await runNativeAction(
                            2, "dGVzdDI=" /*test2*/);
                        }
                        return status;
                      )",
                      result),
              Property(&ClientStatus::proto_status, ACTION_APPLIED));
  // OTHER_ACTION_STATUS == 3
  EXPECT_EQ(*result, base::Value(3));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest, ReturnInteger) {
  std::unique_ptr<base::Value> result;
  ClientStatus status = RunTest("return 12345;", result);
  EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
  EXPECT_EQ(*result, base::Value(12345));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest, ReturningStringFails) {
  // Return value checking is more comprehensively tested in
  // js_flow_util::ContainsOnlyAllowedValues. This test is just to ensure that
  // that util is actually used for JS flow return values.
  std::unique_ptr<base::Value> result;
  ClientStatus status = RunTest("return 'Strings are not allowed!';", result);
  EXPECT_EQ(status.proto_status(), INVALID_ACTION);
  EXPECT_THAT(result, Eq(nullptr));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest, ReturnDictionary) {
  std::unique_ptr<base::Value> result;
  ClientStatus status = RunTest(
      R"(
          return {
            "keyA":12345,
            "keyB": {
              "keyC": true,
              "keyD": 123.45,
              "keyE": null
            },
            "keyF": [false, false, true]
          };
        )",
      result);
  EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
  EXPECT_EQ(*result, *base::JSONReader::Read(R"(
      {
        "keyA":12345,
          "keyB": {
            "keyC": true,
            "keyD": 123.45,
            "keyE": null
          },
        "keyF": [false, false, true]
      }
    )"));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest, ReturnNothing) {
  std::unique_ptr<base::Value> result;
  ClientStatus status = RunTest("", result);
  EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
  EXPECT_THAT(result, Eq(nullptr));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest, ReturnNonJsonObjectFails) {
  std::unique_ptr<base::Value> result;
  ClientStatus status = RunTest(R"(
    function test() {
      console.log('something');
    }
    return test;
  )",
                                result);
  EXPECT_EQ(status.proto_status(), INVALID_ACTION);
  EXPECT_THAT(result, Eq(nullptr));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest, ReturnNull) {
  std::unique_ptr<base::Value> result;
  ClientStatus status = RunTest("return null;", result);
  EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
  EXPECT_EQ(*result, *base::JSONReader::Read("null"));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest, ExceptionReporting) {
  std::unique_ptr<base::Value> result;
  ClientStatus status = RunTest("notdefined;", result);
  EXPECT_EQ(status.proto_status(), UNEXPECTED_JS_ERROR);
  ASSERT_THAT(result, Eq(nullptr));

  // NOTE: Do not change the values here. The above script should output exactly
  // one stack frame with 0:0.
  EXPECT_THAT(
      status.details().unexpected_error_info().js_exception_line_numbers(),
      ElementsAre(0));
  EXPECT_THAT(
      status.details().unexpected_error_info().js_exception_column_numbers(),
      ElementsAre(0));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest, RunMultipleConsecutiveFlows) {
  for (int i = 0; i < 10; ++i) {
    std::unique_ptr<base::Value> result;
    ClientStatus status =
        RunTest(base::StrCat({"return ", base::NumberToString(i)}), result);
    EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
    EXPECT_EQ(*result, base::Value(i));
  }
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest,
                       UnserializableRunNativeActionString) {
  std::unique_ptr<base::Value> result;
  EXPECT_CALL(mock_delegate_, RunNativeAction).Times(0);
  ClientStatus status = RunTest(
      R"(
        // {} is not a string or an array, so this should fail.
        let [status, result] = await runNativeAction(1, {});
        return status;
      )",
      result);
  EXPECT_EQ(result, nullptr);
  EXPECT_EQ(status.proto_status(), INVALID_ACTION);
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest,
                       UnserializableRunNativeActionId) {
  std::unique_ptr<base::Value> result;
  EXPECT_CALL(mock_delegate_, RunNativeAction).Times(0);
  ClientStatus status = RunTest(
      R"(
        // {} is not a number, so this should fail.
        let [status, result] = await runNativeAction({}, "");
        return status;
      )",
      result);
  EXPECT_EQ(result, nullptr);
  EXPECT_EQ(status.proto_status(), INVALID_ACTION);
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest, StartWhileAlreadyRunningFails) {
  EXPECT_CALL(mock_delegate_, RunNativeAction)
      .WillOnce(WithArg<2>([&](auto callback) {
        // Starting a second flow while the first one is running should fail.
        EXPECT_EQ(RunTest(std::string()).proto_status(), INVALID_ACTION);

        // The first flow should be able to finish successfully.
        std::move(callback).Run(ClientStatus(ACTION_APPLIED), nullptr);
      }));

  std::unique_ptr<base::Value> result;
  ClientStatus status = RunTest(
      R"(
      let [status, result] = await runNativeAction(1, "dGVzdA==" /*test*/);
      return status;
      )",
      result);
  EXPECT_EQ(status.proto_status(), ACTION_APPLIED);
  EXPECT_EQ(*result, base::Value(2));
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplTest,
                       EnvironmentIsPreservedBetweenRuns) {
  EXPECT_EQ(RunTest("globalFlowState.i = 5;").proto_status(), ACTION_APPLIED);

  std::unique_ptr<base::Value> result;
  EXPECT_EQ(RunTest("return globalFlowState.i;", result).proto_status(),
            ACTION_APPLIED);
  EXPECT_EQ(*result, base::Value(5));
}

class JsFlowExecutorImplScriptExecutorTest : public BaseBrowserTest {
 public:
  void SetUpOnMainThread() override {
    BaseBrowserTest::SetUpOnMainThread();

    web_controller_ = WebController::CreateForWebContents(
        shell()->web_contents(), &user_data_, &log_info_, nullptr,
        /*enable_full_stack_traces= */ true);

    fake_script_executor_delegate_.SetService(&mock_service_);
    fake_script_executor_delegate_.SetWebController(web_controller_.get());
    fake_script_executor_delegate_.SetCurrentURL(GURL("http://example.com/"));
    fake_script_executor_delegate_.SetWebContents(shell()->web_contents());

    script_executor_ = std::make_unique<ScriptExecutor>(
        /* script_path= */ "",
        /* additional_context= */ std::make_unique<TriggerContext>(),
        /* global_payload= */ "",
        /* script_payload= */ "",
        /* listener= */ nullptr, &ordered_interrupts_,
        &fake_script_executor_delegate_, &fake_script_executor_ui_delegate_);
  }

 protected:
  void Run(const std::string& js_flow,
           const ProcessedActionStatusProto& result) {
    ActionsResponseProto actions_response;
    actions_response.add_actions()->mutable_js_flow()->set_js_flow(js_flow);
    /* actions_response.add_actions() */
    /*     ->mutable_release_elements() */
    /*     ->add_client_ids() */
    /*     ->set_identifier("client_id"); */

    EXPECT_CALL(mock_service_, GetActions)
        .WillOnce(RunOnceCallback<5>(net::HTTP_OK,
                                     actions_response.SerializeAsString(),
                                     ServiceRequestSender::ResponseInfo{}));

    EXPECT_CALL(mock_service_,
                GetNextActions(_, _, _,
                               ElementsAre(Property(
                                   &ProcessedActionProto::status, result)),
                               _, _, _))
        .WillOnce(RunOnceCallback<6>(net::HTTP_OK,
                                     ActionsResponseProto().SerializeAsString(),
                                     ServiceRequestSender::ResponseInfo{}));

    base::RunLoop run_loop;
    script_executor_->Run(
        &user_data_,
        base::BindOnce(&JsFlowExecutorImplScriptExecutorTest::OnFlowFinished,
                       base::Unretained(this), run_loop.QuitClosure()));
    run_loop.Run();
  }

  void OnFlowFinished(base::OnceClosure done_callback,
                      const ScriptExecutor::Result& result) {
    EXPECT_TRUE(result.success);
    std::move(done_callback).Run();
  }

  std::vector<std::unique_ptr<Script>> ordered_interrupts_;

  ProcessedActionStatusDetailsProto log_info_;
  std::unique_ptr<WebController> web_controller_;

  FakeScriptExecutorDelegate fake_script_executor_delegate_;
  FakeScriptExecutorUiDelegate fake_script_executor_ui_delegate_;
  UserData user_data_;

  NiceMock<MockService> mock_service_;
  std::unique_ptr<ScriptExecutor> script_executor_;
};

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplScriptExecutorTest,
                       WaitForDomSucceeds) {
  WaitForDomProto wait_for_dom;
  wait_for_dom.mutable_wait_condition()
      ->mutable_match()
      ->add_filters()
      ->set_css_selector("#button");
  std::string wait_for_dom_base64;
  base::Base64Encode(wait_for_dom.SerializeAsString(), &wait_for_dom_base64);

  Run(R"(const [status, value] = await runNativeAction(19, ')" +
          wait_for_dom_base64 + R"(');
      return {status};)",
      ACTION_APPLIED);
}

IN_PROC_BROWSER_TEST_F(JsFlowExecutorImplScriptExecutorTest, WaitForDomFails) {
  WaitForDomProto wait_for_dom;
  wait_for_dom.mutable_wait_condition()
      ->mutable_match()
      ->add_filters()
      ->set_css_selector("#not-found");
  std::string wait_for_dom_base64;
  base::Base64Encode(wait_for_dom.SerializeAsString(), &wait_for_dom_base64);

  Run(R"(const [status, value] = await runNativeAction(19, ')" +
          wait_for_dom_base64 + R"(');
      return {status};)",
      ELEMENT_RESOLUTION_FAILED);
}

}  // namespace
}  // namespace autofill_assistant
