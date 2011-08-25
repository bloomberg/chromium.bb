// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/tests/ppapi_browser/ppb_file_io/test_sequence_element.h"

#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/third_party/ppapi/c/ppb_core.h"
#include "native_client/tests/ppapi_test_lib/get_browser_interface.h"

namespace common {

TestSequenceElement::~TestSequenceElement() {}

// TestSequenceElementForwardingCallback is the completion callback which
// connects one test sequence element to another.
void TestSequenceElement::TestSequenceElementForwardingCallback(
    void* data,
    int32_t result) {
  TestCallbackData* callback_data = reinterpret_cast<TestCallbackData*>(data);
  // If result is greater than 0, it is a result of a read/write.  In that case
  // we ignore the result since it represents the number of bytes read/written,
  // which is not guaranteed to be a specific size.
  if (result <= 0)
    EXPECT(result == callback_data->expected_result);
  nacl::scoped_ptr<TestSequenceElement> sequence_element(
      callback_data->test_sequence.front());
  sequence_element->Run(callback_data, result);
}

// TODO(sanga): Move this to file_io_tester.h
void TestSequenceElement::CleanupResources(
    TestCallbackData* callback_data) {  // sink
  PPBCore()->ReleaseResource(callback_data->existing_file_io);
  PPBCore()->ReleaseResource(callback_data->existing_file_ref);
  PPBCore()->ReleaseResource(callback_data->non_existing_file_io);
  PPBCore()->ReleaseResource(callback_data->non_existing_file_ref);
  PPBCore()->ReleaseResource(callback_data->file_system);
  delete callback_data;
}

void TestSequenceElement::Run(TestCallbackData* callback_data, int32_t result) {
  callback_data->expected_return_value = expected_return_value_;
  callback_data->expected_result = expected_result_;
  Setup(callback_data);
  Execute(callback_data);
  // Invoke a ppapi function that kicks off another callback.
  callback_data->test_sequence.pop_front();  // pop self off the dequeue
  Next(callback_data);
}

void TestSequenceElement::TestSequenceTerminatingCallback(void* data,
                                                          int32_t result) {
  TestCallbackData* callback_data = reinterpret_cast<TestCallbackData*>(data);
  // If result is greater than 0, it is a result of a read/write.  In that case
  // we ignore the result since it represents the number of bytes read/written,
  // which is not guaranteed to be a specific size.
  if (result <= 0)
    EXPECT(result == callback_data->expected_result);
  CleanupResources(callback_data);
}

namespace {

int32_t CallbackInvokingNoOpFunction(int32_t delay_in_milliseconds,
                                     PP_CompletionCallback callback) {
  PPBCore()->CallOnMainThread(delay_in_milliseconds, callback, PP_OK);
  return PP_OK_COMPLETIONPENDING;
}

}  // namespace

BoundPPAPIFunc
TestSequenceElement::GetCompletionCallbackInitiatingPPAPIFunction(
    TestCallbackData* /*callback_data*/) {
  return std::tr1::bind(CallbackInvokingNoOpFunction, 0,
                        std::tr1::placeholders::_1);
}

void TestSequenceElement::Next(TestCallbackData* callback_data) {
  PP_CompletionCallback_Func callback_func = NULL;
  std::string callback_name = "END";
  if (!callback_data->test_sequence.empty()) {
    callback_func = TestSequenceElementForwardingCallback;
    TestSequenceElement* next_element = callback_data->test_sequence.front();
    callback_name = next_element->name();
  } else {
    callback_func = TestSequenceTerminatingCallback;
  }
  PP_CompletionCallback completion_callback = MakeTestableCompletionCallback(
      callback_name.c_str(), callback_func, callback_data);
  BoundPPAPIFunc ppapi_func =
      GetCompletionCallbackInitiatingPPAPIFunction(callback_data);
  int32_t pp_error = ppapi_func(completion_callback);
  EXPECT(pp_error == callback_data->expected_return_value);
  if (pp_error != PP_OK_COMPLETIONPENDING) {
    // We need to invoke to the next callback.
    PPBCore()->CallOnMainThread(0,  // no delay
                                completion_callback,
                                callback_data->expected_result);
  }
}

}  // namespace common
