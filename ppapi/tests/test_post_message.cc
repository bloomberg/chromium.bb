// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_post_message.h"

#include <algorithm>

#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/cpp/dev/scriptable_object_deprecated.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/var.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

// Windows defines 'PostMessage', so we have to undef it.
#ifdef PostMessage
#undef PostMessage
#endif

REGISTER_TEST_CASE(PostMessage);

namespace {

const char kTestString[] = "Hello world!";
const bool kTestBool = true;
const int32_t kTestInt = 42;
const double kTestDouble = 42.0;
const int32_t kThreadsToRun = 10;

}  // namespace

bool TestPostMessage::Init() {
  return InitTestingInterface();
}

void TestPostMessage::RunTest() {
  RUN_TEST(SendingData);
  RUN_TEST(MessageEvent);
  RUN_TEST(NoHandler);
  RUN_TEST(ExtraParam);
}

void TestPostMessage::HandleMessage(const pp::Var& message_data) {
  message_data_.push_back(message_data);
  testing_interface_->QuitMessageLoop(instance_->pp_instance());
}

bool TestPostMessage::AddEchoingListener(const std::string& expression) {
  std::string js_code;
  // Note the following code is dependent on some features of test_case.html.
  // E.g., it is assumed that the DOM element where the plugin is embedded has
  // an id of 'plugin', and there is a function 'IsTestingMessage' that allows
  // us to ignore the messages that are intended for use by the testing
  // framework itself.
  js_code += "var plugin = document.getElementById('plugin');"
             "var message_handler = function(message_event) {"
             "  if (!IsTestingMessage(message_event.data)) {"
             "    plugin.postMessage(";
  js_code += expression;
  js_code += "                      );"
             "  }"
             "};"
             "plugin.addEventListener('message', message_handler);"
             // Maintain an array of all event listeners, attached to the
             // plugin. This is so that we can easily remove them later (see
             // ClearListeners()).
             "if (!plugin.eventListeners) plugin.eventListeners = [];"
             "plugin.eventListeners.push(message_handler);";
  pp::Var exception;
  instance_->ExecuteScript(js_code, &exception);
  return exception.is_undefined();
}

bool TestPostMessage::ClearListeners() {
  std::string js_code(
      "var plugin = document.getElementById('plugin');"
      "while (plugin.eventListeners.length) {"
      "  plugin.removeEventListener('message', plugin.eventListeners.pop());"
      "}");
  pp::Var exception;
  instance_->ExecuteScript(js_code, &exception);
  return(exception.is_undefined());
}

std::string TestPostMessage::TestSendingData() {
  // Set up the JavaScript message event listener to echo the data part of the
  // message event back to us.
  ASSERT_TRUE(AddEchoingListener("message_event.data"));

  // Test sending a message to JavaScript for each supported type.  The JS sends
  // the data back to us, and we check that they match.
  message_data_.clear();
  instance_->PostMessage(pp::Var(kTestString));
  // PostMessage is asynchronous, so we should not receive a response yet.
  ASSERT_EQ(message_data_.size(), 0);

  testing_interface_->RunMessageLoop(instance_->pp_instance());
  ASSERT_EQ(message_data_.size(), 1);
  ASSERT_TRUE(message_data_.back().is_string());
  ASSERT_EQ(message_data_.back().AsString(), kTestString);

  message_data_.clear();
  instance_->PostMessage(pp::Var(kTestBool));
  ASSERT_EQ(message_data_.size(), 0);
  testing_interface_->RunMessageLoop(instance_->pp_instance());
  ASSERT_EQ(message_data_.size(), 1);
  ASSERT_TRUE(message_data_.back().is_bool());
  ASSERT_EQ(message_data_.back().AsBool(), kTestBool);

  message_data_.clear();
  instance_->PostMessage(pp::Var(kTestInt));
  ASSERT_EQ(message_data_.size(), 0);
  testing_interface_->RunMessageLoop(instance_->pp_instance());
  ASSERT_EQ(message_data_.size(), 1);
  ASSERT_TRUE(message_data_.back().is_number());
  ASSERT_DOUBLE_EQ(message_data_.back().AsDouble(),
                   static_cast<double>(kTestInt));

  message_data_.clear();
  instance_->PostMessage(pp::Var(kTestDouble));
  ASSERT_EQ(message_data_.size(), 0);
  testing_interface_->RunMessageLoop(instance_->pp_instance());
  ASSERT_EQ(message_data_.size(), 1);
  ASSERT_TRUE(message_data_.back().is_number());
  ASSERT_DOUBLE_EQ(message_data_.back().AsDouble(), kTestDouble);

  message_data_.clear();
  instance_->PostMessage(pp::Var());
  ASSERT_EQ(message_data_.size(), 0);
  testing_interface_->RunMessageLoop(instance_->pp_instance());
  ASSERT_EQ(message_data_.size(), 1);
  ASSERT_TRUE(message_data_.back().is_undefined());

  message_data_.clear();
  instance_->PostMessage(pp::Var(pp::Var::Null()));
  ASSERT_EQ(message_data_.size(), 0);
  testing_interface_->RunMessageLoop(instance_->pp_instance());
  ASSERT_EQ(message_data_.size(), 1);
  ASSERT_TRUE(message_data_.back().is_null());

  ASSERT_TRUE(ClearListeners());

  PASS();
}

std::string TestPostMessage::TestMessageEvent() {
  // Set up the JavaScript message event listener to pass us some values from
  // the MessageEvent and make sure they match our expectations.

  // Have the listener pass back the type of message_event and make sure it's
  // "object".
  ASSERT_TRUE(AddEchoingListener("typeof(message_event)"));
  message_data_.clear();
  instance_->PostMessage(pp::Var(kTestInt));
  ASSERT_EQ(message_data_.size(), 0);
  testing_interface_->RunMessageLoop(instance_->pp_instance());
  ASSERT_EQ(message_data_.size(), 1);
  ASSERT_TRUE(message_data_.back().is_string());
  ASSERT_EQ(message_data_.back().AsString(), "object");
  ASSERT_TRUE(ClearListeners());

  // Make sure all the non-data properties have the expected values.
  bool success = AddEchoingListener("((message_event.origin == '')"
                                   " && (message_event.lastEventId == '')"
                                   " && (message_event.source == null)"
                                   " && (message_event.ports.length == 0)"
                                   " && (message_event.bubbles == false)"
                                   " && (message_event.cancelable == false)"
                                   ")");
  ASSERT_TRUE(success);
  message_data_.clear();
  instance_->PostMessage(pp::Var(kTestInt));
  ASSERT_EQ(message_data_.size(), 0);
  testing_interface_->RunMessageLoop(instance_->pp_instance());
  ASSERT_EQ(message_data_.size(), 1);
  ASSERT_TRUE(message_data_.back().is_bool());
  ASSERT_TRUE(message_data_.back().AsBool());
  ASSERT_TRUE(ClearListeners());

  // Add some event handlers to make sure they receive messages.
  ASSERT_TRUE(AddEchoingListener("1"));
  ASSERT_TRUE(AddEchoingListener("2"));
  ASSERT_TRUE(AddEchoingListener("3"));

  message_data_.clear();
  instance_->PostMessage(pp::Var(kTestInt));
  // Make sure we don't get a response in a re-entrant fashion.
  ASSERT_EQ(message_data_.size(), 0);
  // We should get 3 messages.
  testing_interface_->RunMessageLoop(instance_->pp_instance());
  testing_interface_->RunMessageLoop(instance_->pp_instance());
  testing_interface_->RunMessageLoop(instance_->pp_instance());
  ASSERT_EQ(message_data_.size(), 3);
  // Copy to a vector of doubles and sort; w3c does not specify the order for
  // event listeners. (Copying is easier than writing an operator< for pp::Var.)
  //
  // See http://www.w3.org/TR/2000/REC-DOM-Level-2-Events-20001113/events.html.
  VarVector::iterator iter(message_data_.begin()), the_end(message_data_.end());
  std::vector<double> double_vec;
  for (; iter != the_end; ++iter) {
    ASSERT_TRUE(iter->is_number());
    double_vec.push_back(iter->AsDouble());
  }
  std::sort(double_vec.begin(), double_vec.end());
  ASSERT_DOUBLE_EQ(double_vec[0], 1.0);
  ASSERT_DOUBLE_EQ(double_vec[1], 2.0);
  ASSERT_DOUBLE_EQ(double_vec[2], 3.0);

  ASSERT_TRUE(ClearListeners());

  PASS();
}

std::string TestPostMessage::TestNoHandler() {
  // Delete any lingering event listeners.
  ASSERT_TRUE(ClearListeners());

  // Now send a message.  We shouldn't get a response.
  message_data_.clear();
  instance_->PostMessage(pp::Var());
  // Note that at this point, if we call RunMessageLoop, we should hang, because
  // there should be no call to our HandleMessage function to quit the loop.
  // Therefore, we will do CallOnMainThread to yield control.  That event should
  // fire, but we should see no messages when we return.
  TestCompletionCallback callback(instance_->pp_instance());
  pp::Module::Get()->core()->CallOnMainThread(0, callback);
  callback.WaitForResult();
  ASSERT_TRUE(message_data_.empty());

  PASS();
}

std::string TestPostMessage::TestExtraParam() {
  // Delete any lingering event listeners.
  ASSERT_TRUE(ClearListeners());
  // Add a listener that will respond with 1 and an empty array (where the
  // message port array would appear if it was Worker postMessage).
  ASSERT_TRUE(AddEchoingListener("1, []"));

  // Now send a message.  We shouldn't get a response.
  message_data_.clear();
  instance_->PostMessage(pp::Var());
  // Note that at this point, if we call RunMessageLoop, we should hang, because
  // there should be no call to our HandleMessage function to quit the loop.
  // Therefore, we will do CallOnMainThread to yield control.  That event should
  // fire, but we should see no messages when we return.
  TestCompletionCallback callback(instance_->pp_instance());
  pp::Module::Get()->core()->CallOnMainThread(0, callback);
  callback.WaitForResult();
  ASSERT_TRUE(message_data_.empty());

  PASS();
}

