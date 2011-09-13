// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/string_number_conversions.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/proxy/ppapi_proxy_test.h"
#include "ppapi/proxy/ppb_var_proxy.h"

// TODO(dmichael): Make PPB_Var_Proxy and PluginResourceTracker thread-safe and
// add thread-safety tests here.

namespace {
std::string VarToString(const PP_Var& var, const PPB_Var* ppb_var) {
  uint32_t len = 0;
  const char* utf8 = ppb_var->VarToUtf8(var, &len);
  return std::string(utf8, len);
}
}  // namespace

namespace ppapi {
namespace proxy {

class PPB_VarTest : public PluginProxyTest {
 public:
  PPB_VarTest() {}
};

TEST_F(PPB_VarTest, Strings) {
  const PPB_Var* ppb_var = GetPPB_Var_Interface();

  // Make a vector of strings, where the value of test_strings[i] is "i".
  const int kNumStrings = 5;
  std::vector<std::string> test_strings(kNumStrings);
  for (int i = 0; i < kNumStrings; ++i)
    test_strings[i] = base::IntToString(i);

  std::vector<PP_Var> vars(kNumStrings);
  for (int i = 0; i < kNumStrings; ++i) {
    vars[i] = ppb_var->VarFromUtf8(pp_module(),
                                   test_strings[i].c_str(),
                                   test_strings[i].length());
    EXPECT_EQ(test_strings[i], VarToString(vars[i], ppb_var));
  }
  // At this point, they should each have a ref count of 1. Add some more.
  const int kRefsToAdd = 3;
  for (int ref = 0; ref < kRefsToAdd; ++ref) {
    for (int i = 0; i < kNumStrings; ++i) {
      ppb_var->AddRef(vars[i]);
      // Make sure the string is still there with the right value.
      EXPECT_EQ(test_strings[i], VarToString(vars[i], ppb_var));
    }
  }
  for (int ref = 0; ref < kRefsToAdd; ++ref) {
    for (int i = 0; i < kNumStrings; ++i) {
      ppb_var->Release(vars[i]);
      // Make sure the string is still there with the right value.
      EXPECT_EQ(test_strings[i], VarToString(vars[i], ppb_var));
    }
  }
  // Now remove the ref counts for each string and make sure they are gone.
  for (int i = 0; i < kNumStrings; ++i) {
    ppb_var->Release(vars[i]);
    uint32_t len = 10;
    const char* utf8 = ppb_var->VarToUtf8(vars[i], &len);
    EXPECT_EQ(NULL, utf8);
    EXPECT_EQ(0u, len);
  }
}

}  // namespace proxy
}  // namespace ppapi

