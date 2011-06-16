// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Functions and constants for test registration and setup.
//
// NOTE: These must be implemented by the tester:
// - SetupTests()
// - SetupPluginInterfaces()
//
// Sample Usage:
//
//   void MyCallback(void* user_data, int32_t result) { ... }
//
//   PP_Var TestPPBFooDeprecated() {
//     // sync test case
//     PP_Resource my_resource = PPBFoo()->Create(kInvalidInstance);
//     EXPECT(my_resource == kInvalidResource);
//
//     // async test case
//     PP_CompletionCallback testable_callback = MakeTestableCompletionCallback(
//         "MyCallback", MyCallback, NULL);
//     int32_t pp_error = PPBFoo()->AsyncFunction(testable_callback);
//     EXPECT(pp_error == PP_OK_COMPLETIONPENDING);
//
//     return TEST_PASSED;
//   }
//
//   void TestPPBFoo() {
//     // sync test case
//     PP_Resource my_resource = PPBFoo()->Create(kInvalidInstance);
//     EXPECT_ASYNC(my_resource == kInvalidResource);
//
//     // async test case
//     PP_CompletionCallback testable_callback = MakeTestableCompletionCallback(
//         "MyCallback", MyCallback, NULL);
//     int32_t pp_error = PPBFoo()->AsyncFunction(testable_callback);
//     EXPECT_ASYNC(pp_error == PP_OK_COMPLETIONPENDING);
//
//     TEST_PASSED_ASYNC;
//   }
//
//   void SetupTests() {
//     RegisterScriptableTest("TestFooDeprecated", TestPPBFooDeprecated);
//     RegisterTest("TestPPBFoo", TestPPBFoo);
//   }
//
//   const PPP_Bar ppp_bar_interface = { ... };
//
//   void SetupPluginInterface() {
//     RegisterPluginInterface(PPP_BAR_INTERFACE, &ppp_bar_interface);
//   }
//

#ifndef NATIVE_CLIENT_TESTS_PPAPI_TEST_PPB_TEMPLATE_TEST_INTERFACE_H
#define NATIVE_CLIENT_TESTS_PPAPI_TEST_PPB_TEMPLATE_TEST_INTERFACE_H

#include <stdio.h>
#include <limits>

#include "native_client/src/include/nacl_string.h"

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"

////////////////////////////////////////////////////////////////////////////////
// These must be implemented by the tester
////////////////////////////////////////////////////////////////////////////////

// Use RegisterTest() to register each TestFunction.
// Use RegisterScriptableTest() to register each ScriptableTestFunction.
void SetupTests();
// Use RegisterPluginInterface() to register custom PPP_ interfaces other than
// PPP_Instance that is required and provided by default.
void SetupPluginInterfaces();

////////////////////////////////////////////////////////////////////////////////
// Test helpers
////////////////////////////////////////////////////////////////////////////////

// Registers test_function, so it is callable from JS using
// plugin.postMessage(test_name);
typedef void (*TestFunction)();
void RegisterTest(nacl::string test_name, TestFunction test_function);

// DEPRECATED: Registers test_function, so it is callable from JS
// using plugin.test_name().
typedef PP_Var (*ScriptableTestFunction)();
void RegisterScriptableTest(nacl::string test_name,
                            ScriptableTestFunction test_function);

// Registers ppp_interface, so it is returned by PPP_GetInterface().
void RegisterPluginInterface(const char* interface_name,
                             const void* ppp_interface);

// Helper for creating user callbacks whose invocation will be reported to JS.
PP_CompletionCallback MakeTestableCompletionCallback(
    const char* callback_name,  // same as passed JS waitForCallback()
    PP_CompletionCallback_Func func,
    void* user_data);

// Uses PPB_Messaging interface to post "test_name:message".
void PostTestMessage(nacl::string test_name, nacl::string message);

// Use these macros to verify the result of a test and report failures.
// TODO(polina): rename EXPECT_ASYNC to EXPECT when sync scripting is removed.
#define EXPECT_ASYNC(expr) do { \
  if (!(expr)) { \
    fprintf(stderr, \
            "ERROR: [%s] failed at %s:%d\n", #expr, __FILE__, __LINE__); \
    PostTestMessage(__FUNCTION__, "ERROR"); \
  } \
} while (0)

#define EXPECT(expr) do { \
  if (!(expr)) { \
    fprintf(stderr, \
            "ERROR: [%s] failed at %s:%d\n", #expr, __FILE__, __LINE__); \
    return PP_MakeBool(PP_FALSE); \
  } \
} while (0)

// Use these macros to report success.
#define TEST_PASSED_ASYNC PostTestMessage(__FUNCTION__, "PASSED");
#define TEST_PASSED PP_MakeBool(PP_TRUE)  // Usage: return TEST_PASSED;

// Use this constant for stress testing
// (i.e. creating and using a large number of resources).
const int kManyResources = 1000;

const PP_Instance kInvalidInstance = 0;
const PP_Module kInvalidModule = 0;
const PP_Resource kInvalidResource = 0;

// These should not exist.
// Chrome uses the bottom 2 bits to differentiate between different id types.
// 00 - module, 01 - instance, 10 - resource, 11 - var.
const PP_Instance kNotAnInstance = 0xFFFFF0;
const PP_Resource kNotAResource = 0xAAAAA0;

// Interface pointers and ids corresponding to this plugin;
// set at initialization/creation.
PP_Instance pp_instance();
PP_Module pp_module();

#endif  // NATIVE_CLIENT_TESTS_PPAPI_TEST_PPB_TEMPLATE_TEST_INTERFACE_H
