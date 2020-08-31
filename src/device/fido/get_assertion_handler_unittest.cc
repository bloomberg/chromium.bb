// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/stl_util.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/device_response_converter.h"
#include "device/fido/fake_fido_discovery.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device_authenticator.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/get_assertion_request_handler.h"
#include "device/fido/hid/fake_hid_impl_for_testing.h"
#include "device/fido/make_credential_task.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/test_callback_receiver.h"
#include "device/fido/u2f_command_constructor.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "device/fido/win/fake_webauthn_api.h"
#endif  // defined(OS_WIN)

namespace device {

namespace {

constexpr uint8_t kBogusCredentialId[] = {0x01, 0x02, 0x03, 0x04};

using TestGetAssertionRequestCallback = test::StatusAndValuesCallbackReceiver<
    GetAssertionStatus,
    base::Optional<std::vector<AuthenticatorGetAssertionResponse>>,
    const FidoAuthenticator*>;

}  // namespace

using testing::_;

// FidoGetAssertionHandlerTest allows testing GetAssertionRequestHandler against
// MockFidoDevices injected via a FakeFidoDiscoveryFactory.
class FidoGetAssertionHandlerTest : public ::testing::Test {
 public:
  FidoGetAssertionHandlerTest() {
    mock_adapter_ =
        base::MakeRefCounted<::testing::NiceMock<MockBluetoothAdapter>>();
    BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);
  }

  void ForgeDiscoveries() {
    discovery_ = fake_discovery_factory_->ForgeNextHidDiscovery();
    cable_discovery_ = fake_discovery_factory_->ForgeNextCableDiscovery();
    nfc_discovery_ = fake_discovery_factory_->ForgeNextNfcDiscovery();
    platform_discovery_ = fake_discovery_factory_->ForgeNextPlatformDiscovery();
  }

  CtapGetAssertionRequest CreateTestRequestWithCableExtension() {
    CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                    test_data::kClientDataJson);
    request.cable_extension.emplace();
    return request;
  }

  std::unique_ptr<GetAssertionRequestHandler> CreateGetAssertionHandlerU2f() {
    CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                    test_data::kClientDataJson);
    request.allow_list = {PublicKeyCredentialDescriptor(
        CredentialType::kPublicKey,
        fido_parsing_utils::Materialize(test_data::kU2fSignKeyHandle))};
    return CreateGetAssertionHandlerWithRequest(std::move(request));
  }

  std::unique_ptr<GetAssertionRequestHandler> CreateGetAssertionHandlerCtap() {
    CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                    test_data::kClientDataJson);
    request.allow_list = {PublicKeyCredentialDescriptor(
        CredentialType::kPublicKey,
        fido_parsing_utils::Materialize(
            test_data::kTestGetAssertionCredentialId))};
    return CreateGetAssertionHandlerWithRequest(std::move(request));
  }

  std::unique_ptr<GetAssertionRequestHandler>
  CreateGetAssertionHandlerWithRequest(CtapGetAssertionRequest request) {
    ForgeDiscoveries();

    auto handler = std::make_unique<GetAssertionRequestHandler>(
        fake_discovery_factory_.get(), supported_transports_,
        std::move(request),
        /*allow_skipping_pin_touch=*/true, get_assertion_cb_.callback());
    return handler;
  }

  void ExpectAllowedTransportsForRequestAre(
      GetAssertionRequestHandler* request_handler,
      base::flat_set<FidoTransportProtocol> transports) {
    using Transport = FidoTransportProtocol;
    if (base::Contains(transports, Transport::kUsbHumanInterfaceDevice))
      discovery()->WaitForCallToStartAndSimulateSuccess();
    if (base::Contains(transports, Transport::kCloudAssistedBluetoothLowEnergy))
      cable_discovery()->WaitForCallToStartAndSimulateSuccess();
    if (base::Contains(transports, Transport::kNearFieldCommunication))
      nfc_discovery()->WaitForCallToStartAndSimulateSuccess();
    if (base::Contains(transports, Transport::kInternal))
      platform_discovery()->WaitForCallToStartAndSimulateSuccess();

    task_environment_.FastForwardUntilNoTasksRemain();
    EXPECT_FALSE(get_assertion_callback().was_called());

    if (!base::Contains(transports, Transport::kUsbHumanInterfaceDevice))
      EXPECT_FALSE(discovery()->is_start_requested());
    if (!base::Contains(transports,
                        Transport::kCloudAssistedBluetoothLowEnergy))
      EXPECT_FALSE(cable_discovery()->is_start_requested());
    if (!base::Contains(transports, Transport::kNearFieldCommunication))
      EXPECT_FALSE(nfc_discovery()->is_start_requested());
    if (!base::Contains(transports, Transport::kInternal))
      EXPECT_FALSE(platform_discovery()->is_start_requested());

    EXPECT_THAT(
        request_handler->transport_availability_info().available_transports,
        ::testing::UnorderedElementsAreArray(transports));
  }

  void ExpectAllTransportsAreAllowedForRequest(
      GetAssertionRequestHandler* request_handler) {
    ExpectAllowedTransportsForRequestAre(
        request_handler,
        {FidoTransportProtocol::kUsbHumanInterfaceDevice,
         FidoTransportProtocol::kInternal,
         FidoTransportProtocol::kNearFieldCommunication,
         FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy});
  }

  test::FakeFidoDiscovery* discovery() const { return discovery_; }
  test::FakeFidoDiscovery* cable_discovery() const { return cable_discovery_; }
  test::FakeFidoDiscovery* nfc_discovery() const { return nfc_discovery_; }
  test::FakeFidoDiscovery* platform_discovery() const {
    return platform_discovery_;
  }
  TestGetAssertionRequestCallback& get_assertion_callback() {
    return get_assertion_cb_;
  }

  void set_supported_transports(
      base::flat_set<FidoTransportProtocol> transports) {
    supported_transports_ = std::move(transports);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<test::FakeFidoDiscoveryFactory> fake_discovery_factory_ =
      std::make_unique<test::FakeFidoDiscoveryFactory>();
  test::FakeFidoDiscovery* discovery_;
  test::FakeFidoDiscovery* cable_discovery_;
  test::FakeFidoDiscovery* nfc_discovery_;
  test::FakeFidoDiscovery* platform_discovery_;
  scoped_refptr<::testing::NiceMock<MockBluetoothAdapter>> mock_adapter_;
  TestGetAssertionRequestCallback get_assertion_cb_;
  base::flat_set<FidoTransportProtocol> supported_transports_ = {
      FidoTransportProtocol::kUsbHumanInterfaceDevice,
      FidoTransportProtocol::kInternal,
      FidoTransportProtocol::kNearFieldCommunication,
      FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy};
};

TEST_F(FidoGetAssertionHandlerTest, TransportAvailabilityInfo) {
  auto request_handler =
      CreateGetAssertionHandlerWithRequest(CtapGetAssertionRequest(
          test_data::kRelyingPartyId, test_data::kClientDataJson));

  EXPECT_EQ(FidoRequestHandlerBase::RequestType::kGetAssertion,
            request_handler->transport_availability_info().request_type);
}

TEST_F(FidoGetAssertionHandlerTest, CtapRequestOnSingleDevice) {
  auto request_handler = CreateGetAssertionHandlerCtap();
  discovery()->WaitForCallToStartAndSimulateSuccess();
  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponse);

  discovery()->AddDevice(std::move(device));
  get_assertion_callback().WaitForCallback();

  EXPECT_EQ(GetAssertionStatus::kSuccess, get_assertion_callback().status());
  EXPECT_TRUE(get_assertion_callback().value<0>());
}

// Test a scenario where the connected authenticator is a U2F device.
TEST_F(FidoGetAssertionHandlerTest, TestU2fSign) {
  auto request_handler = CreateGetAssertionHandlerU2f();
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeU2fWithGetInfoExpectation();
  device->ExpectRequestAndRespondWith(
      test_data::kU2fSignCommandApdu,
      test_data::kApduEncodedNoErrorSignResponse);

  discovery()->AddDevice(std::move(device));
  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(GetAssertionStatus::kSuccess, get_assertion_callback().status());
  EXPECT_TRUE(get_assertion_callback().value<0>());
}

TEST_F(FidoGetAssertionHandlerTest, TestIncompatibleUserVerificationSetting) {
  auto request = CtapGetAssertionRequest(test_data::kRelyingPartyId,
                                         test_data::kClientDataJson);
  request.user_verification = UserVerificationRequirement::kRequired;
  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation(
      test_data::kTestGetInfoResponseWithoutUvSupport);
  device->ExpectRequestAndRespondWith(
      MockFidoDevice::EncodeCBORRequest(AsCTAPRequestValuePair(
          MakeCredentialTask::GetTouchRequest(device.get()))),
      test_data::kTestMakeCredentialResponse);

  discovery()->AddDevice(std::move(device));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(GetAssertionStatus::kAuthenticatorMissingUserVerification,
            get_assertion_callback().status());
}

TEST_F(FidoGetAssertionHandlerTest,
       TestU2fSignRequestWithUserVerificationRequired) {
  auto request = CtapGetAssertionRequest(test_data::kRelyingPartyId,
                                         test_data::kClientDataJson);
  request.allow_list = {PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey,
      fido_parsing_utils::Materialize(test_data::kU2fSignKeyHandle))};
  request.user_verification = UserVerificationRequirement::kRequired;
  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));
  discovery()->WaitForCallToStartAndSimulateSuccess();

  auto device = MockFidoDevice::MakeU2fWithGetInfoExpectation();
  device->ExpectRequestAndRespondWith(
      ConstructBogusU2fRegistrationCommand(),
      test_data::kApduEncodedNoErrorRegisterResponse);
  discovery()->AddDevice(std::move(device));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_EQ(GetAssertionStatus::kAuthenticatorMissingUserVerification,
            get_assertion_callback().status());
}

TEST_F(FidoGetAssertionHandlerTest, IncorrectRpIdHash) {
  auto request_handler =
      CreateGetAssertionHandlerWithRequest(CtapGetAssertionRequest(
          test_data::kRelyingPartyId, test_data::kClientDataJson));
  discovery()->WaitForCallToStartAndSimulateSuccess();
  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponseWithIncorrectRpIdHash);

  discovery()->AddDevice(std::move(device));

  get_assertion_callback().WaitForCallback();
  EXPECT_EQ(GetAssertionStatus::kAuthenticatorResponseInvalid,
            get_assertion_callback().status());
}

// Tests a scenario where the authenticator responds with credential ID that
// is not included in the allowed list.
TEST_F(FidoGetAssertionHandlerTest, InvalidCredential) {
  CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                  test_data::kClientDataJson);
  request.allow_list = {PublicKeyCredentialDescriptor(
      CredentialType::kPublicKey,
      fido_parsing_utils::Materialize(test_data::kKeyHandleAlpha))};
  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));
  discovery()->WaitForCallToStartAndSimulateSuccess();
  // Resident Keys must be disabled, otherwise allow list check is skipped.
  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation(
      test_data::kTestGetInfoResponseWithoutResidentKeySupport);
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponse);

  discovery()->AddDevice(std::move(device));

  get_assertion_callback().WaitForCallback();
  EXPECT_EQ(GetAssertionStatus::kAuthenticatorResponseInvalid,
            get_assertion_callback().status());
}

// Tests a scenario where the authenticator responds with an empty credential.
// When GetAssertion request only has a single credential in the allow list,
// this is a valid response. Check that credential is set by the client before
// the response is returned to the relying party.
TEST_F(FidoGetAssertionHandlerTest, ValidEmptyCredential) {
  auto request_handler = CreateGetAssertionHandlerCtap();
  discovery()->WaitForCallToStartAndSimulateSuccess();
  // Resident Keys must be disabled, otherwise allow list check is skipped.
  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation(
      test_data::kTestGetInfoResponseWithoutResidentKeySupport);
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponseWithEmptyCredential);
  discovery()->AddDevice(std::move(device));

  get_assertion_callback().WaitForCallback();
  const auto& response = get_assertion_callback().value<0>();
  EXPECT_EQ(GetAssertionStatus::kSuccess, get_assertion_callback().status());
  ASSERT_TRUE(response);
  ASSERT_EQ(1u, response->size());
  EXPECT_TRUE(response.value()[0].credential());
  EXPECT_THAT(
      response.value()[0].raw_credential_id(),
      ::testing::ElementsAreArray(test_data::kTestGetAssertionCredentialId));
}

TEST_F(FidoGetAssertionHandlerTest, TruncatedUTF8) {
  // Webauthn says[1] that authenticators may truncate strings in user entities.
  // Since authenticators aren't going to do UTF-8 processing, that means that
  // they may truncate a multi-byte code point and thus produce an invalid
  // string in the CBOR. This test exercises that case.
  //
  // [1] https://www.w3.org/TR/webauthn/#sctn-user-credential-params
  auto request_handler = CreateGetAssertionHandlerCtap();
  discovery()->WaitForCallToStartAndSimulateSuccess();
  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation(
      test_data::kTestCtap2OnlyAuthenticatorGetInfoResponse);
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponseWithTruncatedUTF8);
  discovery()->AddDevice(std::move(device));

  get_assertion_callback().WaitForCallback();
  const auto& response = get_assertion_callback().value<0>();
  EXPECT_EQ(GetAssertionStatus::kSuccess, get_assertion_callback().status());
  ASSERT_TRUE(response);
  ASSERT_EQ(1u, response->size());
  ASSERT_TRUE(response.value()[0].user_entity());
  EXPECT_EQ(63u, response.value()[0].user_entity()->name->size());
}

TEST_F(FidoGetAssertionHandlerTest, TruncatedAndInvalidUTF8) {
  // This test exercises the case where a UTF-8 string is truncated in a
  // response, and the UTF-8 string contains invalid code-points that
  // |base::IsStringUTF8| will be unhappy with.
  auto request_handler = CreateGetAssertionHandlerCtap();
  discovery()->WaitForCallToStartAndSimulateSuccess();
  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation(
      test_data::kTestCtap2OnlyAuthenticatorGetInfoResponse);
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponseWithTruncatedAndInvalidUTF8);
  discovery()->AddDevice(std::move(device));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(get_assertion_callback().was_called());
}

// Tests a scenario where authenticator responds without user entity in its
// response but client is expecting a resident key credential.
TEST_F(FidoGetAssertionHandlerTest, IncorrectUserEntity) {
  // Use a GetAssertion request with an empty allow list.
  auto request_handler =
      CreateGetAssertionHandlerWithRequest(CtapGetAssertionRequest(
          test_data::kRelyingPartyId, test_data::kClientDataJson));
  discovery()->WaitForCallToStartAndSimulateSuccess();
  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponse);

  discovery()->AddDevice(std::move(device));

  get_assertion_callback().WaitForCallback();
  EXPECT_EQ(GetAssertionStatus::kAuthenticatorResponseInvalid,
            get_assertion_callback().status());
}

TEST_F(FidoGetAssertionHandlerTest,
       AllTransportsAllowedIfAllowCredentialsListUndefined) {
  auto request = CreateTestRequestWithCableExtension();
  EXPECT_CALL(*mock_adapter_, IsPresent()).WillOnce(::testing::Return(true));
  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));
  ExpectAllTransportsAreAllowedForRequest(request_handler.get());
}

TEST_F(FidoGetAssertionHandlerTest,
       AllTransportsAllowedIfAllowCredentialsListIsEmpty) {
  auto request = CreateTestRequestWithCableExtension();
  EXPECT_CALL(*mock_adapter_, IsPresent()).WillOnce(::testing::Return(true));
  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));
  ExpectAllTransportsAreAllowedForRequest(request_handler.get());
}

TEST_F(FidoGetAssertionHandlerTest,
       AllTransportsAllowedIfHasAllowedCredentialWithEmptyTransportsList) {
  auto request = CreateTestRequestWithCableExtension();
  request.allow_list = {
      PublicKeyCredentialDescriptor(
          CredentialType::kPublicKey,
          fido_parsing_utils::Materialize(
              test_data::kTestGetAssertionCredentialId),
          {FidoTransportProtocol::kUsbHumanInterfaceDevice}),
      PublicKeyCredentialDescriptor(
          CredentialType::kPublicKey,
          fido_parsing_utils::Materialize(kBogusCredentialId)),
  };

  EXPECT_CALL(*mock_adapter_, IsPresent()).WillOnce(::testing::Return(true));
  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));
  ExpectAllTransportsAreAllowedForRequest(request_handler.get());
}

TEST_F(FidoGetAssertionHandlerTest,
       AllowedTransportsAreUnionOfTransportsLists) {
  auto request = CreateTestRequestWithCableExtension();
  request.allow_list = {
      PublicKeyCredentialDescriptor(
          CredentialType::kPublicKey,
          fido_parsing_utils::Materialize(
              test_data::kTestGetAssertionCredentialId),
          {FidoTransportProtocol::kUsbHumanInterfaceDevice}),
      PublicKeyCredentialDescriptor(
          CredentialType::kPublicKey,
          fido_parsing_utils::Materialize(kBogusCredentialId),
          {FidoTransportProtocol::kInternal,
           FidoTransportProtocol::kNearFieldCommunication}),
  };

  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));
  ExpectAllowedTransportsForRequestAre(
      request_handler.get(), {FidoTransportProtocol::kUsbHumanInterfaceDevice,
                              FidoTransportProtocol::kInternal,
                              FidoTransportProtocol::kNearFieldCommunication});
}

TEST_F(FidoGetAssertionHandlerTest, SupportedTransportsAreOnlyNfc) {
  const base::flat_set<FidoTransportProtocol> kNfc = {
      FidoTransportProtocol::kNearFieldCommunication,
  };

  set_supported_transports(kNfc);
  auto request_handler = CreateGetAssertionHandlerWithRequest(
      CreateTestRequestWithCableExtension());
  ExpectAllowedTransportsForRequestAre(request_handler.get(), kNfc);
}

TEST_F(FidoGetAssertionHandlerTest,
       SupportedTransportsAreOnlyCableAndInternal) {
  const base::flat_set<FidoTransportProtocol> kCableAndInternal = {
      FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy,
      FidoTransportProtocol::kInternal,
  };

  EXPECT_CALL(*mock_adapter_, IsPresent()).WillOnce(::testing::Return(true));
  set_supported_transports(kCableAndInternal);
  auto request_handler = CreateGetAssertionHandlerWithRequest(
      CreateTestRequestWithCableExtension());
  ExpectAllowedTransportsForRequestAre(request_handler.get(),
                                       kCableAndInternal);
}

TEST_F(FidoGetAssertionHandlerTest, SuccessWithOnlyUsbTransportAllowed) {
  auto request = CreateTestRequestWithCableExtension();
  request.allow_list = {
      PublicKeyCredentialDescriptor(
          CredentialType::kPublicKey,
          fido_parsing_utils::Materialize(
              test_data::kTestGetAssertionCredentialId),
          {FidoTransportProtocol::kUsbHumanInterfaceDevice}),
  };

  set_supported_transports({FidoTransportProtocol::kUsbHumanInterfaceDevice});

  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));

  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponse);
  discovery()->WaitForCallToStartAndSimulateSuccess();
  discovery()->AddDevice(std::move(device));

  get_assertion_callback().WaitForCallback();

  EXPECT_EQ(GetAssertionStatus::kSuccess, get_assertion_callback().status());
  EXPECT_TRUE(get_assertion_callback().value<0>());
  EXPECT_THAT(
      request_handler->transport_availability_info().available_transports,
      ::testing::UnorderedElementsAre(
          FidoTransportProtocol::kUsbHumanInterfaceDevice));
}

TEST_F(FidoGetAssertionHandlerTest, SuccessWithOnlyNfcTransportAllowed) {
  auto request = CreateTestRequestWithCableExtension();
  request.allow_list = {
      PublicKeyCredentialDescriptor(
          CredentialType::kPublicKey,
          fido_parsing_utils::Materialize(
              test_data::kTestGetAssertionCredentialId),
          {FidoTransportProtocol::kNearFieldCommunication}),
  };

  set_supported_transports({FidoTransportProtocol::kNearFieldCommunication});

  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));

  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->SetDeviceTransport(FidoTransportProtocol::kNearFieldCommunication);
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponse);
  nfc_discovery()->WaitForCallToStartAndSimulateSuccess();
  nfc_discovery()->AddDevice(std::move(device));

  get_assertion_callback().WaitForCallback();

  EXPECT_EQ(GetAssertionStatus::kSuccess, get_assertion_callback().status());
  EXPECT_TRUE(get_assertion_callback().value<0>());
  EXPECT_THAT(
      request_handler->transport_availability_info().available_transports,
      ::testing::UnorderedElementsAre(
          FidoTransportProtocol::kNearFieldCommunication));
}

TEST_F(FidoGetAssertionHandlerTest, SuccessWithOnlyInternalTransportAllowed) {
  auto request = CreateTestRequestWithCableExtension();
  request.allow_list = {
      PublicKeyCredentialDescriptor(
          CredentialType::kPublicKey,
          fido_parsing_utils::Materialize(
              test_data::kTestGetAssertionCredentialId),
          {FidoTransportProtocol::kInternal}),
  };

  set_supported_transports({FidoTransportProtocol::kInternal});

  auto request_handler =
      CreateGetAssertionHandlerWithRequest(std::move(request));

  auto device = MockFidoDevice::MakeCtap(
      ReadCTAPGetInfoResponse(test_data::kTestGetInfoResponsePlatformDevice));
  EXPECT_CALL(*device, GetId()).WillRepeatedly(testing::Return("device0"));
  device->SetDeviceTransport(FidoTransportProtocol::kInternal);
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetInfo,
      test_data::kTestGetInfoResponsePlatformDevice);
  device->ExpectCtap2CommandAndRespondWith(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      test_data::kTestGetAssertionResponse);
  platform_discovery()->WaitForCallToStartAndSimulateSuccess();
  platform_discovery()->AddDevice(std::move(device));

  get_assertion_callback().WaitForCallback();

  EXPECT_EQ(GetAssertionStatus::kSuccess, get_assertion_callback().status());
  EXPECT_TRUE(get_assertion_callback().value<0>());
  EXPECT_THAT(
      request_handler->transport_availability_info().available_transports,
      ::testing::UnorderedElementsAre(FidoTransportProtocol::kInternal));
}

// If a device with transport type kInternal returns a
// CTAP2_ERR_OPERATION_DENIED error, the request should complete with
// GetAssertionStatus::kUserConsentDenied. Pending authenticators should be
// cancelled.
TEST_F(FidoGetAssertionHandlerTest,
       TestRequestWithOperationDeniedErrorPlatform) {
  auto request_handler = CreateGetAssertionHandlerCtap();

  auto platform_device = MockFidoDevice::MakeCtapWithGetInfoExpectation(
      test_data::kTestGetInfoResponsePlatformDevice);
  platform_device->SetDeviceTransport(FidoTransportProtocol::kInternal);
  platform_device->ExpectCtap2CommandAndRespondWithError(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      CtapDeviceResponseCode::kCtap2ErrOperationDenied,
      base::TimeDelta::FromMicroseconds(10));
  platform_discovery()->WaitForCallToStartAndSimulateSuccess();
  platform_discovery()->AddDevice(std::move(platform_device));

  auto other_device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  other_device->ExpectCtap2CommandAndDoNotRespond(
      CtapRequestCommand::kAuthenticatorGetAssertion);
  EXPECT_CALL(*other_device, Cancel);

  discovery()->WaitForCallToStartAndSimulateSuccess();
  discovery()->AddDevice(std::move(other_device));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(get_assertion_callback().was_called());
  EXPECT_EQ(GetAssertionStatus::kUserConsentDenied,
            get_assertion_callback().status());
}

// Like |TestRequestWithOperationDeniedErrorPlatform|, but with a
// cross-platform device.
TEST_F(FidoGetAssertionHandlerTest,
       TestRequestWithOperationDeniedErrorCrossPlatform) {
  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->ExpectCtap2CommandAndRespondWithError(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      CtapDeviceResponseCode::kCtap2ErrOperationDenied);

  auto request_handler = CreateGetAssertionHandlerCtap();
  discovery()->WaitForCallToStartAndSimulateSuccess();
  discovery()->AddDevice(std::move(device));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(get_assertion_callback().was_called());
  EXPECT_EQ(GetAssertionStatus::kUserConsentDenied,
            get_assertion_callback().status());
}

// If a device returns CTAP2_ERR_PIN_AUTH_INVALID, the request should complete
// with GetAssertionStatus::kUserConsentDenied.
TEST_F(FidoGetAssertionHandlerTest, TestRequestWithPinAuthInvalid) {
  auto device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  device->ExpectCtap2CommandAndRespondWithError(
      CtapRequestCommand::kAuthenticatorGetAssertion,
      CtapDeviceResponseCode::kCtap2ErrPinAuthInvalid);

  auto request_handler = CreateGetAssertionHandlerCtap();
  discovery()->WaitForCallToStartAndSimulateSuccess();
  discovery()->AddDevice(std::move(device));

  task_environment_.FastForwardUntilNoTasksRemain();
  EXPECT_TRUE(get_assertion_callback().was_called());
  EXPECT_EQ(GetAssertionStatus::kUserConsentDenied,
            get_assertion_callback().status());
}

MATCHER_P(IsCtap2Command, expected_command, "") {
  return !arg.empty() && arg[0] == base::strict_cast<uint8_t>(expected_command);
}

TEST_F(FidoGetAssertionHandlerTest, DeviceFailsImmediately) {
  // Test that, when a device immediately returns an unexpected error, the
  // request continues and waits for another device.

  auto broken_device = MockFidoDevice::MakeCtapWithGetInfoExpectation();
  EXPECT_CALL(
      *broken_device,
      DeviceTransactPtr(
          IsCtap2Command(CtapRequestCommand::kAuthenticatorGetAssertion), _))
      .WillOnce(::testing::DoAll(
          ::testing::WithArg<1>(
              ::testing::Invoke([this](FidoDevice::DeviceCallback& callback) {
                std::vector<uint8_t> response = {static_cast<uint8_t>(
                    CtapDeviceResponseCode::kCtap2ErrInvalidCBOR)};
                base::ThreadTaskRunnerHandle::Get()->PostTask(
                    FROM_HERE,
                    base::BindOnce(std::move(callback), std::move(response)));

                auto working_device =
                    MockFidoDevice::MakeCtapWithGetInfoExpectation();
                working_device->ExpectCtap2CommandAndRespondWith(
                    CtapRequestCommand::kAuthenticatorGetAssertion,
                    test_data::kTestGetAssertionResponse);
                discovery()->AddDevice(std::move(working_device));
              })),
          ::testing::Return(0)));

  auto request_handler = CreateGetAssertionHandlerCtap();
  discovery()->WaitForCallToStartAndSimulateSuccess();
  discovery()->AddDevice(std::move(broken_device));

  get_assertion_callback().WaitForCallback();
  EXPECT_EQ(GetAssertionStatus::kSuccess, get_assertion_callback().status());
}

// Tests a scenario where authenticator of incorrect transport type was used to
// conduct CTAP GetAssertion call.
//
// TODO(engedy): This should not happen, instead |allowCredentials| should be
// filtered to only contain items compatible with the transport actually used to
// talk to the authenticator.
TEST(GetAssertionRequestHandlerTest, IncorrectTransportType) {
  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  device::test::VirtualFidoDeviceFactory virtual_device_factory;
  virtual_device_factory.SetSupportedProtocol(device::ProtocolVersion::kCtap2);
  ASSERT_TRUE(virtual_device_factory.mutable_state()->InjectRegistration(
      fido_parsing_utils::Materialize(test_data::kTestGetAssertionCredentialId),
      test_data::kRelyingPartyId));

  // Request the correct credential ID, but set a different transport hint.
  CtapGetAssertionRequest request(test_data::kRelyingPartyId,
                                  test_data::kClientDataJson);
  request.allow_list = {
      PublicKeyCredentialDescriptor(
          CredentialType::kPublicKey,
          fido_parsing_utils::Materialize(
              test_data::kTestGetAssertionCredentialId),
          {FidoTransportProtocol::kBluetoothLowEnergy}),
  };

  TestGetAssertionRequestCallback cb;
  auto request_handler = std::make_unique<GetAssertionRequestHandler>(
      &virtual_device_factory,
      base::flat_set<FidoTransportProtocol>(
          {FidoTransportProtocol::kUsbHumanInterfaceDevice}),
      std::move(request), /*allow_skipping_pin_touch=*/true, cb.callback());

  task_environment.FastForwardUntilNoTasksRemain();
  EXPECT_FALSE(cb.was_called());
}

#if defined(OS_WIN)

class TestObserver : public FidoRequestHandlerBase::Observer {
 public:
  TestObserver() {}
  ~TestObserver() override {}

  void set_controls_dispatch(bool controls_dispatch) {
    controls_dispatch_ = controls_dispatch;
  }

 private:
  // FidoRequestHandlerBase::Observer:
  void OnTransportAvailabilityEnumerated(
      FidoRequestHandlerBase::TransportAvailabilityInfo data) override {}
  bool EmbedderControlsAuthenticatorDispatch(
      const FidoAuthenticator&) override {
    return controls_dispatch_;
  }
  void BluetoothAdapterPowerChanged(bool is_powered_on) override {}
  void FidoAuthenticatorAdded(const FidoAuthenticator& authenticator) override {
  }
  void FidoAuthenticatorRemoved(base::StringPiece device_id) override {}
  bool SupportsPIN() const override { return false; }
  void CollectPIN(
      base::Optional<int> attempts,
      base::OnceCallback<void(std::string)> provide_pin_cb) override {
    NOTREACHED();
  }
  void StartBioEnrollment(base::OnceClosure next_callback) override {}
  void OnSampleCollected(int bio_samples_remaining) override {}
  void FinishCollectToken() override { NOTREACHED(); }
  void OnRetryUserVerification(int attempts) override {}
  void OnInternalUserVerificationLocked() override {}
  void SetMightCreateResidentCredential(bool v) override {}

  bool controls_dispatch_ = false;
};

// Verify that the request handler instantiates a HID device backed
// FidoDeviceAuthenticator or a WinNativeCrossPlatformAuthenticator, depending
// on API availability.
TEST(GetAssertionRequestHandlerWinTest, TestWinUsbDiscovery) {
  base::test::TaskEnvironment task_environment;
  for (const bool enable_api : {false, true}) {
    SCOPED_TRACE(::testing::Message() << "enable_api=" << enable_api);
    FakeWinWebAuthnApi api;
    api.set_available(enable_api);

    // Simulate a connected HID device.
    ScopedFakeFidoHidManager fake_hid_manager;
    fake_hid_manager.AddFidoHidDevice("guid");

    TestGetAssertionRequestCallback cb;
    FidoDiscoveryFactory fido_discovery_factory;
    fido_discovery_factory.set_win_webauthn_api(&api);
    auto handler = std::make_unique<GetAssertionRequestHandler>(
        &fido_discovery_factory,
        base::flat_set<FidoTransportProtocol>(
            {FidoTransportProtocol::kUsbHumanInterfaceDevice}),
        CtapGetAssertionRequest(test_data::kRelyingPartyId,
                                test_data::kClientDataJson),
        /*allow_skipping_pin_touch=*/true, cb.callback());
    // Register an observer that disables automatic dispatch. Dispatch to the
    // (unimplemented) fake Windows API would immediately result in an invalid
    // response.
    TestObserver observer;
    observer.set_controls_dispatch(true);
    handler->set_observer(&observer);

    task_environment.RunUntilIdle();

    ASSERT_FALSE(cb.was_called());
    EXPECT_EQ(handler->AuthenticatorsForTesting().size(), 1u);
    EXPECT_EQ(handler->AuthenticatorsForTesting()
                  .begin()
                  ->second->authenticator->IsWinNativeApiAuthenticator(),
              enable_api);
  }
}

#endif  // defined(OS_WIN)

}  // namespace device
