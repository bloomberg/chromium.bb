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

#include "ppapi/c/dev/ppb_var_deprecated.h"

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_instance.h"

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
  PP_Var RunTest(nacl::string test_name);

 private:
  NACL_DISALLOW_COPY_AND_ASSIGN(TestTable);

  TestTable() {}

  typedef std::map<nacl::string, TestFunction> TestMap;
  TestMap test_map_;
};

PP_Var TestTable::RunTest(nacl::string test_name) {
  TestMap::iterator it = test_map_.find(test_name);
  if (it == test_map_.end())
    return PP_MakeUndefined();
  CHECK(it->second != NULL);
  TestFunction test_function = it->second;
  return test_function();
}

}  // namespace

void RegisterScriptableTest(nacl::string test_name, TestFunction test_func) {
  TestTable::Get()->AddTest(test_name, test_func);
}

PP_Var RunScriptableTest(nacl::string test_name) {
  return TestTable::Get()->RunTest(test_name);
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
  PP_Var js_callback = PPBVarDeprecated()->VarFromUtf8(pp_module(),
                                                       callback_name,
                                                       strlen(callback_name));
  PP_Var window = PPBInstance()->GetWindowObject(pp_instance());
  CHECK(window.type == PP_VARTYPE_OBJECT);

  PP_Var exception = PP_MakeUndefined();
  PPBVarDeprecated()->Call(window, js_callback, 0, NULL, &exception);

  PPBVarDeprecated()->Release(js_callback);
  PPBVarDeprecated()->Release(window);
}

void CallbackWrapper(void* user_data, int32_t result) {
  CallbackInfo* callback_info = reinterpret_cast<CallbackInfo*>(user_data);
  PP_RunCompletionCallback(&callback_info->user_callback, result);
  ReportCallbackInvocationToJS(callback_info->callback_name.c_str());
  delete callback_info;
}

}  // namespace

PP_CompletionCallback MakeTestableCompletionCallback(
    const char* callback_name,  // Also passed to JS waitForCallback()
    PP_CompletionCallback_Func func,
    void* user_data) {
  CHECK(callback_name != NULL && strlen(callback_name) > 0);
  CHECK(func != NULL);

  CallbackInfo* callback_info = new(std::nothrow) CallbackInfo;
  CHECK(callback_info != NULL);
  callback_info->callback_name = callback_name;
  callback_info->user_callback = PP_MakeCompletionCallback(func, user_data);

  return PP_MakeCompletionCallback(CallbackWrapper, callback_info);
}
