// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_post_message.h"

#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/cpp/dev/scriptable_object_deprecated.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/var.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(PostMessage);

namespace {

const PPB_Testing_Dev* GetTestingInterface() {
  static const PPB_Testing_Dev* g_testing_interface =
      reinterpret_cast<PPB_Testing_Dev const*>(
          pp::Module::Get()->GetBrowserInterface(PPB_TESTING_DEV_INTERFACE));
  return g_testing_interface;
}

const char kTestString[] = "Hello world!";
const bool kTestBool = true;
const int32_t kTestInt = 42;
const double kTestDouble = 42.0;
const int32_t kThreadsToRun = 10;

}  // namespace

bool TestPostMessage::Init() {
  testing_interface_ = reinterpret_cast<const PPB_Testing_Dev*>(
      pp::Module::Get()->GetBrowserInterface(PPB_TESTING_DEV_INTERFACE));
  if (!testing_interface_) {
    // Give a more helpful error message for the testing interface being gone
    // since that needs special enabling in Chrome.
    instance_->AppendError("This test needs the testing interface, which is "
        "not currently available. In Chrome, use --enable-pepper-testing when "
        "launching.");
  }
  return (testing_interface_ != NULL);
}

void TestPostMessage::RunTest() {
  RUN_TEST(SendingData);
  RUN_TEST(MessageEvent);
  RUN_TEST(NoHandler);
}

void TestPostMessage::HandleMessage(const pp::Var& message_data) {
  message_data_.push_back(message_data);
}

bool TestPostMessage::MakeOnMessageEcho(const std::string& expression) {
  std::string js_code(
      "document.getElementById('plugin').onmessage = function(message_event) {"
      "    document.getElementById('plugin').postMessage(");
  js_code += expression;
  js_code += ");}";
  pp::Var exception;
  // TODO(dmichael):  Move ExecuteScript to the testing interface.
  instance_->ExecuteScript(js_code, &exception);
  return(exception.is_undefined());
}

std::string TestPostMessage::TestSendingData() {
  // Set up the JavaScript onmessage handler to echo the data part of the
  // message event back to us.
  ASSERT_TRUE(MakeOnMessageEcho("message_event.data"));

  // Test sending a message to JavaScript for each supported type.  The JS sends
  // the data back to us, and we check that they match.
  message_data_.clear();
  instance_->PostMessage(pp::Var(kTestString));
  // Note that the trusted in-process version is completely synchronous, so we
  // do not need to use 'RunMessageLoop' to wait.
  ASSERT_EQ(message_data_.size(), 1);
  ASSERT_TRUE(message_data_.back().is_string());
  ASSERT_EQ(message_data_.back().AsString(), kTestString);

  message_data_.clear();
  instance_->PostMessage(pp::Var(kTestBool));
  ASSERT_EQ(message_data_.size(), 1);
  ASSERT_TRUE(message_data_.back().is_bool());
  ASSERT_EQ(message_data_.back().AsBool(), kTestBool);

  message_data_.clear();
  instance_->PostMessage(pp::Var(kTestInt));
  ASSERT_EQ(message_data_.size(), 1);
  ASSERT_TRUE(message_data_.back().is_number());
  ASSERT_DOUBLE_EQ(message_data_.back().AsDouble(),
                   static_cast<double>(kTestInt));

  message_data_.clear();
  instance_->PostMessage(pp::Var(kTestDouble));
  ASSERT_EQ(message_data_.size(), 1);
  ASSERT_TRUE(message_data_.back().is_number());
  ASSERT_DOUBLE_EQ(message_data_.back().AsDouble(), kTestDouble);

  message_data_.clear();
  instance_->PostMessage(pp::Var());
  ASSERT_EQ(message_data_.size(), 1);
  ASSERT_TRUE(message_data_.back().is_undefined());

  message_data_.clear();
  instance_->PostMessage(pp::Var(pp::Var::Null()));
  ASSERT_EQ(message_data_.size(), 1);
  ASSERT_TRUE(message_data_.back().is_null());

  PASS();
}

std::string TestPostMessage::TestMessageEvent() {
  // Set up the JavaScript onmessage handler to pass us some values from the
  // MessageEvent and make sure they match our expectations.

  // Have onmessage pass back the type of message_event and make sure it's
  // "object".
  ASSERT_TRUE(MakeOnMessageEcho("typeof(message_event)"));
  message_data_.clear();
  instance_->PostMessage(pp::Var(kTestInt));
  ASSERT_EQ(message_data_.size(), 1);
  ASSERT_TRUE(message_data_.back().is_string());
  ASSERT_EQ(message_data_.back().AsString(), "object");

  // Make sure all the non-data properties have the expected values.
  bool success = MakeOnMessageEcho("((message_event.origin == '')"
                                   " && (message_event.lastEventId == '')"
                                   " && (message_event.source == null)"
                                   " && (message_event.ports == null)"
                                   " && (message_event.bubbles == false)"
                                   " && (message_event.cancelable == false)"
                                   ")");
  ASSERT_TRUE(success);
  message_data_.clear();
  instance_->PostMessage(pp::Var(kTestInt));
  // Note that the trusted in-process version is completely synchronous, so we
  // do not need to use 'RunMessageLoop' to wait.
  ASSERT_EQ(message_data_.size(), 1);
  ASSERT_TRUE(message_data_.back().is_bool());
  ASSERT_TRUE(message_data_.back().AsBool());

  PASS();
}

std::string TestPostMessage::TestNoHandler() {
  // Delete the onmessage handler (if it exists)
  std::string js_code(
      "if (document.getElementById('plugin').onmessage) {"
      "  delete document.getElementById('plugin').onmessage;"
      "}");
  pp::Var exception;
  instance_->ExecuteScript(js_code, &exception);
  ASSERT_TRUE(exception.is_undefined());

  // Now send a message and make sure we don't get anything back (and that we
  // don't crash).
  message_data_.clear();
  instance_->PostMessage(pp::Var());
  ASSERT_EQ(message_data_.size(), 0);

  PASS();
}

