// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/test/scoped_task_environment.h"
#include "device/fido/bio/enrollment_handler.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/test_callback_receiver.h"
#include "device/fido/virtual_fido_device_factory.h"

namespace device {
namespace {

class BioEnrollmentHandlerTest : public ::testing::Test {
 protected:
  std::unique_ptr<BioEnrollmentHandler> MakeHandler() {
    return std::make_unique<BioEnrollmentHandler>(
        /*connector=*/nullptr, &virtual_device_factory_,
        base::flat_set<FidoTransportProtocol>{
            FidoTransportProtocol::kUsbHumanInterfaceDevice},
        ready_callback_.callback());
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  test::TestCallbackReceiver<> ready_callback_;
  test::VirtualFidoDeviceFactory virtual_device_factory_;
};

// Tests getting authenticator modality without pin auth.
TEST_F(BioEnrollmentHandlerTest, Modality) {
  VirtualCtap2Device::Config config;
  config.bio_enrollment_support = true;

  virtual_device_factory_.SetCtap2Config(config);
  virtual_device_factory_.SetSupportedProtocol(ProtocolVersion::kCtap2);

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();

  test::StatusAndValueCallbackReceiver<CtapDeviceResponseCode,
                                       base::Optional<BioEnrollmentResponse>>
      cb;
  handler->GetModality(cb.callback());
  cb.WaitForCallback();

  EXPECT_EQ(cb.status(), CtapDeviceResponseCode::kSuccess);
  EXPECT_TRUE(cb.value());
  EXPECT_EQ(cb.value()->modality, BioEnrollmentModality::kFingerprint);
}

// Tests getting authenticator modality without pin auth.
TEST_F(BioEnrollmentHandlerTest, FingerprintSensorInfo) {
  VirtualCtap2Device::Config config;
  config.bio_enrollment_support = true;

  virtual_device_factory_.SetCtap2Config(config);
  virtual_device_factory_.SetSupportedProtocol(ProtocolVersion::kCtap2);

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();

  test::StatusAndValueCallbackReceiver<CtapDeviceResponseCode,
                                       base::Optional<BioEnrollmentResponse>>
      cb;
  handler->GetSensorInfo(cb.callback());
  cb.WaitForCallback();

  EXPECT_EQ(cb.status(), CtapDeviceResponseCode::kSuccess);
  EXPECT_TRUE(cb.value());
  EXPECT_EQ(cb.value()->fingerprint_kind, BioEnrollmentFingerprintKind::kTouch);
  EXPECT_EQ(cb.value()->max_samples_for_enroll, 7);
}

// Tests bio enrollment commands against an authenticator lacking support.
TEST_F(BioEnrollmentHandlerTest, NoBioEnrollmentSupport) {
  VirtualCtap2Device::Config config;

  virtual_device_factory_.SetCtap2Config(config);
  virtual_device_factory_.SetSupportedProtocol(ProtocolVersion::kCtap2);

  auto handler = MakeHandler();
  ready_callback_.WaitForCallback();

  // Test unsupported bio-enrollment command.
  test::StatusAndValueCallbackReceiver<CtapDeviceResponseCode,
                                       base::Optional<BioEnrollmentResponse>>
      cb0;
  handler->GetModality(cb0.callback());
  cb0.WaitForCallback();

  EXPECT_EQ(cb0.status(), CtapDeviceResponseCode::kCtap2ErrUnsupportedOption);
  EXPECT_FALSE(cb0.value());

  // Test second command - handler should not crash after last command.
  test::StatusAndValueCallbackReceiver<CtapDeviceResponseCode,
                                       base::Optional<BioEnrollmentResponse>>
      cb1;
  handler->GetSensorInfo(cb1.callback());
  cb1.WaitForCallback();

  EXPECT_EQ(cb1.status(), CtapDeviceResponseCode::kCtap2ErrUnsupportedOption);
  EXPECT_FALSE(cb1.value());
}

}  // namespace
}  // namespace device
