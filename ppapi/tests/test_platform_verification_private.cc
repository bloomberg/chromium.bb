// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_platform_verification_private.h"

#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/platform_verification.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_array_buffer.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(PlatformVerificationPrivate);

TestPlatformVerificationPrivate::TestPlatformVerificationPrivate(
    TestingInstance* instance)
    : TestCase(instance) {}

void TestPlatformVerificationPrivate::RunTests(const std::string& filter) {
  RUN_CALLBACK_TEST(TestPlatformVerificationPrivate, ChallengePlatform, filter);
}

std::string TestPlatformVerificationPrivate::TestChallengePlatform() {
  pp::PlatformVerification platform_verification_api(instance_);

  pp::VarArrayBuffer challenge_array(256);
  uint8_t* var_data = static_cast<uint8_t*>(challenge_array.Map());
  for (uint32_t i = 0; i < challenge_array.ByteLength(); ++i)
    var_data[i] = i;

  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  std::string service_id_str("fake.service.id");
  pp::Var signed_data, signed_data_signature, platform_key_certificate;
  callback.WaitForResult(platform_verification_api.ChallengePlatform(
      pp::Var(service_id_str), challenge_array, &signed_data,
      &signed_data_signature, &platform_key_certificate,
      callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_ERROR_FAILED, callback.result());
  PASS();
}
