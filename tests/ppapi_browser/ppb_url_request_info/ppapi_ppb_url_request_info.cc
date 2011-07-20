// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests PPB_URLRequestInfo.

#include <string.h>

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "native_client/tests/ppapi_test_lib/test_interface.h"

#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/ppb_url_request_info.h"
#include "ppapi/c/ppb_var.h"

namespace {

// Tests
//   PP_Resource Create(PP_Instance instance).
void TestCreate() {
  PP_Resource url_request = kInvalidResource;

  // Invalid instance -> invalid resource.
  url_request = PPBURLRequestInfo()->Create(kInvalidInstance);
  EXPECT(url_request == kInvalidResource);

  // Valid instance -> valid resource.
  url_request = PPBURLRequestInfo()->Create(pp_instance());
  EXPECT(url_request != kInvalidResource);

  PPBCore()->ReleaseResource(url_request);
  TEST_PASSED;
}

// Tests
//   PP_Bool IsURLRequestInfo(PP_Resource resource).
void TestIsURLRequestInfo() {
  const PPB_URLRequestInfo* ppb = PPBURLRequestInfo();

  // Invalid / non-existent / non-URLRequestInfo resource -> false.
  EXPECT(PP_FALSE == ppb->IsURLRequestInfo(kInvalidResource));
  EXPECT(PP_FALSE == ppb->IsURLRequestInfo(kNotAResource));
  PP_Resource url_loader = PPBURLLoader()->Create(pp_instance());
  CHECK(url_loader != kInvalidResource);
  EXPECT(PP_FALSE == ppb->IsURLRequestInfo(url_loader));
  PPBCore()->ReleaseResource(url_loader);

  // Current URLRequestInfo resource -> true.
  PP_Resource url_request = ppb->Create(pp_instance());
  EXPECT(PP_TRUE == ppb->IsURLRequestInfo(url_request));

  // Released URLRequestInfo resource -> false.
  PPBCore()->ReleaseResource(url_request);
  EXPECT(PP_FALSE == ppb->IsURLRequestInfo(url_request));

  TEST_PASSED;
}

// Tests
//  PP_Bool SetProperty(PP_Resource request,
//                      PP_URLRequestProperty property,
//                      struct PP_Var value);
//
// TODO(polina,elijahtaylor): this needs better coverage.
// Almost all of the properties are strings and bools.
// It would be easy to have a list of bool properties to test in a loop,
// and a list of string properties that have one or more valid inputs and
// at least one invalid input. You could also easily feed bad types,
// undefined vars, etc for each property then.
void TestSetProperty() {
  const PPB_URLRequestInfo* ppb = PPBURLRequestInfo();
  PP_Resource url_request = ppb->Create(pp_instance());
  PP_Var string_var;
  CHECK(url_request != kInvalidResource);

  // Valid string -> true.
  string_var = PPBVar()->VarFromUtf8(pp_module(), "", 0);
  EXPECT(PP_TRUE == ppb->SetProperty(
      url_request,
      PP_URLREQUESTPROPERTY_URL,
      string_var));
  PPBVar()->Release(string_var);
  const char* url = "www.google.com";
  string_var = PPBVar()->VarFromUtf8(pp_module(), url, strlen(url));
  EXPECT(PP_TRUE == ppb->SetProperty(
      url_request,
      PP_URLREQUESTPROPERTY_URL,
      string_var));
  PPBVar()->Release(string_var);
  // Valid bool -> true.
  EXPECT(PP_TRUE == ppb->SetProperty(
      url_request,
      PP_URLREQUESTPROPERTY_STREAMTOFILE,
      PP_MakeBool(PP_FALSE)));
  EXPECT(PP_TRUE == ppb->SetProperty(
      url_request,
      PP_URLREQUESTPROPERTY_STREAMTOFILE,
      PP_MakeBool(PP_TRUE)));
  // Valid int32 -> true.
  EXPECT(PP_TRUE == ppb->SetProperty(
      url_request,
      PP_URLREQUESTPROPERTY_PREFETCHBUFFERUPPERTHRESHOLD,
      PP_MakeInt32(0)));
  EXPECT(PP_TRUE == ppb->SetProperty(
      url_request,
      PP_URLREQUESTPROPERTY_PREFETCHBUFFERUPPERTHRESHOLD,
      PP_MakeInt32(100)));
  // Bug filed against Chrome to error check such out of range
  // values: http://code.google.com/p/chromium/issues/detail?id=89842.
  EXPECT(PP_TRUE == ppb->SetProperty(
      url_request,
      PP_URLREQUESTPROPERTY_PREFETCHBUFFERUPPERTHRESHOLD,
      PP_MakeInt32(-100)));

  // Invalid string -> false.
  EXPECT(PP_FALSE == ppb->SetProperty(
      url_request,
      PP_URLREQUESTPROPERTY_METHOD,
      PP_MakeUndefined()));
  EXPECT(PP_FALSE == ppb->SetProperty(
      url_request,
      PP_URLREQUESTPROPERTY_METHOD,
      PP_MakeBool(PP_TRUE)));
  // Invalid bool -> false.
  EXPECT(PP_FALSE == ppb->SetProperty(
      url_request,
      PP_URLREQUESTPROPERTY_FOLLOWREDIRECTS,
      PP_MakeUndefined()));
  EXPECT(PP_FALSE == ppb->SetProperty(
      url_request,
      PP_URLREQUESTPROPERTY_FOLLOWREDIRECTS,
      PP_MakeInt32(123)));
  // Invalid int32 -> false.
  EXPECT(PP_FALSE == ppb->SetProperty(
      url_request,
      PP_URLREQUESTPROPERTY_PREFETCHBUFFERLOWERTHRESHOLD,
      PP_MakeUndefined()));
  EXPECT(PP_FALSE == ppb->SetProperty(
      url_request,
      PP_URLREQUESTPROPERTY_PREFETCHBUFFERLOWERTHRESHOLD,
      PP_MakeBool(PP_FALSE)));

  PPBCore()->ReleaseResource(url_request);
  TEST_PASSED;
}

// Allocates and manipulates a large number of resources.
void TestStress() {
  const int kManyResources = 500;
  PP_Resource url_request_info[kManyResources];
  const PPB_URLRequestInfo* ppb = PPBURLRequestInfo();

  for (int i = 0; i < kManyResources; i++) {
    url_request_info[i] = ppb->Create(pp_instance());
    EXPECT(url_request_info[i] != kInvalidResource);
    EXPECT(PP_TRUE == ppb->IsURLRequestInfo(url_request_info[i]));
    EXPECT(PP_TRUE == ppb->SetProperty(url_request_info[i],
                                       PP_URLREQUESTPROPERTY_STREAMTOFILE,
                                       PP_MakeBool(PP_FALSE)));
  }
  for (int i = 0; i < kManyResources; i++) {
    PPBCore()->ReleaseResource(url_request_info[i]);
    EXPECT(PP_FALSE == ppb->IsURLRequestInfo(url_request_info[i]));
  }

  TEST_PASSED;
}

}  // namespace

void SetupTests() {
  RegisterTest("TestCreate", TestCreate);
  RegisterTest("TestIsURLRequestInfo", TestIsURLRequestInfo);
  RegisterTest("TestSetProperty", TestSetProperty);
  // TODO(polina, elijahtaylor): add missing tests for Append*ToBody functions.
  RegisterTest("TestStress", TestStress);
}

void SetupPluginInterfaces() {
  // none
}
