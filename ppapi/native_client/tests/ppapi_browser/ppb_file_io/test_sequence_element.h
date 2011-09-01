// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef NATIVE_CLIENT_TESTS_PPAPI_BROWSER_PPB_FILE_IO_TEST_SEQUENCE_ELEMENT_H_
#define NATIVE_CLIENT_TESTS_PPAPI_BROWSER_PPB_FILE_IO_TEST_SEQUENCE_ELEMENT_H_

#include <deque>
#include <string>
#include <tr1/functional>

#include "native_client/src/include/nacl_base.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_file_info.h"
#include "ppapi/native_client/tests/ppapi_test_lib/test_interface.h"

namespace common {

class TestSequenceElement;

// TestCallbackData is used to pass data from on test sequence element to
// another, e.g. file io, file ref, etc.
struct TestCallbackData {
  TestCallbackData(PP_Resource system,
                   PP_FileInfo info,
                   std::deque<TestSequenceElement*> sequence)
      : file_system(system), file_info(info),
      existing_file_io(kInvalidResource), existing_file_ref(kInvalidResource),
      non_existing_file_io(kInvalidResource),
      non_existing_file_ref(kInvalidResource), expected_result(PP_OK),
      expected_return_value(PP_OK_COMPLETIONPENDING), data(NULL),
      test_sequence(sequence) {}

  const PP_Resource file_system;
  const PP_FileInfo file_info;
  PP_Resource existing_file_io;
  PP_Resource existing_file_ref;
  PP_Resource non_existing_file_io;
  PP_Resource non_existing_file_ref;
  // The following known properties of the file assigned to the file before
  // testing.
  int32_t expected_result;  // The expected result of a callback operation.
  int32_t expected_return_value;
  void* data;  // Used to attach contextual data
  // Array of test functions that themselve can invoke functions which trigger a
  // callback.
  std::deque<TestSequenceElement*> test_sequence;
};

// OpenFileForTest is an element in a test sequence used to open the existing
typedef std::tr1::function<int32_t (PP_CompletionCallback)> BoundPPAPIFunc;

// TestSequenceElement is a specific section of a sequence of tests.
// Each element may or may not have side effects that effect the subsequent test
// sequence elements (e.g. opening or closing a file).
// Example:
// class ConcreteTestElement : public TestSequenceElement {
//  private:
//   virtual BoundPPAPIFunc
//       GetCompletionCallbackInitiatingPPAPIFunction(
//           TestCallbackData* callback_data) {
//     return std::tr1::bind(PPBFileRef()->Delete,
//                           callback_data->existing_file_ref,
//                           std::tr1::placeholder::_1);
//   }
//
// void TestFoo() {
//   PP_FileInfo file_info;
//   InitFileInfo(PP_FILESYSTEMTYPE_LOCALTEMPORARY, &file_info);
//   FileIOTester tester(file_info);
//   tester.AddSequenceElement(new ConcreteTestElement);
//   tester.Run();
// }
class TestSequenceElement {
 public:
  explicit TestSequenceElement(const std::string& name)
      : name_(name), expected_return_value_(PP_OK_COMPLETIONPENDING),
      expected_result_(PP_OK) {}
  TestSequenceElement(const std::string& name, int32_t expected_return_value,
                      int32_t expected_result)
      : name_(name), expected_return_value_(expected_return_value),
      expected_result_(expected_result) {}
  virtual ~TestSequenceElement();

  // TestSequenceElementForwardingCallback is the callback that is called from
  // the previous sequence element's invocation of a ppapi function which calls
  // a completion callback.
  static void TestSequenceElementForwardingCallback(void* data, int32_t result);
  static void CleanupResources(TestCallbackData* callback_data);

  // Run executes the element's section of the test sequence.
  // |callback_data| is a pointer to data being passed through each element.
  // |result| is the result of the ppapi operation invoked by previous element
  // in the test sequence.
  void Run(TestCallbackData* callback_data,
           int32_t result);
  const char* name() { return name_.c_str(); }

 private:
  TestSequenceElement();  // Disable default ctor

  static void TestSequenceTerminatingCallback(void* data, int32_t result);

  // Returns a PPAPI function that initiates a completion callback.  This is
  // produced by taking a PPAPI function that takes a completion callback as the
  // last argument and possibly binding every other parameter with values from
  // the given test data (e.g.):
  //   return std::tr1::bind(PPB_FileRef()->Delete,
  //                         callback_data->existing_file_ref,
  //                         std::tr1::_1)
  // Subclasses define this function to chain the test sequence element with
  // ppapi functions.
  virtual BoundPPAPIFunc GetCompletionCallbackInitiatingPPAPIFunction(
      TestCallbackData* callback_data);
  // Set up.  Can be a no-op if not defined by subcass.
  virtual void Setup(TestCallbackData* callback_data) {}
  // Implemented by subclasses to verify results of previously test sequence
  // elements or modify known states for subsequent test elements.
  virtual void Execute(TestCallbackData* callback_data) {}

  void Next(TestCallbackData* callback_data);

  const std::string name_;
  const int32_t expected_return_value_;
  const int32_t expected_result_;

  DISALLOW_COPY_AND_ASSIGN(TestSequenceElement);
};

}  // namespace common

#endif  // NATIVE_CLIENT_TESTS_PPAPI_BROWSER_PPB_FILE_IO_TEST_SEQUENCE_ELEMENT_H_
