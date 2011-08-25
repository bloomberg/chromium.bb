// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/tests/ppapi_test_lib/test_interface.h"

#include <string.h>
#include <map>
#include <new>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "native_client/tests/ppapi_test_lib/internal_utils.h"

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_messaging.h"
#include "ppapi/c/ppb_var.h"

void PostTestMessage(nacl::string test_name, nacl::string message) {
  nacl::string test_message = test_name;
  test_message += ":";
  test_message += message;
  PP_Var post_var = PPBVar()->VarFromUtf8(pp_instance(),
                                          test_message.c_str(),
                                          test_message.size());
  PPBMessaging()->PostMessage(pp_instance(), post_var);
  PPBVar()->Release(post_var);
}

PP_Var PP_MakeString(const char* s) {
  return PPBVar()->VarFromUtf8(pp_module(), s, strlen(s));
}

nacl::string StringifyVar(const PP_Var& var) {
  uint32_t dummy_size;
  switch (var.type) {
    default:
     return "<UNKNOWN>" +  toString(var.type);
    case  PP_VARTYPE_NULL:
      return "<NULL>";
    case  PP_VARTYPE_BOOL:
     return "<BOOL>" + toString(var.value.as_bool);
    case  PP_VARTYPE_INT32:
     return "<INT32>" + toString(var.value.as_int);
    case  PP_VARTYPE_DOUBLE:
     return "<DOUBLE>" + toString(var.value.as_double);
    case PP_VARTYPE_STRING:
     return "<STRING>" + nacl::string(PPBVar()->VarToUtf8(var, &dummy_size));
  }
}

////////////////////////////////////////////////////////////////////////////////
// Test registration
////////////////////////////////////////////////////////////////////////////////

namespace {

class TestTable {
 public:
  // Return singleton intsance.
  static TestTable* Get() {
    static TestTable table;
    return &table;
  }

  void AddTest(nacl::string test_name, TestFunction test_function) {
    test_map_[test_name] = test_function;
  }
  void RunTest(nacl::string test_name);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(TestTable);

  TestTable() {}

  typedef std::map<nacl::string, TestFunction> TestMap;
  TestMap test_map_;
};

void TestTable::RunTest(nacl::string test_name) {
  TestMap::iterator it = test_map_.find(test_name);
  if (it == test_map_.end()) {
    PostTestMessage(test_name, "NOTFOUND");
    return;
  }
  CHECK(it->second != NULL);
  TestFunction test_function = it->second;
  return test_function();
}

}  // namespace

void RegisterTest(nacl::string test_name, TestFunction test_func) {
  TestTable::Get()->AddTest(test_name, test_func);
}

void RunTest(nacl::string test_name) {
  TestTable::Get()->RunTest(test_name);
}

////////////////////////////////////////////////////////////////////////////////
// Testable callback support
////////////////////////////////////////////////////////////////////////////////

namespace {

struct CallbackInfo {
  nacl::string callback_name;
  PP_CompletionCallback user_callback;
};

void ReportCallbackInvocationToJS(const char* callback_name) {
  PP_Var callback_var = PPBVar()->VarFromUtf8(pp_module(),
                                              callback_name,
                                              strlen(callback_name));
  // Report using postmessage for async tests.
  PPBMessaging()->PostMessage(pp_instance(), callback_var);
  PPBVar()->Release(callback_var);
}

void CallbackWrapper(void* user_data, int32_t result) {
  CallbackInfo* callback_info = reinterpret_cast<CallbackInfo*>(user_data);
  PP_RunCompletionCallback(&callback_info->user_callback, result);
  ReportCallbackInvocationToJS(callback_info->callback_name.c_str());
  delete callback_info;
}

}  // namespace

PP_CompletionCallback MakeTestableCompletionCallback(
    const char* callback_name,  // Tested for by JS harness.
    PP_CompletionCallback_Func func,
    void* user_data) {
  CHECK(callback_name != NULL && strlen(callback_name) > 0);
  CHECK(func != NULL);

  CallbackInfo* callback_info = new(std::nothrow) CallbackInfo;
  CHECK(callback_info != NULL);
  callback_info->callback_name = callback_name;
  callback_info->user_callback =
    PP_MakeOptionalCompletionCallback(func, user_data);

  return PP_MakeOptionalCompletionCallback(CallbackWrapper, callback_info);
}

PP_CompletionCallback MakeTestableCompletionCallback(
    const char* callback_name,  // Tested for by JS harness.
    PP_CompletionCallback_Func func) {
  return MakeTestableCompletionCallback(callback_name, func, NULL);
}
