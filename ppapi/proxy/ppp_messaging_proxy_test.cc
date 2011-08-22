// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>

#include "base/synchronization/waitable_event.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppp_messaging.h"
#include "ppapi/proxy/ppapi_proxy_test.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {
namespace proxy {

namespace {

// This is a poor man's mock of PPP_Messaging using global variables. Eventually
// we should generalize making PPAPI interface mocks by using IDL or macro/
// template magic.
PP_Instance received_instance;
PP_Var received_var;
base::WaitableEvent handle_message_called(false, false);

void HandleMessage(PP_Instance instance, PP_Var message_data) {
  received_instance = instance;
  received_var = message_data;
  handle_message_called.Signal();
}

// Clear all the 'received' values for our mock.  Call this before you expect
// one of the functions to be invoked.
void ResetReceived() {
  received_instance = 0;
  received_var.type = PP_VARTYPE_UNDEFINED;
  received_var.value.as_id = 0;
}

PPP_Messaging ppp_messaging_mock = {
  &HandleMessage
};

// Define a fake PPB_Var for the host side so that we can send a string var.
void AddRef(PP_Var /*var*/) {
}
void Release(PP_Var /*var*/) {
}
PP_Var VarFromUtf8(PP_Module /*module*/, const char* /*data*/, uint32_t len) {
  return PP_MakeUndefined();
}
// No matter what id we're given, always provide kTestString and its length.
const std::string kTestString = "Hello world!";
const char* VarToUtf8(PP_Var /*var*/, uint32_t* len) {
  *len = kTestString.size();
  return kTestString.c_str();
}

PPB_Var ppb_var_mock = {
  &AddRef,
  &Release,
  &VarFromUtf8,
  &VarToUtf8
};

}  // namespace

class PPP_Messaging_ProxyTest : public TwoWayTest {
 public:
   PPP_Messaging_ProxyTest()
       : TwoWayTest(TwoWayTest::TEST_PPP_INTERFACE) {
     plugin().RegisterTestInterface(PPP_MESSAGING_INTERFACE,
                                    &ppp_messaging_mock);
     host().RegisterTestInterface(PPB_VAR_INTERFACE, &ppb_var_mock);
   }
};

TEST_F(PPP_Messaging_ProxyTest, SendMessages) {
  // Grab the host-side proxy of ppp_messaging.
  const PPP_Messaging* ppp_messaging = static_cast<const PPP_Messaging*>(
      host().host_dispatcher()->GetProxiedInterface(
          PPP_MESSAGING_INTERFACE));

  PP_Instance expected_instance = pp_instance();
  PP_Var expected_var = PP_MakeUndefined();
  ResetReceived();
  ppp_messaging->HandleMessage(expected_instance, expected_var);
  handle_message_called.Wait();
  EXPECT_EQ(expected_instance, received_instance);
  EXPECT_EQ(expected_var.type, received_var.type);

  expected_var = PP_MakeNull();
  ResetReceived();
  ppp_messaging->HandleMessage(expected_instance, expected_var);
  handle_message_called.Wait();
  EXPECT_EQ(expected_instance, received_instance);
  EXPECT_EQ(expected_var.type, received_var.type);

  expected_var = PP_MakeBool(PP_TRUE);
  ResetReceived();
  ppp_messaging->HandleMessage(expected_instance, expected_var);
  handle_message_called.Wait();
  EXPECT_EQ(expected_instance, received_instance);
  EXPECT_EQ(expected_var.type, received_var.type);
  EXPECT_EQ(expected_var.value.as_bool, received_var.value.as_bool);

  expected_var = PP_MakeInt32(12345);
  ResetReceived();
  ppp_messaging->HandleMessage(expected_instance, expected_var);
  handle_message_called.Wait();
  EXPECT_EQ(expected_instance, received_instance);
  EXPECT_EQ(expected_var.type, received_var.type);
  EXPECT_EQ(expected_var.value.as_int, received_var.value.as_int);

  expected_var = PP_MakeDouble(3.1415);
  ResetReceived();
  ppp_messaging->HandleMessage(expected_instance, expected_var);
  handle_message_called.Wait();
  EXPECT_EQ(expected_instance, received_instance);
  EXPECT_EQ(expected_var.type, received_var.type);
  EXPECT_EQ(expected_var.value.as_double, received_var.value.as_double);

  expected_var.type = PP_VARTYPE_STRING;
  expected_var.value.as_id = 1979;
  ResetReceived();
  ppp_messaging->HandleMessage(expected_instance, expected_var);
  handle_message_called.Wait();
  EXPECT_EQ(expected_instance, received_instance);
  EXPECT_EQ(expected_var.type, received_var.type);

  StringVar* received_string = StringVar::FromPPVar(received_var);
  ASSERT_TRUE(received_string);
  EXPECT_EQ(kTestString, received_string->value());
  // Now release the var, and the string should go away (because the ref
  // count should be one).
  plugin().var_tracker().ReleaseVar(received_var);
  EXPECT_FALSE(StringVar::FromPPVar(received_var));
}

}  // namespace proxy
}  // namespace ppapi

