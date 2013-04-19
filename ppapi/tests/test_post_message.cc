// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_post_message.h"

#include <algorithm>
#include <sstream>

#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_array_buffer.h"
#include "ppapi/tests/pp_thread.h"
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

// On Windows XP bots, the NonMainThread test can run very slowly. So we dial
// back the number of threads & messages when running on Windows.
#ifdef PPAPI_OS_WIN
const int32_t kThreadsToRun = 2;
const int32_t kMessagesToSendPerThread = 5;
#else
const int32_t kThreadsToRun = 4;
const int32_t kMessagesToSendPerThread = 10;
#endif

// The struct that invoke_post_message_thread_func expects for its argument.
// It includes the instance on which to invoke PostMessage, and the value to
// pass to PostMessage.
struct InvokePostMessageThreadArg {
  InvokePostMessageThreadArg(pp::Instance* i, const pp::Var& v)
      : instance(i), value_to_send(v) {}
  pp::Instance* instance;
  pp::Var value_to_send;
};

void InvokePostMessageThreadFunc(void* user_data) {
  InvokePostMessageThreadArg* arg =
      static_cast<InvokePostMessageThreadArg*>(user_data);
  for (int32_t i = 0; i < kMessagesToSendPerThread; ++i)
    arg->instance->PostMessage(arg->value_to_send);
  delete arg;
}

class ScopedArrayBufferSizeSetter {
 public:
  ScopedArrayBufferSizeSetter(const PPB_Testing_Dev* interface,
                              PP_Instance instance,
                              uint32_t threshold)
     : interface_(interface),
       instance_(instance) {
    interface_->SetMinimumArrayBufferSizeForShmem(instance_, threshold);
  }
  ~ScopedArrayBufferSizeSetter() {
    interface_->SetMinimumArrayBufferSizeForShmem(instance_, 0);
  }
 private:
  const PPB_Testing_Dev* interface_;
  PP_Instance instance_;
};

#define FINISHED_WAITING_MESSAGE "TEST_POST_MESSAGE_FINISHED_WAITING"

}  // namespace

TestPostMessage::TestPostMessage(TestingInstance* instance)
    : TestCase(instance) {
}

TestPostMessage::~TestPostMessage() {
  // Remove the special listener that only responds to a FINISHED_WAITING
  // string. See Init for where it gets added.
  std::string js_code;
  js_code += "var plugin = document.getElementById('plugin');"
             "plugin.removeEventListener('message',"
             "                           plugin.wait_for_messages_handler);"
             "delete plugin.wait_for_messages_handler;";
  instance_->EvalScript(js_code);
}

bool TestPostMessage::Init() {
  bool success = CheckTestingInterface();

  // Set up a special listener that only responds to a FINISHED_WAITING string.
  // This is for use by WaitForMessages.
  std::string js_code;
  // Note the following code is dependent on some features of test_case.html.
  // E.g., it is assumed that the DOM element where the plugin is embedded has
  // an id of 'plugin', and there is a function 'IsTestingMessage' that allows
  // us to ignore the messages that are intended for use by the testing
  // framework itself.
  js_code += "var plugin = document.getElementById('plugin');"
             "var wait_for_messages_handler = function(message_event) {"
             "  if (!IsTestingMessage(message_event.data) &&"
             "      message_event.data === '" FINISHED_WAITING_MESSAGE "') {"
             "    plugin.postMessage('" FINISHED_WAITING_MESSAGE "');"
             "  }"
             "};"
             "plugin.addEventListener('message', wait_for_messages_handler);"
             // Stash it on the plugin so we can remove it in the destructor.
             "plugin.wait_for_messages_handler = wait_for_messages_handler;";
  instance_->EvalScript(js_code);

  // Set up the JavaScript message event listener to echo the data part of the
  // message event back to us.
  success = success && AddEchoingListener("message_event.data");
  message_data_.clear();
  // Send a message that the first test will expect to receive. This is to
  // verify that we can send messages when the 'Instance::Init' function is on
  // the stack.
  instance_->PostMessage(pp::Var(kTestString));

  return success;
}

void TestPostMessage::RunTests(const std::string& filter) {
  // Note: SendInInit must be first, because it expects to receive a message
  // that was sent in Init above.
  RUN_TEST(SendInInit, filter);
  RUN_TEST(SendingData, filter);
  RUN_TEST(SendingArrayBuffer, filter);
  RUN_TEST(MessageEvent, filter);
  RUN_TEST(NoHandler, filter);
  RUN_TEST(ExtraParam, filter);
  if (testing_interface_->IsOutOfProcess())
    RUN_TEST(NonMainThread, filter);
}

void TestPostMessage::HandleMessage(const pp::Var& message_data) {
  if (message_data.is_string() &&
      (message_data.AsString() == FINISHED_WAITING_MESSAGE))
    testing_interface_->QuitMessageLoop(instance_->pp_instance());
  else
    message_data_.push_back(message_data);
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
             "  if (!IsTestingMessage(message_event.data) &&"
             "      !(message_event.data === '" FINISHED_WAITING_MESSAGE "')) {"
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
  instance_->EvalScript(js_code);
  return true;
}

bool TestPostMessage::ClearListeners() {
  std::string js_code;
  js_code += "var plugin = document.getElementById('plugin');"
             "while (plugin.eventListeners.length) {"
             "  plugin.removeEventListener('message',"
             "                             plugin.eventListeners.pop());"
             "}";
  instance_->EvalScript(js_code);
  return true;
}

int TestPostMessage::WaitForMessages() {
  size_t message_size_before = message_data_.size();
  // We first post a FINISHED_WAITING_MESSAGE. This should be guaranteed to
  // come back _after_ any other incoming messages that were already pending.
  instance_->PostMessage(pp::Var(FINISHED_WAITING_MESSAGE));
  testing_interface_->RunMessageLoop(instance_->pp_instance());
  // Now that the FINISHED_WAITING_MESSAGE has been echoed back to us, we know
  // that all pending messages have been slurped up. Return the number we
  // received (which may be zero).
  return message_data_.size() - message_size_before;
}

std::string TestPostMessage::TestSendInInit() {
  ASSERT_EQ(WaitForMessages(), 1);
  // This test assumes Init already sent a message.
  ASSERT_EQ(message_data_.size(), 1);
  ASSERT_TRUE(message_data_.back().is_string());
  ASSERT_EQ(message_data_.back().AsString(), kTestString);
  message_data_.clear();
  PASS();
}

std::string TestPostMessage::TestSendingData() {
  // Clean up after previous tests. This also swallows the message sent by Init
  // if we didn't run the 'SendInInit' test. All tests other than 'SendInInit'
  // should start with these.
  WaitForMessages();
  ASSERT_TRUE(ClearListeners());
  // Set up the JavaScript message event listener to echo the data part of the
  // message event back to us.
  ASSERT_TRUE(AddEchoingListener("message_event.data"));

  // Test sending a message to JavaScript for each supported type.  The JS sends
  // the data back to us, and we check that they match.
  message_data_.clear();
  instance_->PostMessage(pp::Var(kTestString));
  // PostMessage is asynchronous, so we should not receive a response yet.
  ASSERT_EQ(message_data_.size(), 0);
  ASSERT_EQ(WaitForMessages(), 1);
  ASSERT_TRUE(message_data_.back().is_string());
  ASSERT_EQ(message_data_.back().AsString(), kTestString);

  message_data_.clear();
  instance_->PostMessage(pp::Var(kTestBool));
  ASSERT_EQ(message_data_.size(), 0);
  ASSERT_EQ(WaitForMessages(), 1);
  ASSERT_TRUE(message_data_.back().is_bool());
  ASSERT_EQ(message_data_.back().AsBool(), kTestBool);

  message_data_.clear();
  instance_->PostMessage(pp::Var(kTestInt));
  ASSERT_EQ(message_data_.size(), 0);
  ASSERT_EQ(WaitForMessages(), 1);
  ASSERT_TRUE(message_data_.back().is_number());
  ASSERT_DOUBLE_EQ(message_data_.back().AsDouble(),
                   static_cast<double>(kTestInt));

  message_data_.clear();
  instance_->PostMessage(pp::Var(kTestDouble));
  ASSERT_EQ(message_data_.size(), 0);
  ASSERT_EQ(WaitForMessages(), 1);
  ASSERT_TRUE(message_data_.back().is_number());
  ASSERT_DOUBLE_EQ(message_data_.back().AsDouble(), kTestDouble);

  message_data_.clear();
  instance_->PostMessage(pp::Var());
  ASSERT_EQ(message_data_.size(), 0);
  ASSERT_EQ(WaitForMessages(), 1);
  ASSERT_TRUE(message_data_.back().is_undefined());

  message_data_.clear();
  instance_->PostMessage(pp::Var(pp::Var::Null()));
  ASSERT_EQ(message_data_.size(), 0);
  ASSERT_EQ(WaitForMessages(), 1);
  ASSERT_TRUE(message_data_.back().is_null());

  message_data_.clear();
  ASSERT_TRUE(ClearListeners());

  PASS();
}

std::string TestPostMessage::TestSendingArrayBuffer() {
  // Clean up after previous tests. This also swallows the message sent by Init
  // if we didn't run the 'SendInInit' test. All tests other than 'SendInInit'
  // should start with these.
  WaitForMessages();
  ASSERT_TRUE(ClearListeners());

  // TODO(sehr,dmichael): Add testing of longer array buffers when
  // crbug.com/110086 is fixed.
  ScopedArrayBufferSizeSetter setter(testing_interface_,
                                     instance_->pp_instance(),
                                     200);
  uint32_t sizes[] = { 0, 100, 1000, 10000 };
  for (size_t i = 0; i < sizeof(sizes)/sizeof(sizes[i]); ++i) {
    std::ostringstream size_stream;
    size_stream << sizes[i];
    const std::string kSizeAsString(size_stream.str());

    // Create an appropriately sized array buffer with test_data[i] == i.
    pp::VarArrayBuffer test_data(sizes[i]);
    if (sizes[i] > 0)
      ASSERT_NE(NULL, test_data.Map());
    // Make sure we can Unmap/Map successfully (there's not really any way to
    // detect if it's unmapped, so we just re-map before getting the pointer to
    // the buffer).
    test_data.Unmap();
    test_data.Map();
    ASSERT_EQ(sizes[i], test_data.ByteLength());
    unsigned char* buff = static_cast<unsigned char*>(test_data.Map());
    const uint32_t kByteLength = test_data.ByteLength();
    for (size_t j = 0; j < kByteLength; ++j)
      buff[j] = static_cast<uint8_t>(j % 256u);

    // Have the listener test some properties of the ArrayBuffer.
    std::vector<std::string> properties_to_check;
    properties_to_check.push_back(
        "message_event.data.constructor.name === 'ArrayBuffer'");
    properties_to_check.push_back(
        std::string("message_event.data.byteLength === ") + kSizeAsString);
    if (sizes[i] > 0) {
      properties_to_check.push_back(
          "(new DataView(message_event.data)).getUint8(0) == 0");
      // Checks that the last element has the right value: (byteLength-1)%256.
      std::string received_byte("(new DataView(message_event.data)).getUint8("
                                "    message_event.data.byteLength-1)");
      std::string expected_byte("(message_event.data.byteLength-1)%256");
      properties_to_check.push_back(received_byte + " == " + expected_byte);
    }
    for (std::vector<std::string>::iterator iter = properties_to_check.begin();
         iter != properties_to_check.end();
         ++iter) {
      ASSERT_TRUE(AddEchoingListener(*iter));
      message_data_.clear();
      instance_->PostMessage(test_data);
      ASSERT_EQ(message_data_.size(), 0);
      ASSERT_EQ(WaitForMessages(), 1);
      ASSERT_TRUE(message_data_.back().is_bool());
      if (!message_data_.back().AsBool())
        return std::string("Failed: ") + *iter + ", size: " + kSizeAsString;
      ASSERT_TRUE(message_data_.back().AsBool());
      ASSERT_TRUE(ClearListeners());
    }

    // Set up the JavaScript message event listener to echo the data part of the
    // message event back to us.
    ASSERT_TRUE(AddEchoingListener("message_event.data"));
    message_data_.clear();
    instance_->PostMessage(test_data);
    // PostMessage is asynchronous, so we should not receive a response yet.
    ASSERT_EQ(message_data_.size(), 0);
    ASSERT_EQ(WaitForMessages(), 1);
    ASSERT_TRUE(message_data_.back().is_array_buffer());
    pp::VarArrayBuffer received(message_data_.back());
    message_data_.clear();
    ASSERT_EQ(test_data.ByteLength(), received.ByteLength());
    unsigned char* received_buff = static_cast<unsigned char*>(received.Map());
    // The buffer should be copied, so this should be a distinct buffer. When
    // 'transferrables' are implemented for PPAPI, we'll also want to test that
    // we get the _same_ buffer back when it's transferred.
    if (sizes[i] > 0)
      ASSERT_NE(buff, received_buff);
    for (size_t i = 0; i < test_data.ByteLength(); ++i)
      ASSERT_EQ(buff[i], received_buff[i]);

    message_data_.clear();
    ASSERT_TRUE(ClearListeners());
  }

  PASS();
}

std::string TestPostMessage::TestMessageEvent() {
  // Set up the JavaScript message event listener to pass us some values from
  // the MessageEvent and make sure they match our expectations.

  WaitForMessages();
  ASSERT_TRUE(ClearListeners());
  // Have the listener pass back the class name of message_event and make sure
  // it's "MessageEvent".
  ASSERT_TRUE(AddEchoingListener("message_event.constructor.name"));
  message_data_.clear();
  instance_->PostMessage(pp::Var(kTestInt));
  ASSERT_EQ(message_data_.size(), 0);
  ASSERT_EQ(WaitForMessages(), 1);
  ASSERT_TRUE(message_data_.back().is_string());
  ASSERT_EQ(message_data_.back().AsString(), "MessageEvent");
  ASSERT_TRUE(ClearListeners());

  // Make sure all the non-data properties have the expected values.
  bool success = AddEchoingListener("((message_event.origin === '')"
                                   " && (message_event.lastEventId === '')"
                                   " && (message_event.source === null)"
                                   " && (message_event.ports.length === 0)"
                                   " && (message_event.bubbles === false)"
                                   " && (message_event.cancelable === false)"
                                   ")");
  ASSERT_TRUE(success);
  message_data_.clear();
  instance_->PostMessage(pp::Var(kTestInt));
  ASSERT_EQ(message_data_.size(), 0);
  ASSERT_EQ(WaitForMessages(), 1);
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
  ASSERT_EQ(WaitForMessages(), 3);
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

  message_data_.clear();
  ASSERT_TRUE(ClearListeners());

  PASS();
}

std::string TestPostMessage::TestNoHandler() {
  // Delete any lingering messages and event listeners.
  WaitForMessages();
  ASSERT_TRUE(ClearListeners());

  // Now send a message.  We shouldn't get a response.
  message_data_.clear();
  instance_->PostMessage(pp::Var());
  ASSERT_EQ(WaitForMessages(), 0);
  ASSERT_TRUE(message_data_.empty());

  PASS();
}

std::string TestPostMessage::TestExtraParam() {
  // Delete any lingering messages and event listeners.
  WaitForMessages();
  ASSERT_TRUE(ClearListeners());
  // Add a listener that will respond with 1 and an empty array (where the
  // message port array would appear if it was Worker postMessage).
  ASSERT_TRUE(AddEchoingListener("1, []"));

  // Now send a message.  We shouldn't get a response.
  message_data_.clear();
  instance_->PostMessage(pp::Var());
  ASSERT_EQ(WaitForMessages(), 0);
  ASSERT_TRUE(message_data_.empty());

  ASSERT_TRUE(ClearListeners());

  PASS();
}

std::string TestPostMessage::TestNonMainThread() {
  WaitForMessages();
  ASSERT_TRUE(ClearListeners());
  ASSERT_TRUE(AddEchoingListener("message_event.data"));
  message_data_.clear();

  // Set up a thread for each integer from 0 to (kThreadsToRun - 1).  Make each
  // thread send the number that matches its index kMessagesToSendPerThread
  // times.  For good measure, call postMessage from the main thread
  // kMessagesToSendPerThread times. At the end, we make sure we got all the
  // values we expected.
  PP_ThreadType threads[kThreadsToRun];
  for (int32_t i = 0; i < kThreadsToRun; ++i) {
    // Set up a thread to send a value of i.
    void* arg = new InvokePostMessageThreadArg(instance_, pp::Var(i));
    PP_CreateThread(&threads[i], &InvokePostMessageThreadFunc, arg);
  }
  // Invoke PostMessage right now to send a value of (kThreadsToRun).
  for (int32_t i = 0; i < kMessagesToSendPerThread; ++i)
    instance_->PostMessage(pp::Var(kThreadsToRun));

  // Now join all threads.
  for (int32_t i = 0; i < kThreadsToRun; ++i)
    PP_JoinThread(threads[i]);

  // PostMessage is asynchronous, so we should not receive a response yet.
  ASSERT_EQ(message_data_.size(), 0);

  // Make sure we got all values that we expected.  Note that because it's legal
  // for the JavaScript engine to treat our integers as floating points, we
  // can't just use std::find or equality comparison. So we instead, we convert
  // each incoming value to an integer, and count them in received_counts.
  int32_t expected_num = (kThreadsToRun + 1) * kMessagesToSendPerThread;
  // Count how many we receive per-index.
  std::vector<int32_t> expected_counts(kThreadsToRun + 1,
                                       kMessagesToSendPerThread);
  std::vector<int32_t> received_counts(kThreadsToRun + 1, 0);
  ASSERT_EQ(WaitForMessages(), expected_num);
  for (int32_t i = 0; i < expected_num; ++i) {
    const pp::Var& latest_var(message_data_[i]);
    ASSERT_TRUE(latest_var.is_int() || latest_var.is_double());
    int32_t received_value = -1;
    if (latest_var.is_int()) {
      received_value = latest_var.AsInt();
    } else if (latest_var.is_double()) {
      received_value = static_cast<int32_t>(latest_var.AsDouble() + 0.5);
    }
    ASSERT_TRUE(received_value >= 0);
    ASSERT_TRUE(received_value <= kThreadsToRun);
    ++received_counts[received_value];
  }
  ASSERT_EQ(received_counts, expected_counts);

  message_data_.clear();
  ASSERT_TRUE(ClearListeners());

  PASS();
}
