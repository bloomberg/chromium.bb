// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_query_policy.h"

#include "ppapi/c/dev/ppb_query_policy_dev.h"
#include "ppapi/c/dev/ppp_policy_update_dev.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(QueryPolicy);

namespace {

// Since this is a unittest, we don't care about a global string.
std::string g_received_policy;

void PolicyUpdated(PP_Instance instance, PP_Var pp_policy_json) {
  g_received_policy = pp::Var(pp::Var::DontManage(), pp_policy_json).AsString();
  GetTestingInterface()->QuitMessageLoop(instance);
}

static PPP_PolicyUpdate_Dev policy_updated_interface = {
  &PolicyUpdated,
};

}  // namespace

bool TestQueryPolicy::Init() {
  query_policy_interface_ = static_cast<PPB_QueryPolicy_Dev const*>(
      pp::Module::Get()->GetBrowserInterface(PPB_QUERY_POLICY_DEV_INTERFACE));
  pp::Module::Get()->AddPluginInterface(PPP_POLICY_UPDATE_DEV_INTERFACE,
                                        &policy_updated_interface);

  return query_policy_interface_ && InitTestingInterface();
}

void TestQueryPolicy::RunTest() {
  RUN_TEST(SubscribeToPolicyUpdates);
}

std::string TestQueryPolicy::TestSubscribeToPolicyUpdates() {
  query_policy_interface_->SubscribeToPolicyUpdates(
      instance_->pp_instance());

  // Wait for a response on PPP_PolicyUpdate_Dev.
  GetTestingInterface()->RunMessageLoop(instance_->pp_instance());

  ASSERT_TRUE(g_received_policy == "{\"test_policy\": \"i like bananas\"}");

  PASS();
}
