// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/authenticator_impl.h"

#include <algorithm>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/cxx17_backports.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "components/webauthn/content/browser/internal_authenticator_impl.h"
#include "content/browser/webauth/authenticator_common.h"
#include "content/browser/webauth/authenticator_environment_impl.h"
#include "content/browser/webauth/client_data_json.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "content/test/test_render_frame_host.h"
#include "crypto/sha2.h"
#include "device/base/features.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/test/mock_bluetooth_adapter.h"
#include "device/fido/attested_credential_data.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/cable/v2_authenticator.h"
#include "device/fido/cable/v2_constants.h"
#include "device/fido/cable/v2_discovery.h"
#include "device/fido/cable/v2_handshake.h"
#include "device/fido/cable/v2_test_util.h"
#include "device/fido/fake_fido_discovery.h"
#include "device/fido/features.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/fido_types.h"
#include "device/fido/filter.h"
#include "device/fido/hid/fake_hid_impl_for_testing.h"
#include "device/fido/large_blob.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/multiple_virtual_fido_device_factory.h"
#include "device/fido/pin.h"
#include "device/fido/public_key.h"
#include "device/fido/test_callback_receiver.h"
#include "device/fido/virtual_ctap2_device.h"
#include "device/fido/virtual_fido_device.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-shared.h"
#include "third_party/boringssl/src/include/openssl/bytestring.h"
#include "third_party/boringssl/src/include/openssl/ec_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/obj.h"
#include "third_party/zlib/google/compression_utils.h"
#include "url/url_util.h"

#if defined(OS_MAC)
#include "device/fido/mac/authenticator_config.h"
#include "device/fido/mac/scoped_touch_id_test_environment.h"
#endif

#if defined(OS_WIN)
#include "device/fido/win/fake_webauthn_api.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/dbus/tpm_manager/tpm_manager_client.h"
#include "chromeos/dbus/u2f/u2f_client.h"
#endif

namespace content {

using ::testing::_;

using blink::mojom::AttestationConveyancePreference;
using blink::mojom::AuthenticatorSelectionCriteria;
using blink::mojom::AuthenticatorSelectionCriteriaPtr;
using blink::mojom::AuthenticatorStatus;
using blink::mojom::AuthenticatorTransport;
using blink::mojom::CableAuthentication;
using blink::mojom::CableAuthenticationPtr;
using blink::mojom::GetAssertionAuthenticatorResponsePtr;
using blink::mojom::MakeCredentialAuthenticatorResponsePtr;
using blink::mojom::PublicKeyCredentialCreationOptions;
using blink::mojom::PublicKeyCredentialCreationOptionsPtr;
using blink::mojom::PublicKeyCredentialDescriptor;
using blink::mojom::PublicKeyCredentialDescriptorPtr;
using blink::mojom::PublicKeyCredentialParameters;
using blink::mojom::PublicKeyCredentialParametersPtr;
using blink::mojom::PublicKeyCredentialRequestOptions;
using blink::mojom::PublicKeyCredentialRequestOptionsPtr;
using blink::mojom::PublicKeyCredentialRpEntity;
using blink::mojom::PublicKeyCredentialRpEntityPtr;
using blink::mojom::PublicKeyCredentialType;
using blink::mojom::PublicKeyCredentialUserEntity;
using blink::mojom::PublicKeyCredentialUserEntityPtr;
using cbor::Reader;
using cbor::Value;
using device::VirtualCtap2Device;
using device::VirtualFidoDevice;

namespace {

using InterestingFailureReason =
    AuthenticatorRequestClientDelegate::InterestingFailureReason;
using FailureReasonCallbackReceiver =
    ::device::test::TestCallbackReceiver<InterestingFailureReason>;

constexpr base::TimeDelta kTestTimeout = base::TimeDelta::FromMinutes(1);

// The size of credential IDs returned by GetTestCredentials().
constexpr size_t kTestCredentialIdLength = 32u;

constexpr char kTestOrigin1[] = "https://a.google.com";
constexpr char kTestOrigin2[] = "https://acme.org";
constexpr char kTestRelyingPartyId[] = "google.com";
constexpr char kCryptotokenOrigin[] =
    "chrome-extension://kmendfapggjehodndflmmgagdbamhnfd";
constexpr char kTestExtensionOrigin[] =
    "chrome-extension://abcdefghijklmnopqrstuvwxyzabcdef";

constexpr uint8_t kTestChallengeBytes[] = {
    0x68, 0x71, 0x34, 0x96, 0x82, 0x22, 0xEC, 0x17, 0x20, 0x2E, 0x42,
    0x50, 0x5F, 0x8E, 0xD2, 0xB1, 0x6A, 0xE2, 0x2F, 0x16, 0xBB, 0x05,
    0xB8, 0x8C, 0x25, 0xDB, 0x9E, 0x60, 0x26, 0x45, 0xF1, 0x41};

constexpr char kTestRegisterClientDataJsonString[] =
    R"({"challenge":"aHE0loIi7BcgLkJQX47SsWriLxa7BbiMJdueYCZF8UE","origin":)"
    R"("https://a.google.com", "type":"webauthn.create"})";

constexpr char kTestSignClientDataJsonString[] =
    R"({"challenge":"aHE0loIi7BcgLkJQX47SsWriLxa7BbiMJdueYCZF8UE","origin":)"
    R"("https://a.google.com", "type":"webauthn.get"})";

typedef struct {
  const char* origin;
  // Either a relying party ID or a U2F AppID.
  const char* claimed_authority;
  AuthenticatorStatus expected_status;
} OriginClaimedAuthorityPair;

constexpr OriginClaimedAuthorityPair kValidRelyingPartyTestCases[] = {
    {"http://localhost", "localhost", AuthenticatorStatus::SUCCESS},
    {"https://myawesomedomain", "myawesomedomain",
     AuthenticatorStatus::SUCCESS},
    {"https://foo.bar.google.com", "foo.bar.google.com",
     AuthenticatorStatus::SUCCESS},
    {"https://foo.bar.google.com", "bar.google.com",
     AuthenticatorStatus::SUCCESS},
    {"https://foo.bar.google.com", "google.com", AuthenticatorStatus::SUCCESS},
    {"https://earth.login.awesomecompany", "login.awesomecompany",
     AuthenticatorStatus::SUCCESS},
    {"https://google.com:1337", "google.com", AuthenticatorStatus::SUCCESS},

    // Hosts with trailing dot valid for rpIds with or without trailing dot.
    // Hosts without trailing dots only matches rpIDs without trailing dot.
    // Two trailing dots only matches rpIDs with two trailing dots.
    {"https://google.com.", "google.com", AuthenticatorStatus::SUCCESS},
    {"https://google.com.", "google.com.", AuthenticatorStatus::SUCCESS},
    {"https://google.com..", "google.com..", AuthenticatorStatus::SUCCESS},

    // Leading dots are ignored in canonicalized hosts.
    {"https://.google.com", "google.com", AuthenticatorStatus::SUCCESS},
    {"https://..google.com", "google.com", AuthenticatorStatus::SUCCESS},
    {"https://.google.com", ".google.com", AuthenticatorStatus::SUCCESS},
    {"https://..google.com", ".google.com", AuthenticatorStatus::SUCCESS},
    {"https://accounts.google.com", ".google.com",
     AuthenticatorStatus::SUCCESS},
};

constexpr OriginClaimedAuthorityPair kInvalidRelyingPartyTestCases[] = {
    {"https://google.com", "com", AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"http://google.com", "google.com", AuthenticatorStatus::INVALID_DOMAIN},
    {"http://myawesomedomain", "myawesomedomain",
     AuthenticatorStatus::INVALID_DOMAIN},
    {"https://google.com", "foo.bar.google.com",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"http://myawesomedomain", "randomdomain",
     AuthenticatorStatus::INVALID_DOMAIN},
    {"https://myawesomedomain", "randomdomain",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://notgoogle.com", "google.com)",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://not-google.com", "google.com)",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://evil.appspot.com", "appspot.com",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://evil.co.uk", "co.uk", AuthenticatorStatus::BAD_RELYING_PARTY_ID},

    {"https://google.com", "google.com.",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://google.com", "google.com..",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://google.com", ".google.com",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://google.com..", "google.com",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://.com", "com.", AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://.co.uk", "co.uk.", AuthenticatorStatus::BAD_RELYING_PARTY_ID},

    {"https://1.2.3", "1.2.3", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://1.2.3", "2.3", AuthenticatorStatus::INVALID_DOMAIN},

    {"https://127.0.0.1", "127.0.0.1", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://127.0.0.1", "27.0.0.1", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://127.0.0.1", ".0.0.1", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://127.0.0.1", "0.0.1", AuthenticatorStatus::INVALID_DOMAIN},

    {"https://[::127.0.0.1]", "127.0.0.1", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://[::127.0.0.1]", "[127.0.0.1]",
     AuthenticatorStatus::INVALID_DOMAIN},

    {"https://[::1]", "1", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://[::1]", "1]", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://[::1]", "::1", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://[::1]", "[::1]", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://[1::1]", "::1", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://[1::1]", "::1]", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://[1::1]", "[::1]", AuthenticatorStatus::INVALID_DOMAIN},

    {"http://google.com:443", "google.com",
     AuthenticatorStatus::INVALID_DOMAIN},
    {"data:google.com", "google.com", AuthenticatorStatus::OPAQUE_DOMAIN},
    {"data:text/html,google.com", "google.com",
     AuthenticatorStatus::OPAQUE_DOMAIN},
    {"ws://google.com", "google.com", AuthenticatorStatus::INVALID_DOMAIN},
    {"gopher://google.com", "google.com", AuthenticatorStatus::OPAQUE_DOMAIN},
    {"ftp://google.com", "google.com", AuthenticatorStatus::INVALID_DOMAIN},
    {"file:///google.com", "google.com", AuthenticatorStatus::INVALID_PROTOCOL},
    // Use of webauthn from a WSS origin may be technically valid, but we
    // prohibit use on non-HTTPS origins. (At least for now.)
    {"wss://google.com", "google.com", AuthenticatorStatus::INVALID_PROTOCOL},

    {"data:,", "", AuthenticatorStatus::OPAQUE_DOMAIN},
    {"https://google.com", "", AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"ws:///google.com", "", AuthenticatorStatus::INVALID_DOMAIN},
    {"wss:///google.com", "", AuthenticatorStatus::INVALID_PROTOCOL},
    {"gopher://google.com", "", AuthenticatorStatus::OPAQUE_DOMAIN},
    {"ftp://google.com", "", AuthenticatorStatus::INVALID_DOMAIN},
    {"file:///google.com", "", AuthenticatorStatus::INVALID_PROTOCOL},

    // This case is acceptable according to spec, but both renderer
    // and browser handling currently do not permit it.
    {"https://login.awesomecompany", "awesomecompany",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},

    // These are AppID test cases, but should also be invalid relying party
    // examples too.
    {"https://example.com", "https://com/",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://example.com", "https://com/foo",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://example.com", "https://foo.com/",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://example.com", "http://example.com",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"http://example.com", "https://example.com",
     AuthenticatorStatus::INVALID_DOMAIN},
    {"https://127.0.0.1", "https://127.0.0.1",
     AuthenticatorStatus::INVALID_DOMAIN},
    {"https://www.notgoogle.com",
     "https://www.gstatic.com/securitykey/origins.json",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://www.google.com",
     "https://www.gstatic.com/securitykey/origins.json#x",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://www.google.com",
     "https://www.gstatic.com/securitykey/origins.json2",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://www.google.com", "https://gstatic.com/securitykey/origins.json",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://ggoogle.com", "https://www.gstatic.com/securitykey/origi",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://com", "https://www.gstatic.com/securitykey/origins.json",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
};

using TestIsUvpaaCallback = device::test::ValueCallbackReceiver<bool>;
using TestMakeCredentialCallback = device::test::StatusAndValueCallbackReceiver<
    AuthenticatorStatus,
    MakeCredentialAuthenticatorResponsePtr>;
using TestGetAssertionCallback = device::test::StatusAndValueCallbackReceiver<
    AuthenticatorStatus,
    GetAssertionAuthenticatorResponsePtr>;
using TestRequestStartedCallback = device::test::TestCallbackReceiver<>;

std::vector<uint8_t> GetTestChallengeBytes() {
  return std::vector<uint8_t>(std::begin(kTestChallengeBytes),
                              std::end(kTestChallengeBytes));
}

device::PublicKeyCredentialRpEntity GetTestPublicKeyCredentialRPEntity() {
  device::PublicKeyCredentialRpEntity entity;
  entity.id = std::string(kTestRelyingPartyId);
  entity.name = "TestRP@example.com";
  return entity;
}

device::PublicKeyCredentialUserEntity GetTestPublicKeyCredentialUserEntity() {
  device::PublicKeyCredentialUserEntity entity;
  entity.display_name = "User A. Name";
  std::vector<uint8_t> id(32, 0x0A);
  entity.id = id;
  entity.name = "username@example.com";
  entity.icon_url = GURL("https://gstatic.com/fakeurl2.png");
  return entity;
}

std::vector<device::PublicKeyCredentialParams::CredentialInfo>
GetTestPublicKeyCredentialParameters(int32_t algorithm_identifier) {
  std::vector<device::PublicKeyCredentialParams::CredentialInfo> parameters;
  device::PublicKeyCredentialParams::CredentialInfo fake_parameter;
  fake_parameter.type = device::CredentialType::kPublicKey;
  fake_parameter.algorithm = algorithm_identifier;
  parameters.push_back(std::move(fake_parameter));
  return parameters;
}

device::AuthenticatorSelectionCriteria GetTestAuthenticatorSelectionCriteria() {
  return device::AuthenticatorSelectionCriteria(
      device::AuthenticatorAttachment::kAny,
      device::ResidentKeyRequirement::kDiscouraged,
      device::UserVerificationRequirement::kPreferred);
}

std::vector<device::PublicKeyCredentialDescriptor> GetTestCredentials(
    size_t num_credentials = 1) {
  std::vector<device::PublicKeyCredentialDescriptor> descriptors;
  for (size_t i = 0; i < num_credentials; i++) {
    DCHECK(i <= std::numeric_limits<uint8_t>::max());
    std::vector<uint8_t> id(kTestCredentialIdLength, static_cast<uint8_t>(i));
    base::flat_set<device::FidoTransportProtocol> transports{
        device::FidoTransportProtocol::kUsbHumanInterfaceDevice,
        device::FidoTransportProtocol::kBluetoothLowEnergy};
    descriptors.emplace_back(device::CredentialType::kPublicKey, std::move(id),
                             std::move(transports));
  }
  return descriptors;
}

PublicKeyCredentialCreationOptionsPtr
GetTestPublicKeyCredentialCreationOptions() {
  auto options = PublicKeyCredentialCreationOptions::New();
  options->relying_party = GetTestPublicKeyCredentialRPEntity();
  options->user = GetTestPublicKeyCredentialUserEntity();
  options->public_key_parameters = GetTestPublicKeyCredentialParameters(
      static_cast<int32_t>(device::CoseAlgorithmIdentifier::kEs256));
  options->challenge.assign(32, 0x0A);
  options->timeout = base::TimeDelta::FromMinutes(1);
  options->authenticator_selection = GetTestAuthenticatorSelectionCriteria();
  return options;
}

PublicKeyCredentialRequestOptionsPtr
GetTestPublicKeyCredentialRequestOptions() {
  auto options = PublicKeyCredentialRequestOptions::New();
  options->relying_party_id = std::string(kTestRelyingPartyId);
  options->challenge.assign(32, 0x0A);
  options->timeout = base::TimeDelta::FromMinutes(1);
  options->user_verification = device::UserVerificationRequirement::kPreferred;
  options->allow_credentials = GetTestCredentials();
  return options;
}

std::vector<device::CableDiscoveryData> GetTestCableExtension() {
  device::CableDiscoveryData cable;
  cable.version = device::CableDiscoveryData::Version::V1;
  cable.v1.emplace();
  cable.v1->client_eid.fill(0x01);
  cable.v1->authenticator_eid.fill(0x02);
  cable.v1->session_pre_key.fill(0x03);

  std::vector<device::CableDiscoveryData> ret;
  ret.emplace_back(std::move(cable));
  return ret;
}

device::AuthenticatorData AuthDataFromMakeCredentialResponse(
    const MakeCredentialAuthenticatorResponsePtr& response) {
  absl::optional<Value> attestation_value =
      Reader::Read(response->attestation_object);
  CHECK(attestation_value);
  const auto& attestation = attestation_value->GetMap();

  const auto auth_data_it = attestation.find(Value(device::kAuthDataKey));
  CHECK(auth_data_it != attestation.end());
  const std::vector<uint8_t>& auth_data = auth_data_it->second.GetBytestring();
  absl::optional<device::AuthenticatorData> parsed_auth_data =
      device::AuthenticatorData::DecodeAuthenticatorData(auth_data);
  return std::move(parsed_auth_data.value());
}

url::Origin GetTestOrigin() {
  const GURL test_relying_party_url(kTestOrigin1);
  CHECK(test_relying_party_url.is_valid());
  return url::Origin::Create(test_relying_party_url);
}

std::string GetTestClientDataJSON(ClientDataRequestType type) {
  return BuildClientDataJson(std::move(type), GetTestOrigin().Serialize(),
                             GetTestChallengeBytes(),
                             /*is_cross_origin=*/false);
}

std::vector<uint8_t> StringToVector(const std::string& string) {
  std::vector<uint8_t> vector;
  vector.assign(string.begin(), string.end());
  return vector;
}

std::vector<uint8_t> CompressLargeBlob(base::span<const uint8_t> blob) {
  std::string output;
  CHECK(compression::GzipCompress(blob, &output));
  return StringToVector(output);
}

std::vector<uint8_t> UncompressLargeBlob(base::span<const uint8_t> blob) {
  std::string output;
  CHECK(compression::GzipUncompress(blob, &output));
  return StringToVector(output);
}

// Convert a blink::mojom::AttestationConveyancePreference to a
// device::AtttestationConveyancePreference.
device::AttestationConveyancePreference ConvertAttestationConveyancePreference(
    AttestationConveyancePreference in) {
  switch (in) {
    case AttestationConveyancePreference::NONE:
      return ::device::AttestationConveyancePreference::kNone;
    case AttestationConveyancePreference::INDIRECT:
      return ::device::AttestationConveyancePreference::kIndirect;
    case AttestationConveyancePreference::DIRECT:
      return ::device::AttestationConveyancePreference::kDirect;
    case AttestationConveyancePreference::ENTERPRISE:
      return ::device::AttestationConveyancePreference::
          kEnterpriseIfRPListedOnAuthenticator;
  }
}

}  // namespace

class AuthenticatorTestBase : public RenderViewHostTestHarness {
 protected:
  AuthenticatorTestBase()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~AuthenticatorTestBase() override = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

#if BUILDFLAG(IS_CHROMEOS_ASH)
    chromeos::TpmManagerClient::InitializeFake();
    chromeos::U2FClient::InitializeFake();
#endif

#if defined(OS_WIN)
    // Disable the Windows WebAuthn API integration by default. Individual tests
    // can modify this.
    fake_win_webauthn_api_.set_available(false);
    AuthenticatorEnvironmentImpl::GetInstance()->SetWinWebAuthnApiForTesting(
        &fake_win_webauthn_api_);
#endif

    ResetVirtualDevice();
  }

  void TearDown() override {
    RenderViewHostTestHarness::TearDown();

#if BUILDFLAG(IS_CHROMEOS_ASH)
    chromeos::U2FClient::Shutdown();
    chromeos::TpmManagerClient::Shutdown();
#endif
#if defined(OS_WIN)
    AuthenticatorEnvironmentImpl::GetInstance()
        ->ClearWinWebAuthnApiForTesting();
#endif
  }

  void ResetVirtualDevice() {
    auto virtual_device_factory =
        std::make_unique<device::test::VirtualFidoDeviceFactory>();
    virtual_device_factory_ = virtual_device_factory.get();
    AuthenticatorEnvironmentImpl::GetInstance()
        ->ReplaceDefaultDiscoveryFactoryForTesting(
            std::move(virtual_device_factory));
#if defined(OS_WIN)
    virtual_device_factory_->set_win_webauthn_api(&fake_win_webauthn_api_);
#endif
  }

  device::test::VirtualFidoDeviceFactory* virtual_device_factory_;
#if defined(OS_WIN)
  device::FakeWinWebAuthnApi fake_win_webauthn_api_;
#endif
};

class AuthenticatorImplTest : public AuthenticatorTestBase {
 protected:
  AuthenticatorImplTest() {
    url::AddStandardScheme("chrome-extension", url::SCHEME_WITH_HOST);
  }
  ~AuthenticatorImplTest() override = default;

  void SetUp() override {
    AuthenticatorTestBase::SetUp();
    bluetooth_global_values_->SetLESupported(true);
    device::BluetoothAdapterFactory::SetAdapterForTesting(mock_adapter_);
  }

  void NavigateAndCommit(const GURL& url) {
    RenderViewHostTestHarness::NavigateAndCommit(url);
  }

  mojo::Remote<blink::mojom::Authenticator> ConnectToAuthenticator() {
    mojo::Remote<blink::mojom::Authenticator> authenticator;
    static_cast<RenderFrameHostImpl*>(main_rfh())
        ->GetWebAuthenticationService(
            authenticator.BindNewPipeAndPassReceiver());
    return authenticator;
  }

  struct MakeCredentialResult {
    AuthenticatorStatus status;
    MakeCredentialAuthenticatorResponsePtr response;
  };

  MakeCredentialResult AuthenticatorMakeCredential() {
    return AuthenticatorMakeCredential(
        GetTestPublicKeyCredentialCreationOptions());
  }

  MakeCredentialResult AuthenticatorMakeCredential(
      PublicKeyCredentialCreationOptionsPtr options) {
    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();
    TestMakeCredentialCallback callback_receiver;
    authenticator->MakeCredential(std::move(options),
                                  callback_receiver.callback());
    callback_receiver.WaitForCallback();
    AuthenticatorStatus status;
    MakeCredentialAuthenticatorResponsePtr response;
    std::tie(status, response) = callback_receiver.TakeResult();
    return {status, std::move(response)};
  }

  MakeCredentialResult AuthenticatorMakeCredentialAndWaitForTimeout(
      PublicKeyCredentialCreationOptionsPtr options) {
    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();
    TestMakeCredentialCallback callback_receiver;
    authenticator->MakeCredential(std::move(options),
                                  callback_receiver.callback());
    task_environment()->FastForwardBy(kTestTimeout);
    callback_receiver.WaitForCallback();
    AuthenticatorStatus status;
    MakeCredentialAuthenticatorResponsePtr response;
    std::tie(status, response) = callback_receiver.TakeResult();
    return {status, std::move(response)};
  }

  struct GetAssertionResult {
    AuthenticatorStatus status;
    GetAssertionAuthenticatorResponsePtr response;
  };

  GetAssertionResult AuthenticatorGetAssertion() {
    return AuthenticatorGetAssertion(
        GetTestPublicKeyCredentialRequestOptions());
  }

  GetAssertionResult AuthenticatorGetAssertion(
      PublicKeyCredentialRequestOptionsPtr options) {
    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();
    TestGetAssertionCallback callback_receiver;
    authenticator->GetAssertion(std::move(options),
                                callback_receiver.callback());
    callback_receiver.WaitForCallback();
    AuthenticatorStatus status;
    GetAssertionAuthenticatorResponsePtr response;
    std::tie(status, response) = callback_receiver.TakeResult();
    return {status, std::move(response)};
  }

  GetAssertionResult AuthenticatorGetAssertionAndWaitForTimeout(
      PublicKeyCredentialRequestOptionsPtr options) {
    mojo::Remote<blink::mojom::Authenticator> authenticator =
        ConnectToAuthenticator();
    TestGetAssertionCallback callback_receiver;
    authenticator->GetAssertion(std::move(options),
                                callback_receiver.callback());
    task_environment()->FastForwardBy(kTestTimeout);
    AuthenticatorStatus status;
    GetAssertionAuthenticatorResponsePtr response;
    std::tie(status, response) = callback_receiver.TakeResult();
    return {status, std::move(response)};
  }

  AuthenticatorStatus TryAuthenticationWithAppId(const std::string& origin,
                                                 const std::string& appid) {
    const GURL origin_url(origin);
    NavigateAndCommit(origin_url);

    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->relying_party_id = origin_url.host();
    options->appid = appid;

    return AuthenticatorGetAssertion(std::move(options)).status;
  }

  AuthenticatorStatus TryRegistrationWithAppIdExclude(
      const std::string& origin,
      const std::string& appid_exclude) {
    const GURL origin_url(origin);
    NavigateAndCommit(origin_url);

    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->relying_party.id = origin_url.host();
    options->appid_exclude = appid_exclude;

    return AuthenticatorMakeCredential(std::move(options)).status;
  }

  void EnableFeature(const base::Feature& feature) {
    scoped_feature_list_.emplace();
    scoped_feature_list_->InitAndEnableFeature(feature);
  }

  void DisableFeature(const base::Feature& feature) {
    scoped_feature_list_.emplace();
    scoped_feature_list_->InitAndDisableFeature(feature);
  }

 protected:
  absl::optional<base::test::ScopedFeatureList> scoped_feature_list_;
  std::unique_ptr<device::BluetoothAdapterFactory::GlobalValuesForTesting>
      bluetooth_global_values_ =
          device::BluetoothAdapterFactory::Get()->InitGlobalValuesForTesting();
  scoped_refptr<::testing::NiceMock<device::MockBluetoothAdapter>>
      mock_adapter_ = base::MakeRefCounted<
          ::testing::NiceMock<device::MockBluetoothAdapter>>();
  data_decoder::test::InProcessDataDecoder data_decoder_service_;

 private:
  url::ScopedSchemeRegistryForTests scoped_registry_;
};

TEST_F(AuthenticatorImplTest, ClientDataJSONSerialization) {
  // First test that the output is in the expected form. Some verifiers may be
  // depending on the exact JSON serialisation. Since the serialisation may add
  // extra elements, this can only test that the expected value is a prefix of
  // the returned value.
  std::vector<uint8_t> challenge_bytes = {1, 2, 3};
  EXPECT_EQ(
      BuildClientDataJson(ClientDataRequestType::kWebAuthnCreate, "ori\"gin",
                          challenge_bytes, false)
          .find(
              "{\"type\":\"webauthn.create\",\"challenge\":\"AQID\",\"origin\":"
              "\"ori\\\"gin\",\"crossOrigin\":false"),
      0u);

  // Second, check that a generic JSON parser correctly parses the result.
  static const struct {
    const ClientDataRequestType type;
    const char* origin;
    std::vector<uint8_t> challenge;
    bool is_cross_origin;
  } kTestCases[] = {
      {
          ClientDataRequestType::kWebAuthnGet,
          "origin",
          {1, 2, 3},
          false,
      },
      {
          ClientDataRequestType::kU2fRegister,
          "ori\"gin",
          {1, 2, 3, 4},
          true,
      },
      {
          ClientDataRequestType::kU2fSign,
          "\x01\x02\x03\x04{}\x05c",
          {1, 2, 3, 4, 5},
          true,
      },
      {
          ClientDataRequestType::kPaymentGet,
          "origin",
          {1, 2, 3},
          false,
      },
  };

  size_t num = 0;
  for (const auto& test : kTestCases) {
    SCOPED_TRACE(num++);

    const std::string json = BuildClientDataJson(
        test.type, test.origin, test.challenge, test.is_cross_origin);

    const auto parsed = base::JSONReader::Read(json);
    ASSERT_TRUE(parsed.has_value());
    std::string type_key;
    std::string expected_type;
    switch (test.type) {
      case ClientDataRequestType::kU2fRegister:
        type_key = "typ";
        expected_type = "navigator.id.finishEnrollment";
        break;
      case ClientDataRequestType::kU2fSign:
        type_key = "typ";
        expected_type = "navigator.id.getAssertion";
        break;
      case ClientDataRequestType::kWebAuthnCreate:
        type_key = "type";
        expected_type = "webauthn.create";
        break;
      case ClientDataRequestType::kWebAuthnGet:
        type_key = "type";
        expected_type = "webauthn.get";
        break;
      case ClientDataRequestType::kPaymentGet:
        type_key = "type";
        expected_type = "payment.get";
        break;
    }

    EXPECT_EQ(*parsed->FindStringKey(type_key), expected_type);
    EXPECT_EQ(*parsed->FindStringKey("origin"), test.origin);
    std::string expected_challenge;
    base::Base64UrlEncode(
        base::StringPiece(reinterpret_cast<const char*>(test.challenge.data()),
                          test.challenge.size()),
        base::Base64UrlEncodePolicy::OMIT_PADDING, &expected_challenge);
    EXPECT_EQ(*parsed->FindStringKey("challenge"), expected_challenge);
    EXPECT_EQ(*parsed->FindBoolKey("crossOrigin"), test.is_cross_origin);
  }
}

// Verify behavior for various combinations of origins and RP IDs.
TEST_F(AuthenticatorImplTest, MakeCredentialOriginAndRpIds) {
  std::vector<OriginClaimedAuthorityPair> tests(
      &kValidRelyingPartyTestCases[0],
      &kValidRelyingPartyTestCases[base::size(kValidRelyingPartyTestCases)]);
  tests.insert(tests.end(), &kInvalidRelyingPartyTestCases[0],
               &kInvalidRelyingPartyTestCases[base::size(
                   kInvalidRelyingPartyTestCases)]);

  for (const auto& test_case : tests) {
    SCOPED_TRACE(std::string(test_case.claimed_authority) + " " +
                 std::string(test_case.origin));

    NavigateAndCommit(GURL(test_case.origin));
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->relying_party.id = test_case.claimed_authority;

    EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
              test_case.expected_status);
  }
}

// Test that MakeCredential returns INVALID_ICON_URL in the correct cases.
TEST_F(AuthenticatorImplTest, MakeCredentialURLs) {
  constexpr auto ok = AuthenticatorStatus::SUCCESS;
  constexpr auto bad = AuthenticatorStatus::INVALID_ICON_URL;
  const std::pair<GURL, AuthenticatorStatus> kTestCases[] = {
      {GURL(), ok},
      {GURL("https://secure-origin.com/kitten.png"), ok},
      {GURL("about:blank"), ok},
      {GURL("about:srcdoc"), ok},
      {GURL(
           "data:image/"
           "png;base64,"
           "iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAAACXBIWXMAAC4jAAAuIwF"
           "4pT92AAAAB3RJTUUH4wYUETEs5V5U8gAAABl0RVh0Q29tbWVudABDcmVhdGVkIHdpdG"
           "ggR0lNUFeBDhcAAABGSURBVCjPY/z//"
           "z8DKYAJmcPYyICHi0UDyTYMDg2MFIUSnsAZAp5mbGT4X49DBcxLEAUsBMxrRCiFABb8"
           "gYNpLTXiAT8AAEeHFZvhj9g8AAAAAElFTkSuQmCC"),
       ok},

      {GURL("http://insecure-origin.com/kitten.png"), bad},
      {GURL("invalid:/url"), bad},
  };

  NavigateAndCommit(GURL(kTestOrigin1));

  for (const bool test_user_icon : {false, true}) {
    for (auto test_case : kTestCases) {
      SCOPED_TRACE(test_case.first.possibly_invalid_spec());
      SCOPED_TRACE(test_user_icon);

      PublicKeyCredentialCreationOptionsPtr options =
          GetTestPublicKeyCredentialCreationOptions();
      if (test_user_icon) {
        options->user.icon_url = test_case.first;
      } else {
        options->relying_party.icon_url = test_case.first;
      }

      EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
                test_case.second);
    }
  }
}

// Test that MakeCredential request times out with NOT_ALLOWED_ERROR if user
// verification is required for U2F devices.
TEST_F(AuthenticatorImplTest, MakeCredentialUserVerification) {
  NavigateAndCommit(GURL(kTestOrigin1));

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->authenticator_selection->SetUserVerificationRequirementForTesting(
      device::UserVerificationRequirement::kRequired);

  EXPECT_EQ(
      AuthenticatorMakeCredentialAndWaitForTimeout(std::move(options)).status,
      AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

TEST_F(AuthenticatorImplTest, MakeCredentialResidentKeyUnsupported) {
  NavigateAndCommit(GURL(kTestOrigin1));

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->authenticator_selection->SetResidentKeyForTesting(
      device::ResidentKeyRequirement::kRequired);

  EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
            AuthenticatorStatus::RESIDENT_CREDENTIALS_UNSUPPORTED);
}

// Test that MakeCredential request times out with NOT_ALLOWED_ERROR if a
// platform authenticator is requested for U2F devices.
TEST_F(AuthenticatorImplTest, MakeCredentialPlatformAuthenticator) {
  NavigateAndCommit(GURL(kTestOrigin1));

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->authenticator_selection->SetAuthenticatorAttachmentForTesting(
      device::AuthenticatorAttachment::kPlatform);

  EXPECT_EQ(
      AuthenticatorMakeCredentialAndWaitForTimeout(std::move(options)).status,
      AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

// Parses its arguments as JSON and expects that all the keys in the first are
// also in the second, and with the same value.
void CheckJSONIsSubsetOfJSON(base::StringPiece subset_str,
                             base::StringPiece test_str) {
  std::unique_ptr<base::Value> subset(
      base::JSONReader::ReadDeprecated(subset_str));
  ASSERT_TRUE(subset);
  ASSERT_TRUE(subset->is_dict());
  std::unique_ptr<base::Value> test(base::JSONReader::ReadDeprecated(test_str));
  ASSERT_TRUE(test);
  ASSERT_TRUE(test->is_dict());

  for (auto item : subset->DictItems()) {
    base::Value* test_value = test->FindKey(item.first);
    if (test_value == nullptr) {
      ADD_FAILURE() << item.first << " does not exist in the test dictionary";
      continue;
    }

    if (!item.second.Equals(test_value)) {
      std::string want, got;
      ASSERT_TRUE(base::JSONWriter::Write(item.second, &want));
      ASSERT_TRUE(base::JSONWriter::Write(*test_value, &got));
      ADD_FAILURE() << "Value of " << item.first << " is unequal: want " << want
                    << " got " << got;
    }
  }
}

// Test that client data serializes to JSON properly.
TEST(ClientDataSerializationTest, Register) {
  CheckJSONIsSubsetOfJSON(
      kTestRegisterClientDataJsonString,
      GetTestClientDataJSON(ClientDataRequestType::kWebAuthnCreate));
}

TEST(ClientDataSerializationTest, Sign) {
  CheckJSONIsSubsetOfJSON(
      kTestSignClientDataJsonString,
      GetTestClientDataJSON(ClientDataRequestType::kWebAuthnGet));
}

TEST_F(AuthenticatorImplTest, TestMakeCredentialTimeout) {
  // Don't provide an authenticator tap so the request times out.
  virtual_device_factory_->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting(
          [&](device::VirtualFidoDevice* device) { return false; });
  NavigateAndCommit(GURL(kTestOrigin1));

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();

  EXPECT_EQ(
      AuthenticatorMakeCredentialAndWaitForTimeout(std::move(options)).status,
      AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

// Verify behavior for various combinations of origins and RP IDs.
TEST_F(AuthenticatorImplTest, GetAssertionOriginAndRpIds) {
  // These instances should return security errors (for circumstances
  // that would normally crash the renderer).
  for (const OriginClaimedAuthorityPair& test_case :
       kInvalidRelyingPartyTestCases) {
    SCOPED_TRACE(std::string(test_case.claimed_authority) + " " +
                 std::string(test_case.origin));

    NavigateAndCommit(GURL(test_case.origin));

    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->relying_party_id = test_case.claimed_authority;

    EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
              test_case.expected_status);
  }
}

constexpr OriginClaimedAuthorityPair kValidAppIdCases[] = {
    {"https://example.com", "https://example.com",
     AuthenticatorStatus::SUCCESS},
    {"https://www.example.com", "https://example.com",
     AuthenticatorStatus::SUCCESS},
    {"https://example.com", "https://www.example.com",
     AuthenticatorStatus::SUCCESS},
    {"https://example.com", "https://foo.bar.example.com",
     AuthenticatorStatus::SUCCESS},
    {"https://example.com", "https://foo.bar.example.com/foo/bar",
     AuthenticatorStatus::SUCCESS},
    {"https://google.com", "https://www.gstatic.com/securitykey/origins.json",
     AuthenticatorStatus::SUCCESS},
    {"https://www.google.com",
     "https://www.gstatic.com/securitykey/origins.json",
     AuthenticatorStatus::SUCCESS},
    {"https://www.google.com",
     "https://www.gstatic.com/securitykey/a/google.com/origins.json",
     AuthenticatorStatus::SUCCESS},
    {"https://accounts.google.com",
     "https://www.gstatic.com/securitykey/origins.json",
     AuthenticatorStatus::SUCCESS},
};

// Verify behavior for various combinations of origins and RP IDs.
TEST_F(AuthenticatorImplTest, AppIdExtensionValues) {
  for (const auto& test_case : kValidAppIdCases) {
    SCOPED_TRACE(std::string(test_case.origin) + " " +
                 std::string(test_case.claimed_authority));

    EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR,
              TryAuthenticationWithAppId(test_case.origin,
                                         test_case.claimed_authority));

    EXPECT_EQ(AuthenticatorStatus::SUCCESS,
              TryRegistrationWithAppIdExclude(test_case.origin,
                                              test_case.claimed_authority));
  }

  // All the invalid relying party test cases should also be invalid as AppIDs.
  for (const auto& test_case : kInvalidRelyingPartyTestCases) {
    SCOPED_TRACE(std::string(test_case.origin) + " " +
                 std::string(test_case.claimed_authority));

    if (strlen(test_case.claimed_authority) == 0) {
      // In this case, no AppID is actually being tested.
      continue;
    }

    AuthenticatorStatus test_status = TryAuthenticationWithAppId(
        test_case.origin, test_case.claimed_authority);
    EXPECT_TRUE(test_status == AuthenticatorStatus::INVALID_DOMAIN ||
                test_status == test_case.expected_status);

    test_status = TryRegistrationWithAppIdExclude(test_case.origin,
                                                  test_case.claimed_authority);
    EXPECT_TRUE(test_status == AuthenticatorStatus::INVALID_DOMAIN ||
                test_status == test_case.expected_status);
  }
}

// Verify that a request coming from Cryptotoken bypasses origin checks.
TEST_F(AuthenticatorImplTest, CryptotokenBypass) {
  {
    NavigateAndCommit(GURL(kCryptotokenOrigin));
    // First, verify that the Cryptotoken request succeeds with the appid.
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->relying_party_id = std::string(kTestOrigin1);

    // Inject a registration for the URL (which is a U2F AppID).
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        options->allow_credentials[0].id(), kTestOrigin1));

    options->appid = kTestOrigin1;

    GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));
    EXPECT_EQ(result.response->echo_appid_extension, true);
    EXPECT_EQ(result.response->appid_extension, true);
  }

  {
    ResetVirtualDevice();
    NavigateAndCommit(GURL(kTestExtensionOrigin));
    // Next, verify that other extensions cannot bypass the origin checks.
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->relying_party_id = std::string(kTestOrigin1);

    // Inject a registration for the URL (which is a U2F AppID).
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        options->allow_credentials[0].id(), kTestOrigin1));

    options->appid = kTestOrigin1;

    EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
              AuthenticatorStatus::INVALID_DOMAIN);
  }
}

// MakeCredential requests from cryptotoken to a U2F authenticator should
// succeed.
TEST_F(AuthenticatorImplTest, CryptoTokenMakeCredentialU2fDevice) {
  virtual_device_factory_->SetSupportedProtocol(device::ProtocolVersion::kU2f);

  NavigateAndCommit(GURL(kCryptotokenOrigin));
  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->relying_party.id = kTestOrigin1;

  EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
            AuthenticatorStatus::SUCCESS);
}

// MakeCredential requests from cryptotoken to an authentictor that does not
// support U2F should fail.
TEST_F(AuthenticatorImplTest, CryptoTokenMakeCredentialCtap2Device) {
  virtual_device_factory_->SetSupportedProtocol(
      device::ProtocolVersion::kCtap2);
  NavigateAndCommit(GURL(kCryptotokenOrigin));

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->relying_party.id = kTestOrigin1;

  EXPECT_EQ(
      AuthenticatorMakeCredentialAndWaitForTimeout(std::move(options)).status,
      AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

// GetAssertion requests from cryptotoken to a U2F/CTAP authenticator should
// use the U2F interface.
TEST_F(AuthenticatorImplTest, CryptoTokenMakeCredentialDualProtocolDevice) {
  NavigateAndCommit(GURL(kCryptotokenOrigin));
  virtual_device_factory_->SetSupportedProtocol(
      device::ProtocolVersion::kCtap2);
  device::VirtualCtap2Device::Config config;
  config.u2f_support = true;
  virtual_device_factory_->SetCtap2Config(config);

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->relying_party.id = kTestOrigin1;

  EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
            AuthenticatorStatus::SUCCESS);
  ASSERT_EQ(virtual_device_factory_->mutable_state()->registrations.size(), 1u);
  EXPECT_TRUE(virtual_device_factory_->mutable_state()
                  ->registrations.begin()
                  ->second.is_u2f);
}

// GetAssertion requests from cryptotoken to a U2F authenticator should
// succeed.
TEST_F(AuthenticatorImplTest, CryptoTokenGetAssertionU2fDevice) {
  NavigateAndCommit(GURL(kCryptotokenOrigin));
  virtual_device_factory_->SetSupportedProtocol(device::ProtocolVersion::kU2f);

  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id(), kTestOrigin1));
  options->appid = kTestOrigin1;

  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::SUCCESS);
}

// GetAssertion requests from cryptotoken to an authentictor that does not
// support U2F should fail.
TEST_F(AuthenticatorImplTest, CryptoTokenGetAssertionCtap2Device) {
  NavigateAndCommit(GURL(kCryptotokenOrigin));
  virtual_device_factory_->SetSupportedProtocol(
      device::ProtocolVersion::kCtap2);

  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id(), kTestOrigin1));
  options->appid = kTestOrigin1;

  EXPECT_EQ(
      AuthenticatorGetAssertionAndWaitForTimeout(std::move(options)).status,
      AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

// GetAssertion requests from cryptotoken should challenge credential on a
// U2F/CTAP authenticator via the U2F interface.
TEST_F(AuthenticatorImplTest, CryptoTokenGetAssertionDualProtocolDevice) {
  NavigateAndCommit(GURL(kCryptotokenOrigin));
  virtual_device_factory_->SetSupportedProtocol(
      device::ProtocolVersion::kCtap2);
  device::VirtualCtap2Device::Config config;
  config.u2f_support = true;
  config.ignore_u2f_credentials = true;
  virtual_device_factory_->SetCtap2Config(config);

  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id(), kTestOrigin1));
  options->appid = kTestOrigin1;

  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::SUCCESS);
}

// Test that Cryptotoken requests should only be dispatched to USB
// authenticators.
TEST_F(AuthenticatorImplTest, CryptotokenUsbOnly) {
  NavigateAndCommit(GURL(kCryptotokenOrigin));

  // caBLE and platform discoveries cannot be instantiated through
  // VirtualFidoDeviceFactory, so we don't test them here.
  for (const device::FidoTransportProtocol transport :
       {device::FidoTransportProtocol::kUsbHumanInterfaceDevice,
        device::FidoTransportProtocol::kBluetoothLowEnergy,
        device::FidoTransportProtocol::kNearFieldCommunication}) {
    SCOPED_TRACE(::testing::Message()
                 << "transport=" << device::ToString(transport));

    ResetVirtualDevice();
    virtual_device_factory_->SetSupportedProtocol(
        device::ProtocolVersion::kU2f);
    virtual_device_factory_->SetTransport(transport);
    virtual_device_factory_->mutable_state()->transport = transport;

    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();

    if (transport == device::FidoTransportProtocol::kUsbHumanInterfaceDevice) {
      EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
                AuthenticatorStatus::SUCCESS);
    } else {
      EXPECT_EQ(AuthenticatorMakeCredentialAndWaitForTimeout(std::move(options))
                    .status,
                AuthenticatorStatus::NOT_ALLOWED_ERROR);
    }
  }
}

// Verify that a credential registered with U2F can be used via webauthn.
TEST_F(AuthenticatorImplTest, AppIdExtension) {
  NavigateAndCommit(GURL(kTestOrigin1));

  {
    // First, test that the appid extension isn't echoed at all when not
    // requested.
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        options->allow_credentials[0].id(), kTestRelyingPartyId));

    GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));
    ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    EXPECT_EQ(result.response->echo_appid_extension, false);
  }

  {
    // Second, test that the appid extension is echoed, but is false, when appid
    // is requested but not used.
    ResetVirtualDevice();
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        options->allow_credentials[0].id(), kTestRelyingPartyId));

    // This AppID won't be used because the RP ID will be tried (successfully)
    // first.
    options->appid = kTestOrigin1;

    GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));
    ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    EXPECT_EQ(result.response->echo_appid_extension, true);
    EXPECT_EQ(result.response->appid_extension, false);
  }

  {
    // Lastly, when used, the appid extension result should be "true".
    ResetVirtualDevice();
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    // Inject a registration for the URL (which is a U2F AppID).
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        options->allow_credentials[0].id(), kTestOrigin1));

    options->appid = kTestOrigin1;

    GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));
    ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    EXPECT_EQ(result.response->echo_appid_extension, true);
    EXPECT_EQ(result.response->appid_extension, true);
  }

  {
    // AppID should still work when the authenticator supports credProtect.
    ResetVirtualDevice();
    device::VirtualCtap2Device::Config config;
    config.u2f_support = true;
    config.pin_support = true;
    config.resident_key_support = true;
    config.cred_protect_support = true;

    virtual_device_factory_->SetCtap2Config(config);

    // Inject a registration for the URL (which is a U2F AppID).
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        options->allow_credentials[0].id(), kTestOrigin1));

    options->appid = kTestOrigin1;

    GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));
    ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    EXPECT_EQ(result.response->echo_appid_extension, true);
    EXPECT_EQ(result.response->appid_extension, true);
  }
}

TEST_F(AuthenticatorImplTest, AppIdExcludeExtension) {
  NavigateAndCommit(GURL(kTestOrigin1));

  // Attempt to register a credential using the appidExclude extension. It
  // should fail when the registration already exists on the authenticator.
  for (bool credential_already_exists : {false, true}) {
    SCOPED_TRACE(credential_already_exists);

    for (bool is_ctap2 : {false, true}) {
      SCOPED_TRACE(is_ctap2);

      ResetVirtualDevice();
      virtual_device_factory_->SetSupportedProtocol(
          is_ctap2 ? device::ProtocolVersion::kCtap2
                   : device::ProtocolVersion::kU2f);

      PublicKeyCredentialCreationOptionsPtr options =
          GetTestPublicKeyCredentialCreationOptions();
      options->appid_exclude = kTestOrigin1;
      options->exclude_credentials = GetTestCredentials();

      if (credential_already_exists) {
        ASSERT_TRUE(
            virtual_device_factory_->mutable_state()->InjectRegistration(
                options->exclude_credentials[0].id(), kTestOrigin1));
      }

      MakeCredentialResult result =
          AuthenticatorMakeCredential(std::move(options));

      if (credential_already_exists) {
        ASSERT_EQ(result.status, AuthenticatorStatus::CREDENTIAL_EXCLUDED);
      } else {
        ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
      }
    }
  }

  {
    // Using appidExclude with an empty exclude list previously caused a crash.
    // See https://bugs.chromium.org/p/chromium/issues/detail?id=1054499.
    virtual_device_factory_->SetSupportedProtocol(
        device::ProtocolVersion::kCtap2);
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->appid_exclude = kTestOrigin1;
    options->exclude_credentials.clear();
    EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
              AuthenticatorStatus::SUCCESS);
  }

  {
    // Also test the case where all credential IDs are eliminated because of
    // their size.
    device::VirtualCtap2Device::Config config;
    config.max_credential_count_in_list = 1;
    config.max_credential_id_length = 1;
    virtual_device_factory_->SetCtap2Config(config);

    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->appid_exclude = kTestOrigin1;
    options->exclude_credentials = GetTestCredentials();

    for (const auto& cred : options->exclude_credentials) {
      ASSERT_GT(cred.id().size(), config.max_credential_id_length);
    }

    EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
              AuthenticatorStatus::SUCCESS);
  }
}

TEST_F(AuthenticatorImplTest, TestGetAssertionTimeout) {
  // The VirtualFidoAuthenticator simulates a tap immediately after it gets the
  // request. Replace by the real discovery that will wait until timeout.
  AuthenticatorEnvironmentImpl::GetInstance()
      ->ReplaceDefaultDiscoveryFactoryForTesting(
          std::make_unique<device::FidoDiscoveryFactory>());
  NavigateAndCommit(GURL(kTestOrigin1));
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();

  EXPECT_EQ(
      AuthenticatorGetAssertionAndWaitForTimeout(std::move(options)).status,
      AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

TEST_F(AuthenticatorImplTest, OversizedCredentialId) {
  // 255 is the maximum size of a U2F credential ID. We also test one greater
  // (256) to ensure that nothing untoward happens.
  const std::vector<size_t> kSizes = {255, 256};

  for (const size_t size : kSizes) {
    SCOPED_TRACE(size);

    NavigateAndCommit(GURL(kTestOrigin1));
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    device::PublicKeyCredentialDescriptor credential;
    credential.SetCredentialTypeForTesting(device::CredentialType::kPublicKey);
    credential.GetIdForTesting().resize(size);
    credential.GetTransportsForTesting().emplace(
        device::FidoTransportProtocol::kUsbHumanInterfaceDevice);

    const bool should_be_valid = size < 256;
    if (should_be_valid) {
      ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
          credential.id(), kTestRelyingPartyId));
    }

    options->allow_credentials.emplace_back(credential);

    EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
              should_be_valid ? AuthenticatorStatus::SUCCESS
                              : AuthenticatorStatus::NOT_ALLOWED_ERROR);
  }
}

TEST_F(AuthenticatorImplTest, NoSilentAuthenticationForCable) {
  // https://crbug.com/954355
  EnableFeature(features::kWebAuthCable);

  NavigateAndCommit(GURL(kTestOrigin1));

  for (bool is_cable_device : {false, true}) {
    ResetVirtualDevice();
    device::VirtualCtap2Device::Config config;
    config.reject_silent_authentication_requests = true;
    virtual_device_factory_->SetCtap2Config(config);

    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->allow_credentials = GetTestCredentials(/*num_credentials=*/2);
    options->cable_authentication_data = GetTestCableExtension();

    if (is_cable_device) {
      virtual_device_factory_->SetTransport(
          device::FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy);
      for (auto& cred : options->allow_credentials) {
        cred.GetTransportsForTesting().clear();
        cred.GetTransportsForTesting().emplace(
            device::FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy);
      }
    }

    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        options->allow_credentials[0].id(), kTestRelyingPartyId));

    // If a caBLE device is not simulated then silent requests should be used.
    // The virtual device will return an error because
    // |reject_silent_authentication_requests| is true and then it'll
    // immediately resolve the touch request.
    EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
              is_cable_device ? AuthenticatorStatus::SUCCESS
                              : AuthenticatorStatus::NOT_ALLOWED_ERROR);
  }
}

TEST_F(AuthenticatorImplTest, TestGetAssertionU2fDeviceBackwardsCompatibility) {
  NavigateAndCommit(GURL(kTestOrigin1));
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  // Inject credential ID to the virtual device so that successful sign in is
  // possible.
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id(), kTestRelyingPartyId));

  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::SUCCESS);
}

TEST_F(AuthenticatorImplTest, GetAssertionWithEmptyAllowCredentials) {
  NavigateAndCommit(GURL(kTestOrigin1));
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  options->allow_credentials.clear();

  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::RESIDENT_CREDENTIALS_UNSUPPORTED);
}

TEST_F(AuthenticatorImplTest, MakeCredentialAlreadyRegistered) {
  NavigateAndCommit(GURL(kTestOrigin1));
  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();

  // Exclude the one already registered credential.
  options->exclude_credentials = GetTestCredentials();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->exclude_credentials[0].id(), kTestRelyingPartyId));

  EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
            AuthenticatorStatus::CREDENTIAL_EXCLUDED);
}

TEST_F(AuthenticatorImplTest, MakeCredentialPendingRequest) {
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  // Make first request.
  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  TestMakeCredentialCallback callback_receiver;
  authenticator->MakeCredential(std::move(options),
                                callback_receiver.callback());

  // Make second request.
  // TODO(crbug.com/785955): Rework to ensure there are potential race
  // conditions once we have VirtualAuthenticatorEnvironment.
  PublicKeyCredentialCreationOptionsPtr options2 =
      GetTestPublicKeyCredentialCreationOptions();
  TestMakeCredentialCallback callback_receiver2;
  authenticator->MakeCredential(std::move(options2),
                                callback_receiver2.callback());
  callback_receiver2.WaitForCallback();

  EXPECT_EQ(AuthenticatorStatus::PENDING_REQUEST, callback_receiver2.status());

  callback_receiver.WaitForCallback();
}

TEST_F(AuthenticatorImplTest, GetAssertionPendingRequest) {
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  // Make first request.
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  TestGetAssertionCallback callback_receiver;
  authenticator->GetAssertion(std::move(options), callback_receiver.callback());

  // Make second request.
  // TODO(crbug.com/785955): Rework to ensure there are potential race
  // conditions once we have VirtualAuthenticatorEnvironment.
  PublicKeyCredentialRequestOptionsPtr options2 =
      GetTestPublicKeyCredentialRequestOptions();
  TestGetAssertionCallback callback_receiver2;
  authenticator->GetAssertion(std::move(options2),
                              callback_receiver2.callback());
  callback_receiver2.WaitForCallback();

  EXPECT_EQ(AuthenticatorStatus::PENDING_REQUEST, callback_receiver2.status());

  callback_receiver.WaitForCallback();
}

TEST_F(AuthenticatorImplTest, NavigationDuringOperation) {
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  base::RunLoop run_loop;
  authenticator.set_disconnect_handler(run_loop.QuitClosure());

  // Make first request.
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  TestGetAssertionCallback callback_receiver;
  authenticator->GetAssertion(std::move(options), callback_receiver.callback());

  // Simulate a navigation while waiting for the user to press the token.
  virtual_device_factory_->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting([&](device::VirtualFidoDevice* device) {
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindLambdaForTesting(
                           [&]() { NavigateAndCommit(GURL(kTestOrigin2)); }));
        return false;
      });

  run_loop.Run();
}

TEST_F(AuthenticatorImplTest, InvalidResponse) {
  virtual_device_factory_->mutable_state()->simulate_invalid_response = true;
  NavigateAndCommit(GURL(kTestOrigin1));

  {
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    EXPECT_EQ(
        AuthenticatorGetAssertionAndWaitForTimeout(std::move(options)).status,
        AuthenticatorStatus::NOT_ALLOWED_ERROR);
  }

  {
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    EXPECT_EQ(
        AuthenticatorMakeCredentialAndWaitForTimeout(std::move(options)).status,
        AuthenticatorStatus::NOT_ALLOWED_ERROR);
  }
}

TEST_F(AuthenticatorImplTest, Ctap2AssertionWithUnknownCredential) {
  NavigateAndCommit(GURL(kTestOrigin1));

  for (bool return_immediate_invalid_credential_error : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "return_immediate_invalid_credential_error="
                 << return_immediate_invalid_credential_error);

    device::VirtualCtap2Device::Config config;
    config.return_immediate_invalid_credential_error =
        return_immediate_invalid_credential_error;
    virtual_device_factory_->SetCtap2Config(config);

    bool pressed = false;
    virtual_device_factory_->mutable_state()->simulate_press_callback =
        base::BindRepeating(
            [](bool* flag, device::VirtualFidoDevice* device) {
              *flag = true;
              return true;
            },
            &pressed);

    EXPECT_EQ(
        AuthenticatorGetAssertion(GetTestPublicKeyCredentialRequestOptions())
            .status,
        AuthenticatorStatus::NOT_ALLOWED_ERROR);
    // The user must have pressed the authenticator for the operation to
    // resolve.
    EXPECT_TRUE(pressed);
  }
}

TEST_F(AuthenticatorImplTest, GetAssertionResponseWithAttestedCredentialData) {
  device::VirtualCtap2Device::Config config;
  config.return_attested_cred_data_in_get_assertion_response = true;
  virtual_device_factory_->SetCtap2Config(config);
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id(), kTestRelyingPartyId));

  NavigateAndCommit(GURL(kTestOrigin1));

  EXPECT_EQ(
      AuthenticatorGetAssertionAndWaitForTimeout(std::move(options)).status,
      AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

#if defined(OS_WIN)
TEST_F(AuthenticatorImplTest, IsUVPAA) {
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  for (const bool enable_win_webauthn_api : {false, true}) {
    SCOPED_TRACE(enable_win_webauthn_api ? "enable_win_webauthn_api"
                                         : "!enable_win_webauthn_api");
    for (const bool is_uvpaa : {false, true}) {
      SCOPED_TRACE(is_uvpaa ? "is_uvpaa" : "!is_uvpaa");

      fake_win_webauthn_api_.set_available(enable_win_webauthn_api);
      fake_win_webauthn_api_.set_is_uvpaa(is_uvpaa);

      TestIsUvpaaCallback cb;
      authenticator->IsUserVerifyingPlatformAuthenticatorAvailable(
          cb.callback());
      cb.WaitForCallback();
      EXPECT_EQ(enable_win_webauthn_api && is_uvpaa, cb.value());
    }
  }
}
#endif  // defined(OS_WIN)

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(AuthenticatorImplTest, IsUVPAA) {
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();
  TestIsUvpaaCallback cb;
  authenticator->IsUserVerifyingPlatformAuthenticatorAvailable(cb.callback());
  cb.WaitForCallback();
  EXPECT_FALSE(cb.value());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// TestWebAuthenticationDelegate is a test fake implementation of the
// WebAuthentuicationDelegate embedder interface.
class TestWebAuthenticationDelegate : public WebAuthenticationDelegate {
 public:
  absl::optional<bool> IsUserVerifyingPlatformAuthenticatorAvailableOverride(
      RenderFrameHost*) override {
    return is_uvpaa_override;
  }

  absl::optional<std::string> MaybeGetRelyingPartyIdOverride(
      const std::string& claimed_rp_id,
      const url::Origin& caller_origin) override {
    return rp_id_override;
  }

  bool ShouldPermitIndividualAttestation(
      content::BrowserContext* browser_context,
      const std::string& relying_party_id) override {
    return permit_individual_attestation ||
           (permit_individual_attestation_for_rp_id.has_value() &&
            relying_party_id == *permit_individual_attestation_for_rp_id);
  }

  bool SupportsResidentKeys(RenderFrameHost*) override {
    return supports_resident_keys;
  }

  bool IsFocused(WebContents* web_contents) override { return is_focused; }

#if defined(OS_MAC)
  absl::optional<TouchIdAuthenticatorConfig> GetTouchIdAuthenticatorConfig(
      BrowserContext* browser_context) override {
    return touch_id_authenticator_config;
  }
#endif

  // If set, the return value of IsUVPAA() will be overridden with this value.
  // Platform-specific implementations will not be invoked.
  absl::optional<bool> is_uvpaa_override;

  // If set, the delegate will override the RP ID used for WebAuthn requests
  // with this value.
  absl::optional<std::string> rp_id_override;

  // Indicates whether individual attestation should be permitted by the
  // delegate.
  bool permit_individual_attestation = false;

  // A specific RP ID for which individual attestation will be permitted.
  absl::optional<std::string> permit_individual_attestation_for_rp_id;

  // Indicates whether resident key operations should be permitted by the
  // delegate.
  bool supports_resident_keys = false;

  // The return value of the focus check issued at the end of a request.
  bool is_focused = true;

#if defined(OS_MAC)
  // Configuration data for the macOS platform authenticator.
  absl::optional<TouchIdAuthenticatorConfig> touch_id_authenticator_config;
#endif
};

enum class EnterprisePolicy {
  LISTED,
  NOT_LISTED,
};

enum class AttestationConsent {
  GRANTED,
  DENIED,
  GRANTED_FOR_ENTERPRISE_ATTESTATION,
  DENIED_FOR_ENTERPRISE_ATTESTATION,
  NOT_USED,
};

enum class AttestationType {
  ANY,
  NONE,
  NONE_WITH_NONZERO_AAGUID,
  U2F,
  SELF,
  SELF_WITH_NONZERO_AAGUID,
  PACKED,
};

// TestAuthenticatorRequestDelegate is a test fake implementation of the
// AuthenticatorRequestClientDelegate embedder interface.
class TestAuthenticatorRequestDelegate
    : public AuthenticatorRequestClientDelegate {
 public:
  TestAuthenticatorRequestDelegate(
      RenderFrameHost* render_frame_host,
      base::OnceClosure action_callbacks_registered_callback,
      AttestationConsent attestation_consent,
      base::OnceClosure started_over_callback)
      : action_callbacks_registered_callback_(
            std::move(action_callbacks_registered_callback)),
        attestation_consent_(attestation_consent),
        started_over_callback_(std::move(started_over_callback)) {}

  ~TestAuthenticatorRequestDelegate() override {
    CHECK(attestation_consent_queried_ ||
          attestation_consent_ == AttestationConsent::NOT_USED);
  }

  void RegisterActionCallbacks(
      base::OnceClosure cancel_callback,
      base::RepeatingClosure start_over_callback,
      device::FidoRequestHandlerBase::RequestCallback request_callback,
      base::RepeatingClosure bluetooth_adapter_power_on_callback) override {
    ASSERT_TRUE(action_callbacks_registered_callback_)
        << "RegisterActionCallbacks called twice.";
    cancel_callback_.emplace(std::move(cancel_callback));
    std::move(action_callbacks_registered_callback_).Run();
    if (started_over_callback_) {
      action_callbacks_registered_callback_ = std::move(started_over_callback_);
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(std::move(start_over_callback)));
    }
  }

  void ShouldReturnAttestation(
      const std::string& relying_party_id,
      const device::FidoAuthenticator* authenticator,
      bool is_enterprise_attestation,
      base::OnceCallback<void(bool)> callback) override {
    bool result = false;
    switch (attestation_consent_) {
      case AttestationConsent::NOT_USED:
        CHECK(false);
        break;
      case AttestationConsent::DENIED:
        CHECK(!is_enterprise_attestation);
        break;
      case AttestationConsent::GRANTED:
        CHECK(!is_enterprise_attestation);
        result = true;
        break;
      case AttestationConsent::DENIED_FOR_ENTERPRISE_ATTESTATION:
        CHECK(is_enterprise_attestation);
        break;
      case AttestationConsent::GRANTED_FOR_ENTERPRISE_ATTESTATION:
        CHECK(is_enterprise_attestation);
        result = true;
        break;
    }

    attestation_consent_queried_ = true;
    std::move(callback).Run(result);
  }

  void OnTransportAvailabilityEnumerated(
      device::FidoRequestHandlerBase::TransportAvailabilityInfo transport_info)
      override {
    // Simulate the behaviour of Chrome's |AuthenticatorRequestDialogModel|
    // which shows a specific error when no transports are available and lets
    // the user cancel the request.
    if (transport_info.available_transports.empty()) {
      std::move(*cancel_callback_).Run();
    }
  }

  base::OnceClosure action_callbacks_registered_callback_;
  absl::optional<base::OnceClosure> cancel_callback_;
  const AttestationConsent attestation_consent_;
  base::OnceClosure started_over_callback_;
  bool attestation_consent_queried_ = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAuthenticatorRequestDelegate);
};

// TestAuthenticatorContentBrowserClient is a test fake implementation of the
// ContentBrowserClient interface that injects |TestWebAuthenticationDelegate|
// and |TestAuthenticatorRequestDelegate| instances into |AuthenticatorImpl|.
class TestAuthenticatorContentBrowserClient : public ContentBrowserClient {
 public:
  TestWebAuthenticationDelegate* GetTestWebAuthenticationDelegate() {
    return &web_authentication_delegate;
  }

  // ContentBrowserClient:
  WebAuthenticationDelegate* GetWebAuthenticationDelegate() override {
    return &web_authentication_delegate;
  }

  std::unique_ptr<AuthenticatorRequestClientDelegate>
  GetWebAuthenticationRequestDelegate(
      RenderFrameHost* render_frame_host) override {
    if (return_null_delegate)
      return nullptr;
    return std::make_unique<TestAuthenticatorRequestDelegate>(
        render_frame_host,
        action_callbacks_registered_callback
            ? std::move(action_callbacks_registered_callback)
            : base::DoNothing(),
        attestation_consent, std::move(started_over_callback_));
  }

  TestWebAuthenticationDelegate web_authentication_delegate;

  // If set, this closure will be called when the subsequently constructed
  // delegate is informed that the request has started.
  base::OnceClosure action_callbacks_registered_callback;

  AttestationConsent attestation_consent = AttestationConsent::NOT_USED;

  // This emulates scenarios where a nullptr RequestClientDelegate is returned
  // because a request is already in progress.
  bool return_null_delegate = false;

  // If started_over_callback_ is set to a non-null callback, the request will
  // be restarted after action callbacks are registered, and
  // |started_over_callback| will replace
  // |action_callbacks_registered_callback|. This should then be called the
  // second time action callbacks are registered.
  base::OnceClosure started_over_callback_;
};

// A test class that installs and removes an
// |TestAuthenticatorContentBrowserClient| automatically and can run tests
// against simulated attestation results.
class AuthenticatorContentBrowserClientTest : public AuthenticatorImplTest {
 public:
  AuthenticatorContentBrowserClientTest() = default;

  struct TestCase {
    AttestationConveyancePreference attestation_requested;
    EnterprisePolicy enterprise_policy;
    AttestationConsent attestation_consent;
    AuthenticatorStatus expected_status;
    AttestationType expected_attestation;
    const char* expected_certificate_substring;
  };

  void SetUp() override {
    AuthenticatorImplTest::SetUp();
    old_client_ = SetBrowserClientForTesting(&test_client_);
  }

  void TearDown() override {
    SetBrowserClientForTesting(old_client_);
    AuthenticatorImplTest::TearDown();
  }

  void RunTestCases(const std::vector<TestCase>& tests) {
    for (size_t i = 0; i < tests.size(); i++) {
      const auto& test = tests[i];
      if (test.attestation_consent != AttestationConsent::NOT_USED) {
        SCOPED_TRACE(test.attestation_consent == AttestationConsent::GRANTED
                         ? "consent granted"
                         : "consent denied");
      }
      SCOPED_TRACE(test.enterprise_policy == EnterprisePolicy::LISTED
                       ? "individual attestation"
                       : "no individual attestation");
      SCOPED_TRACE(
          AttestationConveyancePreferenceToString(test.attestation_requested));
      SCOPED_TRACE(i);

      test_client_.GetTestWebAuthenticationDelegate()
          ->permit_individual_attestation =
          test.enterprise_policy == EnterprisePolicy::LISTED;
      test_client_.attestation_consent = test.attestation_consent;

      PublicKeyCredentialCreationOptionsPtr options =
          GetTestPublicKeyCredentialCreationOptions();
      options->relying_party.id = "example.com";
      options->timeout = base::TimeDelta::FromSeconds(1);
      options->attestation =
          ConvertAttestationConveyancePreference(test.attestation_requested);

      MakeCredentialResult result =
          AuthenticatorMakeCredential(std::move(options));
      EXPECT_EQ(result.status, test.expected_status);

      if (test.expected_status != AuthenticatorStatus::SUCCESS) {
        ASSERT_EQ(AttestationType::ANY, test.expected_attestation);
        continue;
      }

      const device::AuthenticatorData auth_data =
          AuthDataFromMakeCredentialResponse(result.response);

      absl::optional<Value> attestation_value =
          Reader::Read(result.response->attestation_object);
      ASSERT_TRUE(attestation_value);
      ASSERT_TRUE(attestation_value->is_map());
      const auto& attestation = attestation_value->GetMap();

      switch (test.expected_attestation) {
        case AttestationType::ANY:
          ASSERT_STREQ("", test.expected_certificate_substring);
          break;

        case AttestationType::NONE:
          ASSERT_STREQ("", test.expected_certificate_substring);
          ExpectMapHasKeyWithStringValue(attestation, "fmt", "none");
          EXPECT_TRUE(auth_data.attested_data()->IsAaguidZero());
          break;

        case AttestationType::NONE_WITH_NONZERO_AAGUID:
          ASSERT_STREQ("", test.expected_certificate_substring);
          ExpectMapHasKeyWithStringValue(attestation, "fmt", "none");
          EXPECT_FALSE(auth_data.attested_data()->IsAaguidZero());
          break;

        case AttestationType::U2F:
          ExpectMapHasKeyWithStringValue(attestation, "fmt", "fido-u2f");
          if (strlen(test.expected_certificate_substring) > 0) {
            ExpectCertificateContainingSubstring(
                attestation, test.expected_certificate_substring);
          }
          break;

        case AttestationType::PACKED:
          ExpectMapHasKeyWithStringValue(attestation, "fmt", "packed");
          if (strlen(test.expected_certificate_substring) > 0) {
            ExpectCertificateContainingSubstring(
                attestation, test.expected_certificate_substring);
          }
          break;

        case AttestationType::SELF: {
          ASSERT_STREQ("", test.expected_certificate_substring);
          ExpectMapHasKeyWithStringValue(attestation, "fmt", "packed");

          // A self-attestation should not include an X.509 chain nor ECDAA key.
          const auto attestation_statement_it =
              attestation.find(Value("attStmt"));
          ASSERT_TRUE(attestation_statement_it != attestation.end());
          ASSERT_TRUE(attestation_statement_it->second.is_map());
          const auto& attestation_statement =
              attestation_statement_it->second.GetMap();

          ASSERT_TRUE(attestation_statement.find(Value("x5c")) ==
                      attestation_statement.end());
          ASSERT_TRUE(attestation_statement.find(Value("ecdaaKeyId")) ==
                      attestation_statement.end());
          EXPECT_TRUE(auth_data.attested_data()->IsAaguidZero());
          break;
        }
        case AttestationType::SELF_WITH_NONZERO_AAGUID: {
          ASSERT_STREQ("", test.expected_certificate_substring);
          ExpectMapHasKeyWithStringValue(attestation, "fmt", "packed");

          // A self-attestation should not include an X.509 chain nor ECDAA key.
          const auto attestation_statement_it =
              attestation.find(Value("attStmt"));
          ASSERT_TRUE(attestation_statement_it != attestation.end());
          ASSERT_TRUE(attestation_statement_it->second.is_map());
          const auto& attestation_statement =
              attestation_statement_it->second.GetMap();

          ASSERT_TRUE(attestation_statement.find(Value("x5c")) ==
                      attestation_statement.end());
          ASSERT_TRUE(attestation_statement.find(Value("ecdaaKeyId")) ==
                      attestation_statement.end());
          EXPECT_FALSE(auth_data.attested_data()->IsAaguidZero());
          break;
        }
      }
    }
  }

 protected:
  TestAuthenticatorContentBrowserClient test_client_;

  static const char* AttestationConveyancePreferenceToString(
      AttestationConveyancePreference v) {
    switch (v) {
      case AttestationConveyancePreference::NONE:
        return "none";
      case AttestationConveyancePreference::INDIRECT:
        return "indirect";
      case AttestationConveyancePreference::DIRECT:
        return "direct";
      case AttestationConveyancePreference::ENTERPRISE:
        return "enterprise";
      default:
        NOTREACHED();
        return "";
    }
  }

  // Expects that |map| contains the given key with a string-value equal to
  // |expected|.
  static void ExpectMapHasKeyWithStringValue(const Value::MapValue& map,
                                             const char* key,
                                             const char* expected) {
    const auto it = map.find(Value(key));
    ASSERT_TRUE(it != map.end()) << "No such key '" << key << "'";
    const auto& value = it->second;
    EXPECT_TRUE(value.is_string())
        << "Value of '" << key << "' has type "
        << static_cast<int>(value.type()) << ", but expected to find a string";
    EXPECT_EQ(std::string(expected), value.GetString())
        << "Value of '" << key << "' is '" << value.GetString()
        << "', but expected to find '" << expected << "'";
  }

  // Asserts that the webauthn attestation CBOR map in |attestation| contains a
  // single X.509 certificate containing |substring|.
  static void ExpectCertificateContainingSubstring(
      const Value::MapValue& attestation,
      const std::string& substring) {
    const auto& attestation_statement_it = attestation.find(Value("attStmt"));
    ASSERT_TRUE(attestation_statement_it != attestation.end());
    ASSERT_TRUE(attestation_statement_it->second.is_map());
    const auto& attestation_statement =
        attestation_statement_it->second.GetMap();
    const auto& x5c_it = attestation_statement.find(Value("x5c"));
    ASSERT_TRUE(x5c_it != attestation_statement.end());
    ASSERT_TRUE(x5c_it->second.is_array());
    const auto& x5c = x5c_it->second.GetArray();
    ASSERT_EQ(1u, x5c.size());
    ASSERT_TRUE(x5c[0].is_bytestring());
    base::StringPiece cert = x5c[0].GetBytestringAsString();
    EXPECT_TRUE(cert.find(substring) != cert.npos);
  }

  ContentBrowserClient* old_client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AuthenticatorContentBrowserClientTest);
};

// Test that credentials can be created and used from an extension origin when
// permitted by the delegate.
TEST_F(AuthenticatorContentBrowserClientTest, ChromeExtensions) {
  static constexpr char kExtensionId[] = "abcdefg";
  static const std::string kExtensionOrigin =
      std::string("chrome-extension://") + kExtensionId;

  NavigateAndCommit(GURL(kExtensionOrigin + "/test.html"));

  for (bool permit_rp_id_override : {false, true}) {
    SCOPED_TRACE(testing::Message() << "permit=" << permit_rp_id_override);
    if (permit_rp_id_override) {
      test_client_.GetTestWebAuthenticationDelegate()->rp_id_override =
          kExtensionOrigin;
    } else {
      test_client_.GetTestWebAuthenticationDelegate()->rp_id_override =
          absl::nullopt;
    }

    std::vector<uint8_t> credential_id;
    {
      PublicKeyCredentialCreationOptionsPtr options =
          GetTestPublicKeyCredentialCreationOptions();
      options->relying_party.id = kExtensionId;

      MakeCredentialResult result =
          AuthenticatorMakeCredential(std::move(options));
      if (permit_rp_id_override) {
        EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
        credential_id = result.response->info->raw_id;
      } else {
        EXPECT_EQ(result.status, AuthenticatorStatus::INVALID_DOMAIN);
      }
    }

    {
      PublicKeyCredentialRequestOptionsPtr options =
          GetTestPublicKeyCredentialRequestOptions();
      options->relying_party_id = kExtensionId;
      options->allow_credentials[0] = device::PublicKeyCredentialDescriptor(
          device::CredentialType::kPublicKey, std::move(credential_id));

      EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
                permit_rp_id_override ? AuthenticatorStatus::SUCCESS
                                      : AuthenticatorStatus::INVALID_DOMAIN);
    }
  }
}

TEST_F(AuthenticatorContentBrowserClientTest, AttestationBehaviour) {
  const char kStandardCommonName[] = "U2F Attestation";
  const char kIndividualCommonName[] = "Individual Cert";

  const std::vector<TestCase> kTests = {
      {
          AttestationConveyancePreference::NONE,
          EnterprisePolicy::NOT_LISTED,
          AttestationConsent::NOT_USED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
      {
          AttestationConveyancePreference::NONE,
          EnterprisePolicy::LISTED,
          AttestationConsent::NOT_USED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
      {
          AttestationConveyancePreference::INDIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationConsent::DENIED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
      {
          AttestationConveyancePreference::INDIRECT,
          EnterprisePolicy::LISTED,
          AttestationConsent::DENIED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
      {
          AttestationConveyancePreference::INDIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationConsent::GRANTED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::U2F,
          kStandardCommonName,
      },
      {
          AttestationConveyancePreference::INDIRECT,
          EnterprisePolicy::LISTED,
          AttestationConsent::GRANTED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::U2F,
          kStandardCommonName,
      },
      {
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationConsent::DENIED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
      {
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::LISTED,
          AttestationConsent::DENIED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
      {
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationConsent::GRANTED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::U2F,
          kStandardCommonName,
      },
      {
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::LISTED,
          AttestationConsent::GRANTED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::U2F,
          kStandardCommonName,
      },
      {
          // Requesting enterprise attestation and not being approved results in
          // no attestation.
          AttestationConveyancePreference::ENTERPRISE,
          EnterprisePolicy::NOT_LISTED,
          AttestationConsent::NOT_USED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
      {
          AttestationConveyancePreference::ENTERPRISE,
          EnterprisePolicy::LISTED,
          AttestationConsent::NOT_USED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::U2F,
          kIndividualCommonName,
      },
  };

  virtual_device_factory_->mutable_state()->attestation_cert_common_name =
      kStandardCommonName;
  virtual_device_factory_->mutable_state()
      ->individual_attestation_cert_common_name = kIndividualCommonName;
  NavigateAndCommit(GURL("https://example.com"));

  RunTestCases(kTests);
}

TEST_F(AuthenticatorContentBrowserClientTest, Ctap2EnterpriseAttestation) {
  const char kStandardCommonName[] = "U2F Attestation";
  const char kIndividualCommonName[] = "Individual Cert";
  virtual_device_factory_->mutable_state()->attestation_cert_common_name =
      kStandardCommonName;
  virtual_device_factory_->mutable_state()
      ->individual_attestation_cert_common_name = kIndividualCommonName;
  NavigateAndCommit(GURL("https://example.com"));

  {
    SCOPED_TRACE("Without RP listed");

    device::VirtualCtap2Device::Config config;
    config.support_enterprise_attestation = true;
    virtual_device_factory_->SetCtap2Config(config);

    const std::vector<TestCase> kTests = {
        {
            AttestationConveyancePreference::ENTERPRISE,
            EnterprisePolicy::LISTED,
            AttestationConsent::NOT_USED,
            AuthenticatorStatus::SUCCESS,
            AttestationType::PACKED,
            kIndividualCommonName,
        },
        {
            // Requesting enterprise attestation and not being approved results
            // in no attestation.
            AttestationConveyancePreference::ENTERPRISE,
            EnterprisePolicy::NOT_LISTED,
            AttestationConsent::NOT_USED,
            AuthenticatorStatus::SUCCESS,
            AttestationType::NONE,
            "",
        },
    };

    RunTestCases(kTests);
  }

  {
    SCOPED_TRACE("With RP listed");

    device::VirtualCtap2Device::Config config;
    config.support_enterprise_attestation = true;
    config.enterprise_attestation_rps = {"example.com"};
    virtual_device_factory_->SetCtap2Config(config);

    const std::vector<TestCase> kTests = {
        {
            // Despite not being listed in enterprise policy, since the
            // authenticator recognises the RP ID, attestation should still be
            // returned.
            AttestationConveyancePreference::ENTERPRISE,
            EnterprisePolicy::NOT_LISTED,
            AttestationConsent::GRANTED_FOR_ENTERPRISE_ATTESTATION,
            AuthenticatorStatus::SUCCESS,
            AttestationType::PACKED,
            kIndividualCommonName,
        },
        {
            // Consent is required in the case that an enterprise attestation is
            // approved by an authenticator, however.
            AttestationConveyancePreference::ENTERPRISE,
            EnterprisePolicy::NOT_LISTED,
            AttestationConsent::DENIED_FOR_ENTERPRISE_ATTESTATION,
            AuthenticatorStatus::SUCCESS,
            AttestationType::NONE,
            "",
        },
    };

    RunTestCases(kTests);
  }
}

TEST_F(AuthenticatorContentBrowserClientTest,
       Ctap2EnterpriseAttestationUnsolicited) {
  NavigateAndCommit(GURL(kTestOrigin1));

  device::VirtualCtap2Device::Config config;
  config.support_enterprise_attestation = true;
  virtual_device_factory_->SetCtap2Config(config);

  {
    EXPECT_EQ(
        AuthenticatorMakeCredential(GetTestPublicKeyCredentialCreationOptions())
            .status,
        AuthenticatorStatus::SUCCESS);
  }

  config.always_return_enterprise_attestation = true;
  virtual_device_factory_->SetCtap2Config(config);

  {
    EXPECT_EQ(
        AuthenticatorMakeCredential(GetTestPublicKeyCredentialCreationOptions())
            .status,
        AuthenticatorStatus::NOT_ALLOWED_ERROR);
  }
}

TEST_F(AuthenticatorContentBrowserClientTest,
       InappropriatelyIdentifyingAttestation) {
  // This common name is used by several devices that have inappropriately
  // identifying attestation certificates.
  const char kCommonName[] = "FT FIDO 0100";

  const std::vector<TestCase> kTests = {
      {
          AttestationConveyancePreference::ENTERPRISE,
          EnterprisePolicy::NOT_LISTED,
          AttestationConsent::NOT_USED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
      {
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationConsent::GRANTED,
          AuthenticatorStatus::SUCCESS,
          // If individual attestation was not requested then the attestation
          // certificate will be removed, even if consent is given, because the
          // consent isn't to be tracked.
          AttestationType::NONE,
          "",
      },
      {
          AttestationConveyancePreference::ENTERPRISE,
          EnterprisePolicy::LISTED,
          AttestationConsent::NOT_USED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::U2F,
          kCommonName,
      },
  };

  virtual_device_factory_->mutable_state()->attestation_cert_common_name =
      kCommonName;
  virtual_device_factory_->mutable_state()
      ->individual_attestation_cert_common_name = kCommonName;
  NavigateAndCommit(GURL("https://example.com"));

  RunTestCases(kTests);
}

// Test attestation erasure for an authenticator that uses self-attestation
// (which requires a zero AAGUID), but has a non-zero AAGUID. This mirrors the
// behavior of the Touch ID platform authenticator.
TEST_F(AuthenticatorContentBrowserClientTest,
       PlatformAuthenticatorAttestation) {
  test_client_.GetTestWebAuthenticationDelegate()->is_uvpaa_override = true;
  virtual_device_factory_->SetSupportedProtocol(
      device::ProtocolVersion::kCtap2);
  virtual_device_factory_->SetTransport(
      device::FidoTransportProtocol::kInternal);
  virtual_device_factory_->mutable_state()->self_attestation = true;
  virtual_device_factory_->mutable_state()
      ->non_zero_aaguid_with_self_attestation = true;
  NavigateAndCommit(GURL("https://example.com"));

  const std::vector<TestCase> kTests = {
      {
          // Self-attestation is defined as having a zero AAGUID, but
          // |non_zero_aaguid_with_self_attestation| is set above. Thus, if no
          // attestation is requested, the self-attestation will be removed but,
          // because the transport is kInternal, the AAGUID will be preserved.
          AttestationConveyancePreference::NONE,
          EnterprisePolicy::NOT_LISTED,
          AttestationConsent::NOT_USED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE_WITH_NONZERO_AAGUID,
          "",
      },
      {
          // Attestation is always returned if requested because it is privacy
          // preserving. The AttestationConsent value is irrelevant.
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationConsent::NOT_USED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::SELF_WITH_NONZERO_AAGUID,
          "",
      },
  };

  RunTestCases(kTests);
}

TEST_F(AuthenticatorContentBrowserClientTest, Ctap2SelfAttestation) {
  virtual_device_factory_->SetSupportedProtocol(
      device::ProtocolVersion::kCtap2);
  virtual_device_factory_->mutable_state()->self_attestation = true;
  NavigateAndCommit(GURL("https://example.com"));

  const std::vector<TestCase> kTests = {
      {
          // If no attestation is requested, we'll return the self attestation
          // rather than erasing it.
          AttestationConveyancePreference::NONE,
          EnterprisePolicy::NOT_LISTED,
          AttestationConsent::NOT_USED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::SELF,
          "",
      },
      {
          // If attestation is requested, but denied, we'll return none
          // attestation.
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationConsent::DENIED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
      {
          // If attestation is requested and granted, the self attestation will
          // be returned.
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationConsent::GRANTED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::SELF,
          "",
      },
  };

  RunTestCases(kTests);
}

TEST_F(AuthenticatorContentBrowserClientTest,
       Ctap2SelfAttestationNonZeroAaguid) {
  virtual_device_factory_->SetSupportedProtocol(
      device::ProtocolVersion::kCtap2);
  virtual_device_factory_->mutable_state()->self_attestation = true;
  virtual_device_factory_->mutable_state()
      ->non_zero_aaguid_with_self_attestation = true;
  NavigateAndCommit(GURL("https://example.com"));

  const std::vector<TestCase> kTests = {
      {
          // Since the virtual device is configured to set a non-zero AAGUID the
          // self-attestation should still be replaced with a "none"
          // attestation.
          AttestationConveyancePreference::NONE,
          EnterprisePolicy::NOT_LISTED,
          AttestationConsent::NOT_USED,
          AuthenticatorStatus::SUCCESS,
          AttestationType::NONE,
          "",
      },
  };

  RunTestCases(kTests);
}

TEST_F(AuthenticatorContentBrowserClientTest,
       GoogleLegacyAppidSupportEnterpriseAttestation) {
  // When the googleLegacyAppidSupport extension is used, individual attestation
  // decisions should key off the AppId, not the RP ID.
  constexpr char kGstaticAppId[] =
      "https://www.gstatic.com/securitykey/origins.json";
  test_client_.GetTestWebAuthenticationDelegate()
      ->permit_individual_attestation_for_rp_id = kGstaticAppId;

  const char kStandardCommonName[] = "U2F Attestation";
  const char kIndividualCommonName[] = "Individual Cert";
  virtual_device_factory_->mutable_state()->attestation_cert_common_name =
      kStandardCommonName;
  virtual_device_factory_->mutable_state()
      ->individual_attestation_cert_common_name = kIndividualCommonName;

  NavigateAndCommit(GURL("https://accounts.google.com"));

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->relying_party.id = "google.com";
  options->google_legacy_app_id_support = true;
  options->attestation = ::device::AttestationConveyancePreference::
      kEnterpriseIfRPListedOnAuthenticator;

  auto result = AuthenticatorMakeCredential(std::move(options));
  ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);

  const device::AuthenticatorData auth_data =
      AuthDataFromMakeCredentialResponse(result.response);
  absl::optional<Value> attestation_value =
      Reader::Read(result.response->attestation_object);
  ASSERT_TRUE(attestation_value);
  ASSERT_TRUE(attestation_value->is_map());
  const auto& attestation = attestation_value->GetMap();

  ExpectMapHasKeyWithStringValue(attestation, "fmt", "fido-u2f");
  ExpectCertificateContainingSubstring(attestation, kIndividualCommonName);
}

TEST_F(AuthenticatorContentBrowserClientTest, BlockedAttestation) {
  NavigateAndCommit(GURL("https://foo.example.com"));

  static constexpr struct {
    const char* filter_json;
    AttestationConveyancePreference attestation;
    EnterprisePolicy enterprise_policy;
    AttestationType result;
  } kTests[] = {
      // Empty or nonsense filter doesn't block anything.
      {
          "",
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationType::U2F,
      },
      {
          R"({"filters": []})",
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationType::U2F,
      },
      // Direct listing of domain blocks...
      {
          R"({"filters": [{
            "operation": "mc",
            "rp_id": "example.com",
            "action": "no-attestation"
          }]})",
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationType::NONE,
      },
      // ... unless attestation is permitted by policy.
      {
          R"({"filters": [{
            "operation": "mc",
            "rp_id": "example.com",
            "action": "no-attestation"
          }]})",
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::LISTED,
          AttestationType::U2F,
      },
      // The whole domain can be blocked. (Note, blocking a domain would
      // normally want to list both the base domain and a pattern for
      // subdomains because the below also matches fooexample.com.)
      {
          R"({"filters": [{
            "operation": "mc",
            "rp_id": "*example.com",
            "action": "no-attestation"
          }]})",
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationType::NONE,
      },
      // Policy again overrides
      {
          R"({"filters": [{
            "operation": "mc",
            "rp_id": "*example.com",
            "action": "no-attestation"
          }]})",
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::LISTED,
          AttestationType::U2F,
      },
      // An explicit wildcard will match everything, be careful. (Omitting
      // both RP ID and device is a parse error, however.)
      {
          R"({"filters": [{
            "operation": "mc",
            "rp_id": "*",
            "action": "no-attestation"
          }]})",
          AttestationConveyancePreference::DIRECT,
          EnterprisePolicy::NOT_LISTED,
          AttestationType::NONE,
      },
  };

  int test_num = 0;
  for (const auto& test : kTests) {
    SCOPED_TRACE(test_num++);
    SCOPED_TRACE(test.filter_json);

    device::fido_filter::ScopedFilterForTesting filter(test.filter_json);

    const std::vector<TestCase> kTestCase = {
        {
            test.attestation,
            test.enterprise_policy,
            test.result == AttestationType::U2F ? AttestationConsent::GRANTED
                                                : AttestationConsent::NOT_USED,
            AuthenticatorStatus::SUCCESS,
            test.result,
            "",
        },
    };

    RunTestCases(kTestCase);
  }
}

TEST_F(AuthenticatorContentBrowserClientTest, FilteringMakeCredential) {
  static const struct {
    const char* filter_json;
    bool expect_make_credential_success;
  } kTests[] = {
      {
          R"()",
          true,
      },
      // Block by device.
      {
          R"({"filters": [{
          "operation": "mc",
          "device": "VirtualFidoDevice-*",
          "action": "block",
          }]})",
          false,
      },
      // Shouldn't block when the device is unrelated.
      {
          R"({"filters": [{
          "operation": "mc",
          "device": "OtherDevice-*",
          "action": "block",
          }]})",
          true,
      },
      // Block by RP ID.
      {
          R"({"filters": [{
          "operation": "mc",
          "rp_id": "google.com",
          "action": "block",
          }]})",
          false,
      },
      // Unrelated RP ID.
      {
          R"({"filters": [{
          "operation": "mc",
          "rp_id": "other.com",
          "action": "block",
          }]})",
          true,
      },
      // Block specific user ID.
      {
          R"({"filters": [{
          "operation": "mc",
          "rp_id": "*",
          "id_type": "user",
          "id": "0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A",
          "action": "block",
          }]})",
          false,
      },
      // Different user ID.
      {
          R"({"filters": [{
          "operation": "mc",
          "rp_id": "*",
          "id_type": "user",
          "id": "FF0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A0A",
          "action": "block",
          }]})",
          true,
      },
      // Block by user ID length.
      {
          R"({"filters": [{
          "operation": "mc",
          "rp_id": "*",
          "id_type": "user",
          "id_min_size": 32,
          "id_max_size": 32,
          "action": "block",
          }]})",
          false,
      },
      // Block user IDs that are longer than specified by
      // |GetTestPublicKeyCredentialUserEntity|.
      {
          R"({"filters": [{
          "operation": "mc",
          "rp_id": "*",
          "id_type": "user",
          "id_min_size": 33,
          "action": "block",
          }]})",
          true,
      },
      // Block excluded credential ID.
      {
          R"({"filters": [{
          "operation": "mc",
          "rp_id": "*",
          "id_type": "cred",
          "id": "0000000000000000000000000000000000000000000000000000000000000000",
          "action": "block",
          }]})",
          false,
      },
      // Block different credential ID.
      {
          R"({"filters": [{
          "operation": "mc",
          "rp_id": "*",
          "id_type": "cred",
          "id": "FF00000000000000000000000000000000000000000000000000000000000000",
          "action": "block",
          }]})",
          true,
      },
      // Block by excluded credential ID length.
      {
          R"({"filters": [{
          "operation": "mc",
          "rp_id": "*",
          "id_type": "cred",
          "id_min_size": 32,
          "id_max_size": 32,
          "action": "block",
          }]})",
          false,
      },
      // Block longer credentials IDs than are used.
      {
          R"({"filters": [{
          "operation": "mc",
          "rp_id": "*",
          "id_type": "cred",
          "id_min_size": 33,
          "action": "block",
          }]})",
          true,
      },
  };

  NavigateAndCommit(GURL(kTestOrigin1));

  int test_num = 0;
  for (const auto& test : kTests) {
    SCOPED_TRACE(test_num++);
    SCOPED_TRACE(test.filter_json);
    device::fido_filter::ScopedFilterForTesting filter(test.filter_json);

    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->exclude_credentials = GetTestCredentials();
    EXPECT_EQ(AuthenticatorMakeCredentialAndWaitForTimeout(std::move(options))
                      .status == AuthenticatorStatus::SUCCESS,
              test.expect_make_credential_success);
  }
}

TEST_F(AuthenticatorContentBrowserClientTest, FilteringGetAssertion) {
  static const struct {
    const char* filter_json;
    bool expect_get_assertion_success;
  } kTests[] = {
      {
          R"()",
          true,
      },
      // Block by device.
      {
          R"({"filters": [{
          "operation": "ga",
          "device": "VirtualFidoDevice-*",
          "action": "block",
          }]})",
          false,
      },
      // Shouldn't block when the device is unrelated.
      {
          R"({"filters": [{
          "operation": "ga",
          "device": "OtherDevice-*",
          "action": "block",
          }]})",
          true,
      },
      // Block by RP ID.
      {
          R"({"filters": [{
          "operation": "ga",
          "rp_id": "google.com",
          "action": "block",
          }]})",
          false,
      },
      // Unrelated RP ID.
      {
          R"({"filters": [{
          "operation": "ga",
          "rp_id": "other.com",
          "action": "block",
          }]})",
          true,
      },
      // Block allowList credential ID.
      {
          R"({"filters": [{
          "operation": "ga",
          "rp_id": "*",
          "id_type": "cred",
          "id": "0000000000000000000000000000000000000000000000000000000000000000",
          "action": "block",
          }]})",
          false,
      },
      // Block different credential ID.
      {
          R"({"filters": [{
          "operation": "ga",
          "rp_id": "*",
          "id_type": "cred",
          "id": "FF00000000000000000000000000000000000000000000000000000000000000",
          "action": "block",
          }]})",
          true,
      },
      // Block by allowList credential ID length for credentials returned by
      // |GetTestCredentials|.
      {
          R"({"filters": [{
          "operation": "ga",
          "rp_id": "*",
          "id_type": "cred",
          "id_min_size": 32,
          "id_max_size": 32,
          "action": "block",
          }]})",
          false,
      },
      // Block longer credentials IDs than are used.
      {
          R"({"filters": [{
          "operation": "ga",
          "rp_id": "*",
          "id_type": "cred",
          "id_min_size": 33,
          "action": "block",
          }]})",
          true,
      },
  };

  NavigateAndCommit(GURL(kTestOrigin1));

  int test_num = 0;
  bool credential_added = false;
  for (const auto& test : kTests) {
    SCOPED_TRACE(test_num++);
    SCOPED_TRACE(test.filter_json);
    device::fido_filter::ScopedFilterForTesting filter(test.filter_json);

    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    if (!credential_added) {
      ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
          options->allow_credentials[0].id(), kTestRelyingPartyId));
      credential_added = true;
    }

    EXPECT_EQ(
        AuthenticatorGetAssertionAndWaitForTimeout(std::move(options)).status ==
            AuthenticatorStatus::SUCCESS,
        test.expect_get_assertion_success);
  }
}

TEST_F(AuthenticatorContentBrowserClientTest, FilteringFailsOpen) {
  // Setting the filter to invalid JSON should not filter anything.
  device::fido_filter::ScopedFilterForTesting filter(
      "nonsense",
      device::fido_filter::ScopedFilterForTesting::PermitInvalidJSON::kYes);

  NavigateAndCommit(GURL(kTestOrigin1));
  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->exclude_credentials = GetTestCredentials();
  EXPECT_EQ(
      AuthenticatorMakeCredentialAndWaitForTimeout(std::move(options)).status,
      AuthenticatorStatus::SUCCESS);
}

TEST_F(AuthenticatorContentBrowserClientTest,
       MakeCredentialRequestStartedCallback) {
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();

  TestRequestStartedCallback request_started;
  test_client_.action_callbacks_registered_callback =
      request_started.callback();
  authenticator->MakeCredential(std::move(options), base::DoNothing());
  request_started.WaitForCallback();
}

TEST_F(AuthenticatorContentBrowserClientTest,
       GetAssertionRequestStartedCallback) {
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();

  TestRequestStartedCallback request_started;
  test_client_.action_callbacks_registered_callback =
      request_started.callback();
  authenticator->GetAssertion(std::move(options), base::DoNothing());
  request_started.WaitForCallback();
}

TEST_F(AuthenticatorContentBrowserClientTest, MakeCredentialStartOver) {
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();

  TestRequestStartedCallback request_started;
  test_client_.action_callbacks_registered_callback =
      request_started.callback();
  TestRequestStartedCallback request_restarted;
  test_client_.started_over_callback_ = request_restarted.callback();

  authenticator->MakeCredential(std::move(options), base::DoNothing());
  request_started.WaitForCallback();
  request_restarted.WaitForCallback();
}

TEST_F(AuthenticatorContentBrowserClientTest, GetAssertionStartOver) {
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();

  TestRequestStartedCallback request_started;
  test_client_.action_callbacks_registered_callback =
      request_started.callback();
  TestRequestStartedCallback request_restarted;
  test_client_.started_over_callback_ = request_restarted.callback();

  authenticator->GetAssertion(std::move(options), base::DoNothing());
  request_started.WaitForCallback();
  request_restarted.WaitForCallback();
}

TEST_F(AuthenticatorContentBrowserClientTest, Unfocused) {
  // When the |ContentBrowserClient| considers the tab to be unfocused,
  // registration requests should fail with a |NOT_FOCUSED| error, but getting
  // assertions should still work.
  test_client_.GetTestWebAuthenticationDelegate()->is_focused = false;

  NavigateAndCommit(GURL(kTestOrigin1));

  {
    TestRequestStartedCallback request_started;
    test_client_.action_callbacks_registered_callback =
        request_started.callback();

    EXPECT_EQ(
        AuthenticatorMakeCredential(GetTestPublicKeyCredentialCreationOptions())
            .status,
        AuthenticatorStatus::NOT_FOCUSED);
    EXPECT_FALSE(request_started.was_called());
  }

  {
    device::PublicKeyCredentialDescriptor credential;
    credential.SetCredentialTypeForTesting(device::CredentialType::kPublicKey);
    credential.GetIdForTesting().resize(16);
    credential.GetTransportsForTesting() = {
        device::FidoTransportProtocol::kUsbHumanInterfaceDevice};

    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        credential.id(), kTestRelyingPartyId));
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->allow_credentials.emplace_back(credential);

    TestRequestStartedCallback request_started;
    test_client_.action_callbacks_registered_callback =
        request_started.callback();

    EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
              AuthenticatorStatus::SUCCESS);
    EXPECT_TRUE(request_started.was_called());
  }
}

TEST_F(AuthenticatorContentBrowserClientTest,
       NullDelegate_RejectsWithPendingRequest) {
  test_client_.return_null_delegate = true;
  NavigateAndCommit(GURL(kTestOrigin1));

  {
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
              AuthenticatorStatus::PENDING_REQUEST);
  }

  {
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
              AuthenticatorStatus::PENDING_REQUEST);
  }
}

TEST_F(AuthenticatorContentBrowserClientTest, IsUVPAAOverride) {
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  for (const bool is_uvpaa : {false, true}) {
    SCOPED_TRACE(::testing::Message() << "is_uvpaa=" << is_uvpaa);
    test_client_.GetTestWebAuthenticationDelegate()->is_uvpaa_override =
        is_uvpaa;

    TestIsUvpaaCallback cb;
    authenticator->IsUserVerifyingPlatformAuthenticatorAvailable(cb.callback());
    cb.WaitForCallback();
    EXPECT_EQ(is_uvpaa, cb.value());
  }
}

TEST_F(AuthenticatorContentBrowserClientTest,
       CryptotokenBypassesAttestationConsentPrompt) {
  NavigateAndCommit(GURL(kCryptotokenOrigin));

  virtual_device_factory_->SetSupportedProtocol(device::ProtocolVersion::kU2f);
  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  // Despite the direct attestation conveyance preference, the request delegate
  // is not asked for attestation consent. Hence the request will succeed,
  // despite the handler denying all attestation consent prompts.
  options->attestation = device::AttestationConveyancePreference::kDirect;
  test_client_.attestation_consent = AttestationConsent::NOT_USED;

  EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
            AuthenticatorStatus::SUCCESS);
}

TEST_F(AuthenticatorContentBrowserClientTest,
       CableCredentialWithoutCableExtension) {
  // Exercise the case where a credential is marked as "cable" but no caBLE
  // extension is provided. The AuthenticatorRequestClientDelegate should see no
  // transports, which triggers it to cancel the request. (Outside of a testing
  // environment, Chrome's AuthenticatorRequestClientDelegate will show an
  // informative error and wait for the user to cancel the request.)
  EnableFeature(features::kWebAuthCable);
  NavigateAndCommit(GURL(kTestOrigin1));

  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  std::vector<uint8_t> id(32u, 1u);
  base::flat_set<device::FidoTransportProtocol> transports{
      device::FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy};
  options->allow_credentials.clear();
  options->allow_credentials.emplace_back(device::CredentialType::kPublicKey,
                                          std::move(id), std::move(transports));

  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

class MockAuthenticatorRequestDelegateObserver
    : public TestAuthenticatorRequestDelegate {
 public:
  using InterestingFailureReasonCallback =
      base::OnceCallback<void(InterestingFailureReason)>;

  explicit MockAuthenticatorRequestDelegateObserver(
      InterestingFailureReasonCallback failure_reasons_callback =
          base::DoNothing())
      : TestAuthenticatorRequestDelegate(
            nullptr /* render_frame_host */,
            base::DoNothing() /* did_start_request_callback */,
            AttestationConsent::NOT_USED,
            /*started_over_callback=*/base::OnceClosure()),
        failure_reasons_callback_(std::move(failure_reasons_callback)) {}
  ~MockAuthenticatorRequestDelegateObserver() override = default;

  bool DoesBlockRequestOnFailure(InterestingFailureReason reason) override {
    CHECK(failure_reasons_callback_);
    std::move(failure_reasons_callback_).Run(reason);
    return false;
  }

  MOCK_METHOD1(
      OnTransportAvailabilityEnumerated,
      void(device::FidoRequestHandlerBase::TransportAvailabilityInfo data));
  MOCK_METHOD1(EmbedderControlsAuthenticatorDispatch,
               bool(const device::FidoAuthenticator&));
  MOCK_METHOD1(FidoAuthenticatorAdded, void(const device::FidoAuthenticator&));
  MOCK_METHOD1(FidoAuthenticatorRemoved, void(base::StringPiece));

 private:
  InterestingFailureReasonCallback failure_reasons_callback_;

  DISALLOW_COPY_AND_ASSIGN(MockAuthenticatorRequestDelegateObserver);
};

// Fake test construct that shares all other behavior with AuthenticatorCommon
// except that:
//  - FakeAuthenticatorCommon does not trigger UI activity.
//  - MockAuthenticatorRequestDelegateObserver is injected to
//  |request_delegate_|
//    instead of ChromeAuthenticatorRequestDelegate.
class FakeAuthenticatorCommon : public AuthenticatorCommon {
 public:
  explicit FakeAuthenticatorCommon(
      RenderFrameHost* render_frame_host,
      std::unique_ptr<MockAuthenticatorRequestDelegateObserver> mock_delegate)
      : AuthenticatorCommon(render_frame_host),
        mock_delegate_(std::move(mock_delegate)) {}
  ~FakeAuthenticatorCommon() override = default;

  std::unique_ptr<AuthenticatorRequestClientDelegate>
  MaybeCreateRequestDelegate() override {
    DCHECK(mock_delegate_);
    return std::move(mock_delegate_);
  }

 private:
  friend class AuthenticatorImplRequestDelegateTest;

  std::unique_ptr<MockAuthenticatorRequestDelegateObserver> mock_delegate_;
};

class AuthenticatorImplRequestDelegateTest : public AuthenticatorImplTest {
 public:
  AuthenticatorImplRequestDelegateTest() = default;
  ~AuthenticatorImplRequestDelegateTest() override = default;

  mojo::Remote<blink::mojom::Authenticator> ConnectToFakeAuthenticator(
      std::unique_ptr<MockAuthenticatorRequestDelegateObserver> delegate) {
    mojo::Remote<blink::mojom::Authenticator> authenticator;
    // AuthenticatorImpl owns itself. It self-destructs when the RenderFrameHost
    // navigates or is deleted.
    new AuthenticatorImpl(main_rfh(),
                          authenticator.BindNewPipeAndPassReceiver(),
                          std::make_unique<FakeAuthenticatorCommon>(
                              main_rfh(), std::move(delegate)));
    return authenticator;
  }
};

TEST_F(AuthenticatorImplRequestDelegateTest,
       TestRequestDelegateObservesFidoRequestHandler) {
  EXPECT_CALL(*mock_adapter_, IsPresent())
      .WillRepeatedly(::testing::Return(true));

  auto discovery_factory =
      std::make_unique<device::test::FakeFidoDiscoveryFactory>();
  auto* fake_hid_discovery = discovery_factory->ForgeNextHidDiscovery();
  AuthenticatorEnvironmentImpl::GetInstance()
      ->ReplaceDefaultDiscoveryFactoryForTesting(std::move(discovery_factory));

  NavigateAndCommit(GURL(kTestOrigin1));
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  TestGetAssertionCallback callback_receiver;

  auto mock_delegate =
      std::make_unique<MockAuthenticatorRequestDelegateObserver>();
  auto* const mock_delegate_ptr = mock_delegate.get();
  auto authenticator = ConnectToFakeAuthenticator(std::move(mock_delegate));

  auto mock_usb_device = device::MockFidoDevice::MakeCtap();
  mock_usb_device->StubGetId();
  mock_usb_device->SetDeviceTransport(
      device::FidoTransportProtocol::kUsbHumanInterfaceDevice);
  const auto device_id = mock_usb_device->GetId();

  EXPECT_CALL(*mock_delegate_ptr, OnTransportAvailabilityEnumerated(_));
  EXPECT_CALL(*mock_delegate_ptr, EmbedderControlsAuthenticatorDispatch(_))
      .WillOnce(testing::Return(true));

  base::RunLoop usb_device_found_done;
  EXPECT_CALL(*mock_delegate_ptr, FidoAuthenticatorAdded(_))
      .WillOnce(testing::InvokeWithoutArgs(
          [&usb_device_found_done]() { usb_device_found_done.Quit(); }));

  base::RunLoop usb_device_lost_done;
  EXPECT_CALL(*mock_delegate_ptr, FidoAuthenticatorRemoved(_))
      .WillOnce(testing::InvokeWithoutArgs(
          [&usb_device_lost_done]() { usb_device_lost_done.Quit(); }));

  authenticator->GetAssertion(std::move(options), callback_receiver.callback());
  fake_hid_discovery->WaitForCallToStartAndSimulateSuccess();
  fake_hid_discovery->AddDevice(std::move(mock_usb_device));
  usb_device_found_done.Run();

  fake_hid_discovery->RemoveDevice(device_id);
  usb_device_lost_done.Run();
  base::RunLoop().RunUntilIdle();
}

TEST_F(AuthenticatorImplRequestDelegateTest, FailureReasonForTimeout) {
  // The VirtualFidoAuthenticator simulates a tap immediately after it gets the
  // request. Replace by the real discovery that will wait until timeout.
  AuthenticatorEnvironmentImpl::GetInstance()
      ->ReplaceDefaultDiscoveryFactoryForTesting(
          std::make_unique<device::FidoDiscoveryFactory>());
  NavigateAndCommit(GURL(kTestOrigin1));

  FailureReasonCallbackReceiver failure_reason_receiver;
  auto mock_delegate = std::make_unique<
      ::testing::NiceMock<MockAuthenticatorRequestDelegateObserver>>(
      failure_reason_receiver.callback());
  auto authenticator = ConnectToFakeAuthenticator(std::move(mock_delegate));

  TestGetAssertionCallback callback_receiver;
  authenticator->GetAssertion(GetTestPublicKeyCredentialRequestOptions(),
                              callback_receiver.callback());

  task_environment()->FastForwardBy(kTestTimeout);

  callback_receiver.WaitForCallback();
  EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, callback_receiver.status());

  ASSERT_TRUE(failure_reason_receiver.was_called());
  EXPECT_EQ(
      AuthenticatorRequestClientDelegate::InterestingFailureReason::kTimeout,
      std::get<0>(*failure_reason_receiver.result()));
}

TEST_F(AuthenticatorImplRequestDelegateTest,
       FailureReasonForDuplicateRegistration) {
  NavigateAndCommit(GURL(kTestOrigin1));

  FailureReasonCallbackReceiver failure_reason_receiver;
  auto mock_delegate = std::make_unique<
      ::testing::NiceMock<MockAuthenticatorRequestDelegateObserver>>(
      failure_reason_receiver.callback());
  auto authenticator = ConnectToFakeAuthenticator(std::move(mock_delegate));

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->exclude_credentials = GetTestCredentials();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->exclude_credentials[0].id(), kTestRelyingPartyId));

  TestMakeCredentialCallback callback_receiver;
  authenticator->MakeCredential(std::move(options),
                                callback_receiver.callback());

  callback_receiver.WaitForCallback();
  EXPECT_EQ(AuthenticatorStatus::CREDENTIAL_EXCLUDED,
            callback_receiver.status());

  ASSERT_TRUE(failure_reason_receiver.was_called());
  EXPECT_EQ(AuthenticatorRequestClientDelegate::InterestingFailureReason::
                kKeyAlreadyRegistered,
            std::get<0>(*failure_reason_receiver.result()));
}

TEST_F(AuthenticatorImplRequestDelegateTest,
       FailureReasonForMissingRegistration) {
  NavigateAndCommit(GURL(kTestOrigin1));

  FailureReasonCallbackReceiver failure_reason_receiver;
  auto mock_delegate = std::make_unique<
      ::testing::NiceMock<MockAuthenticatorRequestDelegateObserver>>(
      failure_reason_receiver.callback());
  auto authenticator = ConnectToFakeAuthenticator(std::move(mock_delegate));

  TestGetAssertionCallback callback_receiver;
  authenticator->GetAssertion(GetTestPublicKeyCredentialRequestOptions(),
                              callback_receiver.callback());

  callback_receiver.WaitForCallback();
  EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, callback_receiver.status());

  ASSERT_TRUE(failure_reason_receiver.was_called());
  EXPECT_EQ(AuthenticatorRequestClientDelegate::InterestingFailureReason::
                kKeyNotRegistered,
            std::get<0>(*failure_reason_receiver.result()));
}

TEST_F(AuthenticatorImplTest, Transports) {
  NavigateAndCommit(GURL(kTestOrigin1));

  for (auto protocol :
       {device::ProtocolVersion::kU2f, device::ProtocolVersion::kCtap2}) {
    SCOPED_TRACE(static_cast<int>(protocol));
    virtual_device_factory_->SetSupportedProtocol(protocol);

    for (const auto transport : std::map<device::FidoTransportProtocol,
                                         blink::mojom::AuthenticatorTransport>(
             {{device::FidoTransportProtocol::kUsbHumanInterfaceDevice,
               blink::mojom::AuthenticatorTransport::USB},
              {device::FidoTransportProtocol::kBluetoothLowEnergy,
               blink::mojom::AuthenticatorTransport::BLE},
              {device::FidoTransportProtocol::kNearFieldCommunication,
               blink::mojom::AuthenticatorTransport::NFC}})) {
      virtual_device_factory_->SetTransport(transport.first);

      MakeCredentialResult result = AuthenticatorMakeCredential();
      EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);

      const std::vector<device::FidoTransportProtocol>& transports(
          result.response->transports);
      ASSERT_EQ(1u, transports.size());
      EXPECT_EQ(transport.first, transports[0]);
    }
  }
}

TEST_F(AuthenticatorImplTest, ExtensionHMACSecret) {
  NavigateAndCommit(GURL(kTestOrigin1));

  for (const bool include_extension : {false, true}) {
    for (const bool authenticator_support : {false, true}) {
      SCOPED_TRACE(include_extension);
      SCOPED_TRACE(authenticator_support);

      device::VirtualCtap2Device::Config config;
      config.hmac_secret_support = authenticator_support;
      virtual_device_factory_->SetCtap2Config(config);

      PublicKeyCredentialCreationOptionsPtr options =
          GetTestPublicKeyCredentialCreationOptions();
      options->hmac_create_secret = include_extension;
      MakeCredentialResult result =
          AuthenticatorMakeCredential(std::move(options));
      EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);

      device::AuthenticatorData parsed_auth_data =
          AuthDataFromMakeCredentialResponse(result.response);

      // The virtual CTAP2 device always echos the hmac-secret extension on
      // registrations. Therefore, if |hmac_secret| was set above it should be
      // serialised in the CBOR and correctly passed all the way back around to
      // the reply's authenticator data.
      bool has_hmac_secret = false;
      const auto& extensions = parsed_auth_data.extensions();
      if (extensions) {
        CHECK(extensions->is_map());
        const cbor::Value::MapValue& extensions_map = extensions->GetMap();
        const auto hmac_secret_it =
            extensions_map.find(cbor::Value(device::kExtensionHmacSecret));
        if (hmac_secret_it != extensions_map.end()) {
          ASSERT_TRUE(hmac_secret_it->second.is_bool());
          EXPECT_TRUE(hmac_secret_it->second.GetBool());
          has_hmac_secret = true;
        }
      }

      EXPECT_EQ(include_extension && authenticator_support, has_hmac_secret);
    }
  }
}

// Tests that for an authenticator that does not support batching, credential
// lists get probed silently to work around authenticators rejecting exclude
// lists exceeding a certain size.
TEST_F(AuthenticatorImplTest, MakeCredentialWithLargeExcludeList) {
  NavigateAndCommit(GURL(kTestOrigin1));

  for (bool has_excluded_credential : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "has_excluded_credential=" << has_excluded_credential);

    ResetVirtualDevice();
    device::VirtualCtap2Device::Config config;
    config.reject_large_allow_and_exclude_lists = true;
    virtual_device_factory_->SetCtap2Config(config);

    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->exclude_credentials = GetTestCredentials(/*num_credentials=*/10);
    if (has_excluded_credential) {
      ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
          options->exclude_credentials.back().id(), kTestRelyingPartyId));
    }

    EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
              has_excluded_credential ? AuthenticatorStatus::CREDENTIAL_EXCLUDED
                                      : AuthenticatorStatus::SUCCESS);
  }
}

// Tests that for an authenticator that does not support batching, credential
// lists get probed silently to work around authenticators rejecting allow lists
// exceeding a certain size.
TEST_F(AuthenticatorImplTest, GetAssertionWithLargeAllowList) {
  NavigateAndCommit(GURL(kTestOrigin1));

  for (bool has_allowed_credential : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "has_allowed_credential=" << has_allowed_credential);

    ResetVirtualDevice();
    device::VirtualCtap2Device::Config config;
    config.reject_large_allow_and_exclude_lists = true;
    virtual_device_factory_->SetCtap2Config(config);

    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->allow_credentials = GetTestCredentials(/*num_credentials=*/10);
    if (has_allowed_credential) {
      ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
          options->allow_credentials.back().id(), kTestRelyingPartyId));
    }

    EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
              has_allowed_credential ? AuthenticatorStatus::SUCCESS
                                     : AuthenticatorStatus::NOT_ALLOWED_ERROR);
  }
}

// Tests that, regardless of batching support, GetAssertion requests with a
// single allowed credential ID don't result in a silent probing request.
TEST_F(AuthenticatorImplTest, GetAssertionSingleElementAllowListDoesNotProbe) {
  NavigateAndCommit(GURL(kTestOrigin1));

  for (bool supports_batching : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "supports_batching=" << supports_batching);

    ResetVirtualDevice();
    device::VirtualCtap2Device::Config config;
    if (supports_batching) {
      config.max_credential_id_length = kTestCredentialIdLength;
      config.max_credential_count_in_list = 10;
    }
    config.reject_silent_authentication_requests = true;
    virtual_device_factory_->SetCtap2Config(config);

    auto test_credentials = GetTestCredentials(/*num_credentials=*/1);
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        test_credentials.front().id(), kTestRelyingPartyId));

    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->allow_credentials = std::move(test_credentials);

    EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
              AuthenticatorStatus::SUCCESS);
  }
}

// Tests that an allow list that fits into a single batch does not result in a
// silent probing request.
TEST_F(AuthenticatorImplTest, GetAssertionSingleBatchListDoesNotProbe) {
  NavigateAndCommit(GURL(kTestOrigin1));

  for (bool allow_list_fits_single_batch : {false, true}) {
    SCOPED_TRACE(::testing::Message() << "allow_list_fits_single_batch="
                                      << allow_list_fits_single_batch);

    ResetVirtualDevice();
    device::VirtualCtap2Device::Config config;
    config.max_credential_id_length = kTestCredentialIdLength;
    constexpr size_t kBatchSize = 10;
    config.max_credential_count_in_list = kBatchSize;
    config.reject_silent_authentication_requests = true;
    virtual_device_factory_->SetCtap2Config(config);

    auto test_credentials = GetTestCredentials(
        /*num_credentials=*/kBatchSize +
        (allow_list_fits_single_batch ? 0 : 1));
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        test_credentials.back().id(), kTestRelyingPartyId));

    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->allow_credentials = std::move(test_credentials);

    EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
              allow_list_fits_single_batch
                  ? AuthenticatorStatus::SUCCESS
                  : AuthenticatorStatus::NOT_ALLOWED_ERROR);
  }
}

TEST_F(AuthenticatorImplTest, OptionalCredentialInAssertionResponse) {
  // This test exercises the unfortunate optionality in the CTAP2 spec r.e.
  // whether an authenticator returns credential information when the allowlist
  // only has a single entry.
  NavigateAndCommit(GURL(kTestOrigin1));

  for (const auto behavior :
       {device::VirtualCtap2Device::Config::IncludeCredential::ONLY_IF_NEEDED,
        device::VirtualCtap2Device::Config::IncludeCredential::ALWAYS,
        device::VirtualCtap2Device::Config::IncludeCredential::NEVER}) {
    SCOPED_TRACE(static_cast<int>(behavior));

    ResetVirtualDevice();
    device::VirtualCtap2Device::Config config;
    config.include_credential_in_assertion_response = behavior;
    config.max_credential_count_in_list = 10;
    config.max_credential_id_length = 256;
    virtual_device_factory_->SetCtap2Config(config);

    size_t num_credentials;
    bool should_timeout = false;
    switch (behavior) {
      case device::VirtualCtap2Device::Config::IncludeCredential::
          ONLY_IF_NEEDED:
        // The behaviour to test for |ONLY_IF_NEEDED| is that an omitted
        // credential in the response is handled correctly.
        num_credentials = 1;
        break;
      case device::VirtualCtap2Device::Config::IncludeCredential::ALWAYS:
        // Also test that a technically-superfluous credential in the response
        // is handled.
        num_credentials = 1;
        break;
      case device::VirtualCtap2Device::Config::IncludeCredential::NEVER:
        // Test that omitting a credential in an ambiguous context causes a
        // failure.
        num_credentials = 2;
        should_timeout = true;
        break;
    }

    auto test_credentials = GetTestCredentials(num_credentials);
    for (const auto& cred : test_credentials) {
      ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
          cred.id(), kTestRelyingPartyId));
    }

    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->allow_credentials = std::move(test_credentials);

    if (should_timeout) {
      EXPECT_EQ(
          AuthenticatorGetAssertionAndWaitForTimeout(std::move(options)).status,
          AuthenticatorStatus::NOT_ALLOWED_ERROR);
    } else {
      EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
                AuthenticatorStatus::SUCCESS);
    }
  }
}

// Tests that an allowList with only credential IDs of a length exceeding the
// maxCredentialIdLength parameter is not mistakenly interpreted as an empty
// allow list.
TEST_F(AuthenticatorImplTest, AllowListWithOnlyOversizedCredentialIds) {
  NavigateAndCommit(GURL(kTestOrigin1));

  device::VirtualCtap2Device::Config config;
  config.u2f_support = true;
  config.max_credential_id_length = kTestCredentialIdLength;
  config.max_credential_count_in_list = 10;
  virtual_device_factory_->SetCtap2Config(config);

  const std::vector<uint8_t> cred_id(kTestCredentialIdLength + 1, 0);
  // Inject registration so that the test will fail (because of a successful
  // response) if the oversized credential ID is sent.
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      cred_id, kTestRelyingPartyId));

  for (const bool has_app_id : {false, true}) {
    SCOPED_TRACE(has_app_id);
    virtual_device_factory_->mutable_state()->allow_list_sizes.clear();

    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    if (has_app_id) {
      options->appid = kTestOrigin1;
    }
    options->allow_credentials = {device::PublicKeyCredentialDescriptor(
        device::CredentialType::kPublicKey, cred_id)};

    EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
              AuthenticatorStatus::NOT_ALLOWED_ERROR);
    const auto& allow_list_sizes =
        virtual_device_factory_->mutable_state()->allow_list_sizes;
    // No empty allow-list requests should have been made.
    EXPECT_TRUE(std::none_of(allow_list_sizes.cbegin(), allow_list_sizes.cend(),
                             [](size_t size) { return size == 0; }));
  }
}

TEST_F(AuthenticatorImplTest, NoUnexpectedAuthenticatorExtensions) {
  NavigateAndCommit(GURL(kTestOrigin1));

  device::VirtualCtap2Device::Config config;
  config.add_extra_extension = true;
  virtual_device_factory_->SetCtap2Config(config);

  // Check that extra authenticator extensions are rejected when creating a
  // credential.
  EXPECT_EQ(AuthenticatorMakeCredential().status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);

  // Extensions should also be rejected when getting an assertion.
  PublicKeyCredentialRequestOptionsPtr assertion_options =
      GetTestPublicKeyCredentialRequestOptions();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      assertion_options->allow_credentials.back().id(), kTestRelyingPartyId));
  EXPECT_EQ(AuthenticatorGetAssertion(std::move(assertion_options)).status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

TEST_F(AuthenticatorImplTest, NoUnexpectedClientExtensions) {
  NavigateAndCommit(GURL(kTestOrigin1));

  device::VirtualCtap2Device::Config config;
  config.reject_all_extensions = true;
  virtual_device_factory_->SetCtap2Config(config);

  // Check that no unexpected client extensions are sent to the authenticator.
  EXPECT_EQ(AuthenticatorMakeCredential().status, AuthenticatorStatus::SUCCESS);

  // No extensions should be sent when getting an assertion either.
  PublicKeyCredentialRequestOptionsPtr assertion_options =
      GetTestPublicKeyCredentialRequestOptions();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      assertion_options->allow_credentials.back().id(), kTestRelyingPartyId));
  EXPECT_EQ(AuthenticatorGetAssertion(std::move(assertion_options)).status,
            AuthenticatorStatus::SUCCESS);
}

// Tests that on an authenticator that supports batching, exclude lists that fit
// into a single batch are sent without probing.
TEST_F(AuthenticatorImplTest, ExcludeListBatching) {
  NavigateAndCommit(GURL(kTestOrigin1));

  for (bool authenticator_has_excluded_credential : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "authenticator_has_excluded_credential="
                 << authenticator_has_excluded_credential);

    ResetVirtualDevice();
    device::VirtualCtap2Device::Config config;
    config.max_credential_id_length = kTestCredentialIdLength;
    constexpr size_t kBatchSize = 10;
    config.max_credential_count_in_list = kBatchSize;
    // Reject silent authentication requests to ensure we are not probing
    // credentials silently, since the exclude list should fit into a single
    // batch.
    config.reject_silent_authentication_requests = true;
    virtual_device_factory_->SetCtap2Config(config);

    auto test_credentials = GetTestCredentials(kBatchSize);
    test_credentials.insert(
        test_credentials.end() - 1,
        {device::CredentialType::kPublicKey,
         std::vector<uint8_t>(kTestCredentialIdLength + 1, 1)});
    if (authenticator_has_excluded_credential) {
      ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
          test_credentials.back().id(), kTestRelyingPartyId));
    }

    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->exclude_credentials = std::move(test_credentials);
    EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
              authenticator_has_excluded_credential
                  ? AuthenticatorStatus::CREDENTIAL_EXCLUDED
                  : AuthenticatorStatus::SUCCESS);
  }
}

TEST_F(AuthenticatorImplTest, GetPublicKey) {
  NavigateAndCommit(GURL(kTestOrigin1));

  static constexpr struct {
    device::CoseAlgorithmIdentifier algo;
    absl::optional<int> evp_id;
  } kTests[] = {
      {device::CoseAlgorithmIdentifier::kEs256, EVP_PKEY_EC},
      {device::CoseAlgorithmIdentifier::kRs256, EVP_PKEY_RSA},
      {device::CoseAlgorithmIdentifier::kEdDSA, EVP_PKEY_ED25519},
      {device::CoseAlgorithmIdentifier::kInvalidForTesting, absl::nullopt},
  };

  std::vector<device::CoseAlgorithmIdentifier> advertised_algorithms;
  for (const auto& test : kTests) {
    advertised_algorithms.push_back(test.algo);
  }

  device::VirtualCtap2Device::Config config;
  config.advertised_algorithms = std::move(advertised_algorithms);
  virtual_device_factory_->SetCtap2Config(config);

  for (const auto& test : kTests) {
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->public_key_parameters =
        GetTestPublicKeyCredentialParameters(static_cast<int32_t>(test.algo));

    MakeCredentialResult result =
        AuthenticatorMakeCredential(std::move(options));
    ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    const auto& response = result.response;
    EXPECT_EQ(response->public_key_algo, static_cast<int32_t>(test.algo));
    EXPECT_FALSE(response->info->authenticator_data.empty());

    ASSERT_EQ(test.evp_id.has_value(), response->public_key_der.has_value());
    if (!test.evp_id) {
      continue;
    }

    const std::vector<uint8_t>& public_key_der =
        response->public_key_der.value();

    CBS cbs;
    CBS_init(&cbs, public_key_der.data(), public_key_der.size());
    bssl::UniquePtr<EVP_PKEY> pkey(EVP_parse_public_key(&cbs));
    EXPECT_EQ(0u, CBS_len(&cbs));
    ASSERT_TRUE(pkey.get());

    EXPECT_EQ(test.evp_id.value(), EVP_PKEY_id(pkey.get()));
  }
}

TEST_F(AuthenticatorImplTest, AlgorithmsOmitted) {
  // Some CTAP 2.0 security keys shipped support for algorithms other than
  // ECDSA P-256 but the algorithms field didn't exist then. makeCredential
  // requests should get routed to them anyway.

  device::VirtualCtap2Device::Config config;
  // Remove the algorithms field from the getInfo.
  config.advertised_algorithms.clear();
  virtual_device_factory_->SetCtap2Config(config);
  NavigateAndCommit(GURL(kTestOrigin1));

  // Test that an Ed25519 credential can still be created. (The virtual
  // authenticator supports that algorithm.)
  {
    const int32_t algo =
        static_cast<int32_t>(device::CoseAlgorithmIdentifier::kEdDSA);
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->public_key_parameters = GetTestPublicKeyCredentialParameters(algo);
    MakeCredentialResult result =
        AuthenticatorMakeCredential(std::move(options));
    ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    const auto& response = result.response;
    EXPECT_EQ(response->public_key_algo, algo);
  }

  // Test that requesting an unsupported algorithm still collects a touch.
  {
    bool touched = false;
    virtual_device_factory_->mutable_state()->simulate_press_callback =
        base::BindLambdaForTesting([&](device::VirtualFidoDevice* device) {
          touched = true;
          return true;
        });

    const int32_t algo = static_cast<int32_t>(
        device::CoseAlgorithmIdentifier::kInvalidForTesting);
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->public_key_parameters = GetTestPublicKeyCredentialParameters(algo);
    MakeCredentialResult result =
        AuthenticatorMakeCredential(std::move(options));
    EXPECT_EQ(result.status, AuthenticatorStatus::NOT_ALLOWED_ERROR);
    EXPECT_TRUE(touched);
  }
}

TEST_F(AuthenticatorImplTest, VirtualAuthenticatorPublicKeyAlgos) {
  // Exercise all the public key types in the virtual authenticator for create()
  // and get().
  NavigateAndCommit(GURL(kTestOrigin1));

  static const struct {
    device::CoseAlgorithmIdentifier algo;
    const EVP_MD* digest;
  } kTests[] = {
      {device::CoseAlgorithmIdentifier::kEs256, EVP_sha256()},
      {device::CoseAlgorithmIdentifier::kRs256, EVP_sha256()},
      {device::CoseAlgorithmIdentifier::kEdDSA, nullptr},
  };

  std::vector<device::CoseAlgorithmIdentifier> advertised_algorithms;
  for (const auto& test : kTests) {
    advertised_algorithms.push_back(test.algo);
  }

  device::VirtualCtap2Device::Config config;
  config.advertised_algorithms = std::move(advertised_algorithms);
  virtual_device_factory_->SetCtap2Config(config);

  for (const auto& test : kTests) {
    SCOPED_TRACE(static_cast<int>(test.algo));

    PublicKeyCredentialCreationOptionsPtr create_options =
        GetTestPublicKeyCredentialCreationOptions();
    create_options->public_key_parameters =
        GetTestPublicKeyCredentialParameters(static_cast<int32_t>(test.algo));

    MakeCredentialResult create_result =
        AuthenticatorMakeCredential(std::move(create_options));
    ASSERT_EQ(create_result.status, AuthenticatorStatus::SUCCESS);
    EXPECT_EQ(create_result.response->public_key_algo,
              static_cast<int32_t>(test.algo));

    const std::vector<uint8_t>& public_key_der =
        create_result.response->public_key_der.value();
    CBS cbs;
    CBS_init(&cbs, public_key_der.data(), public_key_der.size());
    bssl::UniquePtr<EVP_PKEY> pkey(EVP_parse_public_key(&cbs));
    EXPECT_EQ(0u, CBS_len(&cbs));
    ASSERT_TRUE(pkey.get());

    PublicKeyCredentialRequestOptionsPtr get_options =
        GetTestPublicKeyCredentialRequestOptions();
    device::PublicKeyCredentialDescriptor public_key(
        device::CredentialType::kPublicKey,
        create_result.response->info->raw_id,
        {device::FidoTransportProtocol::kUsbHumanInterfaceDevice});
    get_options->allow_credentials = {std::move(public_key)};
    GetAssertionResult get_result =
        AuthenticatorGetAssertion(std::move(get_options));
    ASSERT_EQ(get_result.status, AuthenticatorStatus::SUCCESS);
    base::span<const uint8_t> signature(get_result.response->signature);
    std::vector<uint8_t> signed_data(
        get_result.response->info->authenticator_data);
    const std::array<uint8_t, crypto::kSHA256Length> client_data_json_hash(
        crypto::SHA256Hash(get_result.response->info->client_data_json));
    signed_data.insert(signed_data.end(), client_data_json_hash.begin(),
                       client_data_json_hash.end());

    bssl::ScopedEVP_MD_CTX md_ctx;
    ASSERT_EQ(EVP_DigestVerifyInit(md_ctx.get(), /*pctx=*/nullptr, test.digest,
                                   /*e=*/nullptr, pkey.get()),
              1);
    EXPECT_EQ(EVP_DigestVerify(md_ctx.get(), signature.data(), signature.size(),
                               signed_data.data(), signed_data.size()),
              1);
  }
}

TEST_F(AuthenticatorImplTest, ResetDiscoveryFactoryOverride) {
  // This is a regression test for crbug.com/1087158.
  NavigateAndCommit(GURL(kTestOrigin1));

  // Make the entire discovery factory disappear mid-request.
  bool was_called = false;
  virtual_device_factory_->SetSupportedProtocol(
      device::ProtocolVersion::kCtap2);
  virtual_device_factory_->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting([&](device::VirtualFidoDevice* device) {
        was_called = true;
        ResetVirtualDevice();
        return false;
      });

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  EXPECT_EQ(
      AuthenticatorMakeCredentialAndWaitForTimeout(std::move(options)).status,
      AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

TEST_F(AuthenticatorImplTest, InvalidU2FPublicKey) {
  NavigateAndCommit(GURL(kTestOrigin1));
  virtual_device_factory_->SetSupportedProtocol(device::ProtocolVersion::kU2f);
  virtual_device_factory_->mutable_state()->u2f_invalid_public_key = true;

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

TEST_F(AuthenticatorImplTest, InvalidU2FSignature) {
  NavigateAndCommit(GURL(kTestOrigin1));
  virtual_device_factory_->SetSupportedProtocol(device::ProtocolVersion::kU2f);
  virtual_device_factory_->mutable_state()->u2f_invalid_signature = true;

  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id(), kTestOrigin1));
  options->appid = kTestOrigin1;

  EXPECT_EQ(
      AuthenticatorGetAssertionAndWaitForTimeout(std::move(options)).status,
      AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

TEST_F(AuthenticatorImplTest, CredBlob) {
  NavigateAndCommit(GURL(kTestOrigin1));

  device::VirtualCtap2Device::Config config;
  config.cred_blob_support = true;
  // credProtect is required for credBlob per CTAP 2.1.
  config.cred_protect_support = true;
  virtual_device_factory_->SetCtap2Config(config);

  const std::vector<uint8_t> cred_blob = {1, 2, 3, 4};

  std::vector<uint8_t> credential_id;
  // Create a credential with a credBlob set.
  {
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->cred_blob = cred_blob;
    auto result = AuthenticatorMakeCredential(std::move(options));
    ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    credential_id = std::move(result.response->info->raw_id);
    EXPECT_TRUE(result.response->echo_cred_blob);
    EXPECT_TRUE(result.response->cred_blob);
  }

  // Expect to be able to fetch the credBlob with an assertion.
  {
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->allow_credentials[0] = device::PublicKeyCredentialDescriptor(
        device::CredentialType::kPublicKey, std::move(credential_id));
    options->get_cred_blob = true;

    auto result = AuthenticatorGetAssertion(std::move(options));
    ASSERT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    EXPECT_TRUE(result.response->echo_get_cred_blob);
    EXPECT_EQ(result.response->get_cred_blob, cred_blob);
  }
}

TEST_F(AuthenticatorImplTest, GoogleLegacyAppidSupport) {
  struct TestCase {
    std::string url;
    bool google_legacy_app_id_support;
    std::string rp_id;
    AuthenticatorStatus expected;
    enum { kU2f, kWebAuthn } credential_type;
    std::string application_parameter_url;
  };
  static const TestCase kTestCases[] = {
      // accounts.google.com can create regular WebAuthn credentials.
      {
          "https://accounts.google.com",
          false,
          "google.com",
          AuthenticatorStatus::SUCCESS,
          TestCase::kWebAuthn,
          "google.com",
      },
      // accounts.google.com can exercise googleLegacyAppidSupport to get a U2F
      // credential with a hard-coded appId.
      {
          "https://accounts.google.com",
          true,
          "google.com",
          AuthenticatorStatus::SUCCESS,
          TestCase::kU2f,
          "https://www.gstatic.com/securitykey/origins.json",
      },
      // login.corp.google.com also can make WebAuthn credentials.
      {
          "https://login.corp.google.com",
          false,
          "google.com",
          AuthenticatorStatus::SUCCESS,
          TestCase::kWebAuthn,
          "google.com",
      },
      // login.corp.google.com also can exercise googleLegacyAppidSupport,
      // yielding a different appId.
      {
          "https://login.corp.google.com",
          true,
          "google.com",
          AuthenticatorStatus::SUCCESS,
          TestCase::kU2f,
          "https://www.gstatic.com/securitykey/a/google.com/origins.json",
      },
      // On other origins, googleLegacyAppidSupport has no effect.
      {
          "https://example.com",
          true,
          "example.com",
          AuthenticatorStatus::SUCCESS,
          TestCase::kWebAuthn,
          "example.com",
      },
      // RP ID checks are still enforced with the extension set.
      {
          "https://accounts.google.com",
          true,
          "example.com",
          AuthenticatorStatus::BAD_RELYING_PARTY_ID,
      },
      {
          "https://example.com",
          true,
          "google.com",
          AuthenticatorStatus::BAD_RELYING_PARTY_ID,
      },
  };
  int i = 0;
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(testing::Message() << i++ << ": " << test_case.url << " "
                                    << test_case.google_legacy_app_id_support
                                    << " " << test_case.rp_id);
    ResetVirtualDevice();
    NavigateAndCommit(GURL(test_case.url));

    device::VirtualCtap2Device::Config config;
    config.u2f_support = true;
    virtual_device_factory_->SetCtap2Config(config);

    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->relying_party.id = test_case.rp_id;
    options->google_legacy_app_id_support =
        test_case.google_legacy_app_id_support;

    auto result = AuthenticatorMakeCredential(std::move(options));
    ASSERT_EQ(result.status, test_case.expected);

    if (test_case.expected != AuthenticatorStatus::SUCCESS) {
      continue;
    }

    ASSERT_EQ(virtual_device_factory_->mutable_state()->registrations.size(),
              1u);

    const std::string client_data_json(
        result.response->info->client_data_json.data(),
        result.response->info->client_data_json.data() +
            result.response->info->client_data_json.size());
    EXPECT_EQ(virtual_device_factory_->mutable_state()
                  ->registrations.begin()
                  ->second.is_u2f,
              test_case.credential_type == TestCase::kU2f);
    // Requests use the type key for WebAuthn rather than U2F API registration,
    // even if googleLegacyAppidSupport is set.
    EXPECT_TRUE(
        base::StartsWith(client_data_json, R"({"type":"webauthn.create")"))
        << client_data_json;
    EXPECT_EQ(virtual_device_factory_->mutable_state()
                  ->registrations.begin()
                  ->second.application_parameter,
              device::fido_parsing_utils::CreateSHA256Hash(
                  test_case.application_parameter_url));
  }
}

static constexpr char kTestPIN[] = "1234";
static constexpr char16_t kTestPIN16[] = u"1234";

class UVTestAuthenticatorClientDelegate
    : public AuthenticatorRequestClientDelegate {
 public:
  explicit UVTestAuthenticatorClientDelegate(bool* collected_pin,
                                             uint32_t* min_pin_length,
                                             bool* did_bio_enrollment,
                                             bool cancel_bio_enrollment)
      : collected_pin_(collected_pin),
        min_pin_length_(min_pin_length),
        did_bio_enrollment_(did_bio_enrollment),
        cancel_bio_enrollment_(cancel_bio_enrollment) {
    *collected_pin_ = false;
    *did_bio_enrollment_ = false;
  }

  bool SupportsPIN() const override { return true; }

  void CollectPIN(
      CollectPINOptions options,
      base::OnceCallback<void(std::u16string)> provide_pin_cb) override {
    *collected_pin_ = true;
    *min_pin_length_ = options.min_pin_length;
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(provide_pin_cb), kTestPIN16));
  }

  void StartBioEnrollment(base::OnceClosure next_callback) override {
    *did_bio_enrollment_ = true;
    if (cancel_bio_enrollment_) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, std::move(next_callback));
      return;
    }
    bio_callback_ = std::move(next_callback);
  }

  void OnSampleCollected(int remaining_samples) override {
    if (remaining_samples <= 0) {
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, std::move(bio_callback_));
    }
  }

  void FinishCollectToken() override {}

 private:
  bool* collected_pin_;
  uint32_t* min_pin_length_;
  base::OnceClosure bio_callback_;
  bool* did_bio_enrollment_;
  bool cancel_bio_enrollment_;
};

class UVTestAuthenticatorContentBrowserClient : public ContentBrowserClient {
 public:
  // ContentBrowserClient:
  WebAuthenticationDelegate* GetWebAuthenticationDelegate() override {
    return &web_authentication_delegate;
  }

  std::unique_ptr<AuthenticatorRequestClientDelegate>
  GetWebAuthenticationRequestDelegate(
      RenderFrameHost* render_frame_host) override {
    return std::make_unique<UVTestAuthenticatorClientDelegate>(
        &collected_pin, &min_pin_length, &did_bio_enrollment,
        cancel_bio_enrollment);
  }

  TestWebAuthenticationDelegate web_authentication_delegate;

  bool collected_pin;
  uint32_t min_pin_length = 0;
  bool did_bio_enrollment;
  bool cancel_bio_enrollment = false;
};

class UVAuthenticatorImplTest : public AuthenticatorImplTest {
 public:
  UVAuthenticatorImplTest() = default;

  void SetUp() override {
    AuthenticatorImplTest::SetUp();
    old_client_ = SetBrowserClientForTesting(&test_client_);
  }

  void TearDown() override {
    SetBrowserClientForTesting(old_client_);
    AuthenticatorImplTest::TearDown();
  }

 protected:
  static PublicKeyCredentialCreationOptionsPtr make_credential_options(
      device::UserVerificationRequirement uv =
          device::UserVerificationRequirement::kRequired,
      bool exclude_credentials = false,
      bool appid_exclude = false) {
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    if (exclude_credentials) {
      options->exclude_credentials = GetTestCredentials(/*num_credentials=*/1);
    }
    if (appid_exclude) {
      CHECK(exclude_credentials);
      options->appid_exclude = kTestOrigin1;
    }
    options->authenticator_selection->SetUserVerificationRequirementForTesting(
        uv);
    return options;
  }

  static PublicKeyCredentialRequestOptionsPtr get_credential_options(
      device::UserVerificationRequirement uv =
          device::UserVerificationRequirement::kRequired) {
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->user_verification = uv;
    return options;
  }

  static const char* UVToString(device::UserVerificationRequirement uv) {
    switch (uv) {
      case device::UserVerificationRequirement::kDiscouraged:
        return "discouraged";
      case device::UserVerificationRequirement::kPreferred:
        return "preferred";
      case device::UserVerificationRequirement::kRequired:
        return "required";
    }
  }

  static bool HasUV(const MakeCredentialAuthenticatorResponsePtr& response) {
    return AuthDataFromMakeCredentialResponse(response)
        .obtained_user_verification();
  }

  static bool HasUV(const GetAssertionAuthenticatorResponsePtr& response) {
    absl::optional<device::AuthenticatorData> auth_data =
        device::AuthenticatorData::DecodeAuthenticatorData(
            response->info->authenticator_data);
    return auth_data->obtained_user_verification();
  }

  UVTestAuthenticatorContentBrowserClient test_client_;

 private:
  ContentBrowserClient* old_client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(UVAuthenticatorImplTest);
};

using PINReason = device::pin::PINEntryReason;
using PINError = device::pin::PINEntryError;

// PINExpectation represent expected |mode|, |attempts|, |min_pin_length| and
// the PIN to answer with.
struct PINExpectation {
  PINReason reason;
  std::u16string pin;
  int attempts;
  uint32_t min_pin_length = device::kMinPinLength;
  PINError error = PINError::kNoError;
};

class PINTestAuthenticatorRequestDelegate
    : public AuthenticatorRequestClientDelegate {
 public:
  PINTestAuthenticatorRequestDelegate(
      bool supports_pin,
      const std::list<PINExpectation>& pins,
      absl::optional<InterestingFailureReason>* failure_reason)
      : supports_pin_(supports_pin),
        expected_(pins),
        failure_reason_(failure_reason) {}
  ~PINTestAuthenticatorRequestDelegate() override {
    DCHECK(expected_.empty())
        << expected_.size() << " unsatisifed PIN expectations";
  }

  bool SupportsPIN() const override { return supports_pin_; }

  void CollectPIN(
      CollectPINOptions options,
      base::OnceCallback<void(std::u16string)> provide_pin_cb) override {
    DCHECK(supports_pin_);
    DCHECK(!expected_.empty()) << "unexpected PIN request";
    if (expected_.front().reason == PINReason::kChallenge) {
      DCHECK(options.attempts == expected_.front().attempts)
          << "got: " << options.attempts
          << " expected: " << expected_.front().attempts;
    }
    DCHECK_EQ(expected_.front().min_pin_length, options.min_pin_length);
    DCHECK_EQ(expected_.front().reason, options.reason);
    DCHECK_EQ(expected_.front().error, options.error);
    std::u16string pin = std::move(expected_.front().pin);
    expected_.pop_front();

    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(provide_pin_cb), std::move(pin)));
  }

  void FinishCollectToken() override {}

  bool DoesBlockRequestOnFailure(InterestingFailureReason reason) override {
    *failure_reason_ = reason;
    return AuthenticatorRequestClientDelegate::DoesBlockRequestOnFailure(
        reason);
  }

 private:
  const bool supports_pin_;
  std::list<PINExpectation> expected_;
  absl::optional<InterestingFailureReason>* const failure_reason_;
  DISALLOW_COPY_AND_ASSIGN(PINTestAuthenticatorRequestDelegate);
};

class PINTestAuthenticatorContentBrowserClient : public ContentBrowserClient {
 public:
  // ContentBrowserClient:
  WebAuthenticationDelegate* GetWebAuthenticationDelegate() override {
    return &web_authentication_delegate;
  }

  std::unique_ptr<AuthenticatorRequestClientDelegate>
  GetWebAuthenticationRequestDelegate(
      RenderFrameHost* render_frame_host) override {
    return std::make_unique<PINTestAuthenticatorRequestDelegate>(
        supports_pin, expected, &failure_reason);
  }

  TestWebAuthenticationDelegate web_authentication_delegate;

  bool supports_pin = true;
  std::list<PINExpectation> expected;
  absl::optional<InterestingFailureReason> failure_reason;
};

class PINAuthenticatorImplTest : public UVAuthenticatorImplTest {
 public:
  PINAuthenticatorImplTest() = default;

  void SetUp() override {
    UVAuthenticatorImplTest::SetUp();
    old_client_ = SetBrowserClientForTesting(&test_client_);
    device::VirtualCtap2Device::Config config;
    config.pin_support = true;
    virtual_device_factory_->SetCtap2Config(config);
    NavigateAndCommit(GURL(kTestOrigin1));
  }

  void TearDown() override {
    SetBrowserClientForTesting(old_client_);
    UVAuthenticatorImplTest::TearDown();
  }

 protected:
  PINTestAuthenticatorContentBrowserClient test_client_;

  // An enumerate of outcomes for PIN tests.
  enum {
    kFailure,
    kNoPIN,
    kSetPIN,
    kUsePIN,
  };

  void ConfigureVirtualDevice(device::PINUVAuthProtocol pin_protocol,
                              bool pin_uv_auth_token,
                              int support_level) {
    device::VirtualCtap2Device::Config config;
    config.pin_protocol = pin_protocol;
    config.pin_uv_auth_token_support = pin_uv_auth_token;
    config.ctap2_versions = {device::Ctap2Version::kCtap2_0,
                             device::Ctap2Version::kCtap2_1};
    switch (support_level) {
      case 0:
        // No support.
        config.pin_support = false;
        virtual_device_factory_->mutable_state()->pin = "";
        virtual_device_factory_->mutable_state()->pin_retries = 0;
        break;

      case 1:
        // PIN supported, but no PIN set.
        config.pin_support = true;
        virtual_device_factory_->mutable_state()->pin = "";
        virtual_device_factory_->mutable_state()->pin_retries = 0;
        break;

      case 2:
        // PIN set.
        config.pin_support = true;
        virtual_device_factory_->mutable_state()->pin = kTestPIN;
        virtual_device_factory_->mutable_state()->pin_retries =
            device::kMaxPinRetries;
        break;

      default:
        NOTREACHED();
    }

    virtual_device_factory_->SetCtap2Config(config);
  }

 private:
  ContentBrowserClient* old_client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PINAuthenticatorImplTest);
};

static constexpr device::UserVerificationRequirement kUVLevel[3] = {
    device::UserVerificationRequirement::kDiscouraged,
    device::UserVerificationRequirement::kPreferred,
    device::UserVerificationRequirement::kRequired,
};

static const char* kUVDescription[3] = {"discouraged", "preferred", "required"};

static const char* kPINSupportDescription[3] = {"no PIN support", "PIN not set",
                                                "PIN set"};

TEST_F(PINAuthenticatorImplTest, MakeCredential) {
  typedef int Expectations[3][3];
  // kExpectedWithUISupport enumerates the expected behaviour when the embedder
  // supports prompting the user for a PIN.
  // clang-format off
  const Expectations kExpectedWithUISupport = {
    //                   discouraged | preferred | required
    /* No support */  {    kNoPIN,      kNoPIN,     kFailure },
    /* PIN not set */ {    kNoPIN,      kNoPIN,     kSetPIN  },
    /* PIN set */     {    kUsePIN,     kUsePIN,    kUsePIN  },
    //                        ^
    //                        |
    //            VirtualCtap2Device cannot fall back to U2F.
  };
  // clang-format on

  // kExpectedWithoutUISupport enumerates the expected behaviour when the
  // embedder cannot prompt the user for a PIN.
  // clang-format off
  const Expectations kExpectedWithoutUISupport = {
    //                   discouraged | preferred | required
    /* No support */  {    kNoPIN,      kNoPIN,     kFailure },
    /* PIN not set */ {    kNoPIN,      kNoPIN,     kFailure },
    /* PIN set */     {    kFailure,    kFailure,   kFailure },
    //                        ^            ^
    //                        |            |
    //            VirtualCtap2Device cannot fall back to U2F and
    //            a PIN is required to create credentials once set
    //            in CTAP 2.0.
  };
  // clang-format on

  for (bool pin_uv_auth_token : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "pin_uv_auth_token=" << pin_uv_auth_token);
    for (bool ui_support : {false, true}) {
      SCOPED_TRACE(::testing::Message() << "ui_support=" << ui_support);
      const Expectations& expected =
          ui_support ? kExpectedWithUISupport : kExpectedWithoutUISupport;
      test_client_.supports_pin = ui_support;

      for (int support_level = 0; support_level <= 2; support_level++) {
        for (const auto pin_protocol :
             {device::PINUVAuthProtocol::kV1, device::PINUVAuthProtocol::kV2}) {
          SCOPED_TRACE(testing::Message()
                       << "support_level="
                       << kPINSupportDescription[support_level]
                       << ", pin_protocol=" << static_cast<int>(pin_protocol));
          for (const bool excluded_credentials : {false, true}) {
            SCOPED_TRACE(::testing::Message()
                         << "excluded_credentials=" << excluded_credentials);
            for (const bool appid_exclude : {false, true}) {
              if (appid_exclude && !excluded_credentials) {
                continue;
              }
              SCOPED_TRACE(::testing::Message()
                           << "appid_exclude=" << appid_exclude);

              for (const bool always_uv : {false, true}) {
                if (always_uv &&
                    (!ui_support ||
                     virtual_device_factory_->mutable_state()->pin.empty())) {
                  continue;
                }
                SCOPED_TRACE(::testing::Message() << "always_uv=" << always_uv);

                ConfigureVirtualDevice(pin_protocol, pin_uv_auth_token,
                                       support_level);

                for (int uv_level = 0; uv_level <= 2; uv_level++) {
                  SCOPED_TRACE(kUVDescription[uv_level]);

                  switch (expected[support_level][uv_level]) {
                    case kNoPIN:
                    case kFailure:
                      // There shouldn't be any PIN prompts.
                      test_client_.expected.clear();
                      break;

                    case kSetPIN:
                      // A single PIN prompt to set a PIN is expected.
                      test_client_.expected = {{PINReason::kSet, kTestPIN16}};
                      break;

                    case kUsePIN:
                      // A single PIN prompt to get the PIN is expected.
                      test_client_.expected = {
                          {PINReason::kChallenge, kTestPIN16, 8}};
                      break;

                    default:
                      NOTREACHED();
                  }

                  MakeCredentialResult result =
                      AuthenticatorMakeCredential(make_credential_options(
                          kUVLevel[uv_level], excluded_credentials,
                          appid_exclude));

                  switch (expected[support_level][uv_level]) {
                    case kFailure:
                      EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR,
                                result.status);
                      break;

                    case kNoPIN:
                      ASSERT_EQ(AuthenticatorStatus::SUCCESS, result.status);
                      EXPECT_EQ("",
                                virtual_device_factory_->mutable_state()->pin);
                      EXPECT_FALSE(HasUV(result.response));
                      break;

                    case kSetPIN:
                    case kUsePIN:
                      ASSERT_EQ(AuthenticatorStatus::SUCCESS, result.status);
                      EXPECT_EQ(kTestPIN,
                                virtual_device_factory_->mutable_state()->pin);
                      EXPECT_TRUE(HasUV(result.response));
                      break;

                    default:
                      NOTREACHED();
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

TEST_F(PINAuthenticatorImplTest, MakeCredentialSoftLock) {
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->pin_retries =
      device::kMaxPinRetries;

  test_client_.expected = {{PINReason::kChallenge, u"wrong", 8},
                           {PINReason::kChallenge, u"wrong", 7,
                            device::kMinPinLength, PINError::kWrongPIN},
                           {PINReason::kChallenge, u"wrong", 6,
                            device::kMinPinLength, PINError::kWrongPIN}};
  EXPECT_EQ(AuthenticatorMakeCredential(make_credential_options()).status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
  EXPECT_EQ(5, virtual_device_factory_->mutable_state()->pin_retries);
  EXPECT_TRUE(virtual_device_factory_->mutable_state()->soft_locked);
  ASSERT_TRUE(test_client_.failure_reason.has_value());
  EXPECT_EQ(InterestingFailureReason::kSoftPINBlock,
            *test_client_.failure_reason);
}

TEST_F(PINAuthenticatorImplTest, MakeCredentialHardLock) {
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->pin_retries = 1;

  test_client_.expected = {{PINReason::kChallenge, u"wrong", 1}};
  EXPECT_EQ(AuthenticatorMakeCredential().status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
  EXPECT_EQ(0, virtual_device_factory_->mutable_state()->pin_retries);
  ASSERT_TRUE(test_client_.failure_reason.has_value());
  EXPECT_EQ(InterestingFailureReason::kHardPINBlock,
            *test_client_.failure_reason);
}

TEST_F(PINAuthenticatorImplTest, MakeCredentialWrongPINFirst) {
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->pin_retries =
      device::kMaxPinRetries;

  // Test that we can successfully get a PIN token after a failure.
  test_client_.expected = {{PINReason::kChallenge, u"wrong", 8},
                           {PINReason::kChallenge, kTestPIN16, 7,
                            device::kMinPinLength, PINError::kWrongPIN}};
  EXPECT_EQ(AuthenticatorMakeCredential().status, AuthenticatorStatus::SUCCESS);
  EXPECT_EQ(static_cast<int>(device::kMaxPinRetries),
            virtual_device_factory_->mutable_state()->pin_retries);
}

TEST_F(PINAuthenticatorImplTest, MakeCredentialSkipPINTouch) {
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  int taps = 0;
  virtual_device_factory_->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting([&](device::VirtualFidoDevice* device) {
        ++taps;
        return true;
      });
  virtual_device_factory_->mutable_state()->pin_retries =
      device::kMaxPinRetries;
  test_client_.expected = {
      {PINReason::kChallenge, kTestPIN16, device::kMaxPinRetries}};
  EXPECT_EQ(AuthenticatorMakeCredential().status, AuthenticatorStatus::SUCCESS);
  EXPECT_EQ(taps, 1);
}

TEST_F(PINAuthenticatorImplTest, MakeCredentialDontSkipPINTouch) {
  // Create two devices. Both are candidates but only the second one will
  // respond to touches.
  auto discovery =
      std::make_unique<device::test::MultipleVirtualFidoDeviceFactory>();
  device::test::MultipleVirtualFidoDeviceFactory::DeviceDetails device_1;
  device_1.config.pin_support = true;
  device_1.state->simulate_press_callback =
      base::BindRepeating([](VirtualFidoDevice* _) { return false; });
  discovery->AddDevice(std::move(device_1));

  int taps = 0;
  device::test::MultipleVirtualFidoDeviceFactory::DeviceDetails device_2;
  device_2.state->pin = kTestPIN;
  device_2.config.pin_support = true;
  device_2.state->simulate_press_callback =
      base::BindLambdaForTesting([&](VirtualFidoDevice* _) {
        ++taps;
        return true;
      });
  discovery->AddDevice(std::move(device_2));

  AuthenticatorEnvironmentImpl::GetInstance()
      ->ReplaceDefaultDiscoveryFactoryForTesting(std::move(discovery));

  test_client_.expected = {
      {PINReason::kChallenge, kTestPIN16, device::kMaxPinRetries}};
  EXPECT_EQ(AuthenticatorMakeCredential().status, AuthenticatorStatus::SUCCESS);
  EXPECT_EQ(taps, 2);
}

TEST_F(PINAuthenticatorImplTest, MakeCredentialAlwaysUv) {
  // Test that if an authenticator is reporting alwaysUv = 1, UV is attempted
  // even if the user verification requirement is discouraged.
  device::VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.always_uv = true;

  // Enable u2f support. Normally, this would allow chrome to create a
  // credential without internal user verification, but we should not attempt
  // that with the alwaysUv flag on.
  config.u2f_support = true;
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  test_client_.expected = {
      {PINReason::kChallenge, kTestPIN16, device::kMaxPinRetries}};

  MakeCredentialResult result =
      AuthenticatorMakeCredential(make_credential_options(
          device::UserVerificationRequirement::kDiscouraged));
  EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
  EXPECT_TRUE(HasUV(result.response));
}

TEST_F(PINAuthenticatorImplTest, MakeCredentialAlwaysUvU2fOnly) {
  // Test that even if an authenticator is reporting alwaysUv = 1, cryptotoken
  // requests don't try getting a PinUvAuthToken.
  NavigateAndCommit(GURL(kCryptotokenOrigin));

  for (bool internal_uv : {true, false}) {
    SCOPED_TRACE(::testing::Message() << "internal_uv=" << internal_uv);
    device::VirtualCtap2Device::Config config;
    config.internal_uv_support = internal_uv;
    config.u2f_support = internal_uv;
    config.always_uv = true;
    config.pin_support = true;
    virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;
    virtual_device_factory_->SetCtap2Config(config);

    PublicKeyCredentialCreationOptionsPtr options = make_credential_options(
        device::UserVerificationRequirement::kDiscouraged);
    if (internal_uv) {
      MakeCredentialResult result =
          AuthenticatorMakeCredential(std::move(options));
      EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
      EXPECT_FALSE(HasUV(result.response));
    } else {
      MakeCredentialResult result =
          AuthenticatorMakeCredentialAndWaitForTimeout(std::move(options));
      EXPECT_EQ(result.status, AuthenticatorStatus::NOT_ALLOWED_ERROR);
    }

    // Look at the permissions to verify that a PinUvAuthToken was not obtained.
    EXPECT_EQ(
        virtual_device_factory_->mutable_state()->pin_uv_token_permissions, 0);
  }
}

TEST_F(PINAuthenticatorImplTest, MakeCredentialMinPINLengthNewPIN) {
  // Test that an authenticator advertising a min PIN length other than the
  // default makes it all the way to CollectPIN when setting a new PIN.
  device::VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.min_pin_length_support = true;
  config.pin_uv_auth_token_support = true;
  config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->min_pin_length = 6;
  test_client_.expected = {{PINReason::kSet, u"123456", 0, 6}};

  MakeCredentialResult result =
      AuthenticatorMakeCredential(make_credential_options());
  EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
}

TEST_F(PINAuthenticatorImplTest, MakeCredentialMinPINLengthExistingPIN) {
  // Test that an authenticator advertising a min PIN length other than the
  // default makes it all the way to CollectPIN when using an existing PIN and
  // the forcePINChange flag is false.
  device::VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.min_pin_length_support = true;
  config.pin_uv_auth_token_support = true;
  config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->min_pin_length = 6;
  virtual_device_factory_->mutable_state()->pin = "123456";
  test_client_.expected = {
      {PINReason::kChallenge, u"123456", device::kMaxPinRetries, 6}};

  MakeCredentialResult result =
      AuthenticatorMakeCredential(make_credential_options());
  EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
}

TEST_F(PINAuthenticatorImplTest, MakeCredentialForcePINChange) {
  // Test that an authenticator with the forcePINChange flag set to true updates
  // the PIN before attempting to make a credential. When querying for an
  // existing PIN, the default min PIN length should be asked since there is no
  // way to know the current PIN length.
  device::VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.min_pin_length_support = true;
  config.pin_uv_auth_token_support = true;
  config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->force_pin_change = true;
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->pin_retries =
      device::kMaxPinRetries;
  virtual_device_factory_->mutable_state()->min_pin_length = 6;
  test_client_.expected = {{PINReason::kChallenge, kTestPIN16,
                            device::kMaxPinRetries, device::kMinPinLength},
                           {PINReason::kChange, u"567890", 0, 6}};

  MakeCredentialResult result =
      AuthenticatorMakeCredential(make_credential_options());
  EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
  EXPECT_EQ("567890", virtual_device_factory_->mutable_state()->pin);
}

TEST_F(PINAuthenticatorImplTest, MakeCredUvNotRqd) {
  // Test that on an authenticator with the makeCredUvNotRqd option enabled,
  // non-discoverable credentials can be created without requiring a PIN.
  for (bool discoverable : {false, true}) {
    for (bool request_uv : {false, true}) {
      SCOPED_TRACE(testing::Message() << "discoverable=" << discoverable
                                      << " request_uv=" << request_uv);

      test_client_.web_authentication_delegate.supports_resident_keys = true;
      ResetVirtualDevice();
      device::VirtualCtap2Device::Config config;
      config.u2f_support = true;
      config.pin_support = true;
      config.resident_key_support = true;
      config.pin_uv_auth_token_support = true;
      config.allow_non_resident_credential_creation_without_uv = true;
      config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
      virtual_device_factory_->SetCtap2Config(config);
      virtual_device_factory_->mutable_state()->pin = kTestPIN;
      // PIN is still required for discoverable credentials, or if the caller
      // requests it.
      if (discoverable || request_uv) {
        test_client_.expected = {{PINReason::kChallenge, kTestPIN16,
                                  device::kMaxPinRetries,
                                  device::kMinPinLength}};
      } else {
        test_client_.expected = {};
      }

      PublicKeyCredentialCreationOptionsPtr request = make_credential_options();
      request->authenticator_selection
          ->SetUserVerificationRequirementForTesting(
              request_uv ? device::UserVerificationRequirement::kPreferred
                         : device::UserVerificationRequirement::kDiscouraged);
      request->authenticator_selection->SetResidentKeyForTesting(
          discoverable ? device::ResidentKeyRequirement::kPreferred
                       : device::ResidentKeyRequirement::kDiscouraged);

      MakeCredentialResult result =
          AuthenticatorMakeCredential(std::move(request));
      EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
      // Requests shouldn't fall back to creating U2F credentials.
      EXPECT_FALSE(virtual_device_factory_->mutable_state()
                       ->registrations.begin()
                       ->second.is_u2f);
    }
  }
}

TEST_F(PINAuthenticatorImplTest, MakeCredUvNotRqdAndAlwaysUv) {
  // makeCredUvNotRqd and alwaysUv can be combined even though they contradict
  // each other. In that case, makeCredUvNotRqd should be ignored and PIN/UV
  // should be collected before creating non-discoverable credentials. If PIN/UV
  // isn't configured, that should be taken care of first.
  for (bool pin_set : {false, true}) {
    SCOPED_TRACE(testing::Message() << "pin_set=" << pin_set);

    ResetVirtualDevice();
    device::VirtualCtap2Device::Config config;
    config.pin_support = true;
    config.pin_uv_auth_token_support = true;
    config.always_uv = true;
    config.allow_non_resident_credential_creation_without_uv = true;
    config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
    virtual_device_factory_->SetCtap2Config(config);
    if (pin_set) {
      virtual_device_factory_->mutable_state()->pin = kTestPIN;
      test_client_.expected = {{PINReason::kChallenge, kTestPIN16,
                                device::kMaxPinRetries, device::kMinPinLength}};
    } else {
      test_client_.expected = {{PINReason::kSet, kTestPIN16,
                                device::kMaxPinRetries, device::kMinPinLength}};
    }

    MakeCredentialResult result =
        AuthenticatorMakeCredential(make_credential_options());
    EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
  }
}

TEST_F(PINAuthenticatorImplTest, GetAssertion) {
  typedef int Expectations[3][3];
  // kExpectedWithUISupport enumerates the expected behaviour when the embedder
  // supports prompting the user for a PIN.
  // clang-format off
  const Expectations kExpectedWithUISupport = {
    //                   discouraged | preferred | required
    /* No support */  {    kNoPIN,      kNoPIN,     kFailure },
    /* PIN not set */ {    kNoPIN,      kNoPIN,     kFailure },
    /* PIN set */     {    kNoPIN,      kUsePIN,    kUsePIN  },
  };
  // clang-format on

  // kExpectedWithoutUISupport enumerates the expected behaviour when the
  // embedder cannot prompt the user for a PIN.
  // clang-format off
  const Expectations kExpectedWithoutUISupport = {
    //                   discouraged | preferred | required
    /* No support */  {    kNoPIN,      kNoPIN,     kFailure },
    /* PIN not set */ {    kNoPIN,      kNoPIN,     kFailure },
    /* PIN set */     {    kNoPIN,      kNoPIN,     kFailure },
  };
  // clang-format on

  PublicKeyCredentialRequestOptionsPtr dummy_options = get_credential_options();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      dummy_options->allow_credentials[0].id(), kTestRelyingPartyId));

  for (bool pin_uv_auth_token : {false, true}) {
    for (bool ui_support : {false, true}) {
      SCOPED_TRACE(::testing::Message() << "ui_support=" << ui_support);
      const Expectations& expected =
          ui_support ? kExpectedWithUISupport : kExpectedWithoutUISupport;
      test_client_.supports_pin = ui_support;

      for (int support_level = 0; support_level <= 2; support_level++) {
        SCOPED_TRACE(kPINSupportDescription[support_level]);
        for (const auto pin_protocol :
             {device::PINUVAuthProtocol::kV1, device::PINUVAuthProtocol::kV2}) {
          SCOPED_TRACE(testing::Message()
                       << "support_level="
                       << kPINSupportDescription[support_level]
                       << ", pin_protocol=" << static_cast<int>(pin_protocol));
          ConfigureVirtualDevice(pin_protocol, pin_uv_auth_token,
                                 support_level);

          for (int uv_level = 0; uv_level <= 2; uv_level++) {
            SCOPED_TRACE(kUVDescription[uv_level]);

            switch (expected[support_level][uv_level]) {
              case kNoPIN:
              case kFailure:
                // No PIN prompts are expected.
                test_client_.expected.clear();
                break;

              case kUsePIN:
                // A single prompt to get the PIN is expected.
                test_client_.expected = {
                    {PINReason::kChallenge, kTestPIN16, 8}};
                break;

              default:
                NOTREACHED();
            }

            GetAssertionResult result = AuthenticatorGetAssertion(
                get_credential_options(kUVLevel[uv_level]));

            switch (expected[support_level][uv_level]) {
              case kFailure:
                EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR,
                          result.status);
                break;

              case kNoPIN:
                EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
                EXPECT_FALSE(HasUV(result.response));
                break;

              case kUsePIN:
                EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
                EXPECT_EQ(kTestPIN,
                          virtual_device_factory_->mutable_state()->pin);
                EXPECT_TRUE(HasUV(result.response));
                break;

              default:
                NOTREACHED();
            }
          }
        }
      }
    }
  }
}

TEST_F(PINAuthenticatorImplTest, GetAssertionSoftLock) {
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->pin_retries =
      device::kMaxPinRetries;

  PublicKeyCredentialRequestOptionsPtr options = get_credential_options();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id(), kTestRelyingPartyId));

  test_client_.expected = {{PINReason::kChallenge, u"wrong", 8},
                           {PINReason::kChallenge, u"wrong", 7,
                            device::kMinPinLength, PINError::kWrongPIN},
                           {PINReason::kChallenge, u"wrong", 6,
                            device::kMinPinLength, PINError::kWrongPIN}};
  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
  EXPECT_EQ(5, virtual_device_factory_->mutable_state()->pin_retries);
  EXPECT_TRUE(virtual_device_factory_->mutable_state()->soft_locked);
  ASSERT_TRUE(test_client_.failure_reason.has_value());
  EXPECT_EQ(InterestingFailureReason::kSoftPINBlock,
            *test_client_.failure_reason);
}

TEST_F(PINAuthenticatorImplTest, GetAssertionHardLock) {
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->pin_retries = 1;

  PublicKeyCredentialRequestOptionsPtr options = get_credential_options();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id(), kTestRelyingPartyId));

  test_client_.expected = {{PINReason::kChallenge, u"wrong", 1}};
  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
  EXPECT_EQ(0, virtual_device_factory_->mutable_state()->pin_retries);
  ASSERT_TRUE(test_client_.failure_reason.has_value());
  EXPECT_EQ(InterestingFailureReason::kHardPINBlock,
            *test_client_.failure_reason);
}

TEST_F(PINAuthenticatorImplTest, GetAssertionSkipPINTouch) {
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  int taps = 0;
  virtual_device_factory_->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting([&](device::VirtualFidoDevice* device) {
        ++taps;
        return true;
      });
  PublicKeyCredentialRequestOptionsPtr options = get_credential_options();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id(), kTestRelyingPartyId));
  test_client_.expected = {
      {PINReason::kChallenge, kTestPIN16, device::kMaxPinRetries}};
  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::SUCCESS);
  EXPECT_EQ(taps, 1);
}

TEST_F(PINAuthenticatorImplTest, GetAssertionDontSkipPINTouch) {
  // Create two devices. Both are candidates but only the second one will
  // respond to touches.
  auto discovery =
      std::make_unique<device::test::MultipleVirtualFidoDeviceFactory>();
  device::test::MultipleVirtualFidoDeviceFactory::DeviceDetails device_1;
  device_1.config.pin_support = true;
  device_1.state->simulate_press_callback =
      base::BindRepeating([](VirtualFidoDevice* _) { return false; });
  discovery->AddDevice(std::move(device_1));

  int taps = 0;
  device::test::MultipleVirtualFidoDeviceFactory::DeviceDetails device_2;
  device_2.state->pin = kTestPIN;
  device_2.config.pin_support = true;
  device_2.state->simulate_press_callback =
      base::BindLambdaForTesting([&](VirtualFidoDevice* _) {
        ++taps;
        return true;
      });
  PublicKeyCredentialRequestOptionsPtr options = get_credential_options();
  ASSERT_TRUE(device_2.state->InjectRegistration(
      options->allow_credentials[0].id(), kTestRelyingPartyId));
  discovery->AddDevice(std::move(device_2));

  AuthenticatorEnvironmentImpl::GetInstance()
      ->ReplaceDefaultDiscoveryFactoryForTesting(std::move(discovery));

  test_client_.expected = {
      {PINReason::kChallenge, kTestPIN16, device::kMaxPinRetries}};
  EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
            AuthenticatorStatus::SUCCESS);
  EXPECT_EQ(taps, 2);
}

TEST_F(PINAuthenticatorImplTest, GetAssertionAlwaysUv) {
  // Test that if an authenticator is reporting alwaysUv = 1, UV is attempted
  // even if the user verification requirement is discouraged.
  device::VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.always_uv = true;
  config.u2f_support = true;
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  PublicKeyCredentialRequestOptionsPtr options =
      get_credential_options(device::UserVerificationRequirement::kDiscouraged);
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id(), kTestRelyingPartyId));
  test_client_.expected = {
      {PINReason::kChallenge, kTestPIN16, device::kMaxPinRetries}};

  GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));
  EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
  EXPECT_TRUE(HasUV(result.response));
}

TEST_F(PINAuthenticatorImplTest, GetAssertionAlwaysUvU2fOnly) {
  // Test that even if an authenticator is reporting alwaysUv = 1, cryptotoken
  // requests don't try getting a PinUvAuthToken.
  NavigateAndCommit(GURL(kCryptotokenOrigin));
  PublicKeyCredentialRequestOptionsPtr options =
      get_credential_options(device::UserVerificationRequirement::kDiscouraged);
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id(), kTestRelyingPartyId));

  for (bool internal_uv : {true, false}) {
    SCOPED_TRACE(::testing::Message() << "internal_uv=" << internal_uv);
    device::VirtualCtap2Device::Config config;
    config.internal_uv_support = internal_uv;
    config.u2f_support = internal_uv;
    config.always_uv = true;
    config.pin_support = true;
    virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;
    virtual_device_factory_->SetCtap2Config(config);

    if (internal_uv) {
      GetAssertionResult result = AuthenticatorGetAssertion(options.Clone());
      EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
      EXPECT_FALSE(HasUV(result.response));
    } else {
      GetAssertionResult result =
          AuthenticatorGetAssertionAndWaitForTimeout(options.Clone());
      EXPECT_EQ(result.status, AuthenticatorStatus::NOT_ALLOWED_ERROR);
    }

    // Look at the permissions to verify that a PinUvAuthToken was not obtained.
    EXPECT_EQ(
        virtual_device_factory_->mutable_state()->pin_uv_token_permissions, 0);
  }
}

TEST_F(PINAuthenticatorImplTest, MakeCredentialNoSupportedAlgorithm) {
  NavigateAndCommit(GURL(kTestOrigin1));

  for (int i = 0; i < 3; i++) {
    SCOPED_TRACE(i);

    test_client_.expected.clear();
    bool expected_to_succeed = false;
    if (i == 0) {
      device::VirtualCtap2Device::Config config;
      // The first config is a CTAP2 device that doesn't support the
      // kInvalidForTesting algorithm. A dummy touch should be requested in this
      // case.
      virtual_device_factory_->SetCtap2Config(config);
    } else if (i == 1) {
      device::VirtualCtap2Device::Config config;
      // The second config is a device with a PIN set that _does_ support the
      // algorithm. Since the PIN is set, we might convert the makeCredential
      // request to U2F, but shouldn't because the algorithm cannot be
      // represented in U2F.
      config.u2f_support = true;
      config.pin_support = true;
      config.advertised_algorithms = {
          device::CoseAlgorithmIdentifier::kInvalidForTesting};
      virtual_device_factory_->mutable_state()->pin = kTestPIN;
      virtual_device_factory_->mutable_state()->pin_retries =
          device::kMaxPinRetries;
      virtual_device_factory_->SetCtap2Config(config);
      test_client_.expected = {
          {PINReason::kChallenge, kTestPIN16, device::kMaxPinRetries}};
      // Since converting to U2F isn't possible, this will trigger a PIN prompt
      // and succeed because the device does actually support the algorithm.
      expected_to_succeed = true;
    } else if (i == 2) {
      // The third case is a plain U2F authenticator, which implicitly only
      // supports ES256.
      virtual_device_factory_->SetSupportedProtocol(
          device::ProtocolVersion::kU2f);
    }

    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    // Set uv=discouraged so that U2F fallback is possible.
    options->authenticator_selection->SetUserVerificationRequirementForTesting(
        device::UserVerificationRequirement::kDiscouraged);
    options->public_key_parameters =
        GetTestPublicKeyCredentialParameters(static_cast<int32_t>(
            device::CoseAlgorithmIdentifier::kInvalidForTesting));

    EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
              expected_to_succeed ? AuthenticatorStatus::SUCCESS
                                  : AuthenticatorStatus::NOT_ALLOWED_ERROR);
  }
}

TEST_F(PINAuthenticatorImplTest, PRFCreatedOnCTAP2) {
  // Check that credential creation requests that include the PRF extension use
  // CTAP2 if possible.
  NavigateAndCommit(GURL(kTestOrigin1));

  for (int i = 0; i < 3; i++) {
    SCOPED_TRACE(i);

    device::VirtualCtap2Device::Config config;
    config.u2f_support = true;
    config.pin_support = true;
    config.hmac_secret_support = true;
    virtual_device_factory_->mutable_state()->pin = kTestPIN;
    virtual_device_factory_->mutable_state()->pin_retries =
        device::kMaxPinRetries;
    virtual_device_factory_->SetCtap2Config(config);

    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    // Set uv=discouraged so that U2F fallback is possible.
    options->authenticator_selection->SetUserVerificationRequirementForTesting(
        device::UserVerificationRequirement::kDiscouraged);

    if (i == 0) {
      // Sanity check: request should fallback to U2F. (If it doesn't fallback
      // to U2F then the PIN test infrastructure will CHECK because
      // |test_client_.expected| is empty.)
      test_client_.expected.clear();
    } else if (i == 1) {
      // If PRF is requested, the fallback to U2F should not happen because the
      // PRF request is higher priority than avoiding a PIN prompt. (The PIN
      // test infrastructure will CHECK if |expected| is set and not used.)
      options->prf_enable = true;
      test_client_.expected = {
          {PINReason::kChallenge, kTestPIN16, device::kMaxPinRetries}};
    } else {
      // If PRF is requested, but the authenticator doesn't support it, then we
      // should still use U2F.
      options->prf_enable = true;
      config.hmac_secret_support = false;
      test_client_.expected.clear();
    }

    virtual_device_factory_->SetCtap2Config(config);

    EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
              AuthenticatorStatus::SUCCESS);
  }
}

// Test that pinUvAuthToken gets sent with every single batch of an exclude
// list. If it wasn't, any batch after the first would be unable to match
// credProtect=uvRequired credentials.
TEST_F(PINAuthenticatorImplTest, ExcludeListBatchesIncludePinToken) {
  NavigateAndCommit(GURL(kTestOrigin1));

  // Set up a CTAP 2.1 authenticator with pinUvAuthToken and exclude list
  // batching.
  device::VirtualCtap2Device::Config config;
  config.max_credential_id_length = kTestCredentialIdLength;
  constexpr size_t kBatchSize = 10;
  config.max_credential_count_in_list = kBatchSize;
  config.pin_support = true;
  config.pin_uv_auth_token_support = true;
  config.ctap2_versions = {device::Ctap2Version::kCtap2_0,
                           device::Ctap2Version::kCtap2_1};
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->pin_retries =
      device::kMaxPinRetries;

  test_client_.expected = {
      {PINReason::kChallenge, kTestPIN16, device::kMaxPinRetries}};

  // Craft an exclude list that is large enough to trigger batched probing and
  // includes one match for a credProtect=uvRequired credential.
  auto test_credentials = GetTestCredentials(kBatchSize + 1);

  device::VirtualFidoDevice::RegistrationData cred_protect_credential(
      kTestRelyingPartyId);
  cred_protect_credential.protection = device::CredProtect::kUVRequired;
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      test_credentials.back().id(), std::move(cred_protect_credential)));

  // The request should fail because the exclude list matches.
  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->exclude_credentials = std::move(test_credentials);
  EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
            AuthenticatorStatus::CREDENTIAL_EXCLUDED);
}

TEST_F(PINAuthenticatorImplTest, RemoveSecondAuthenticator) {
  // Create two PIN-capable devices. Touch one of them to trigger a prompt for
  // a PIN. Remove the other. Don't crash.
  base::RepeatingCallback<void(bool)> disconnect_1, disconnect_2;

  device::test::MultipleVirtualFidoDeviceFactory::DeviceDetails device_1;
  device_1.state->pin = kTestPIN;
  device_1.config.pin_support = true;
  std::tie(disconnect_1, device_1.disconnect_events) =
      device::FidoDeviceDiscovery::EventStream<bool>::New();

  device::test::MultipleVirtualFidoDeviceFactory::DeviceDetails device_2;
  device_2.state->pin = kTestPIN;
  device_2.config.pin_support = true;
  std::tie(disconnect_2, device_2.disconnect_events) =
      device::FidoDeviceDiscovery::EventStream<bool>::New();

  int callbacks = 0;
  auto touch_callback = [&](int device_num) -> bool {
    callbacks++;
    if (callbacks == 1) {
      // Wait for the other authenticator to be triggered.
      return false;
    } else if (callbacks == 2) {
      // Touch authenticator to collect a PIN.
      return true;
    } else {
      CHECK_EQ(callbacks, 3);

      // Disconnect other authenticator then complete with a touch.
      if (device_num == 1) {
        disconnect_2.Run(false);
      } else {
        disconnect_1.Run(false);
      }
      return true;
    }
  };

  device_1.state->simulate_press_callback = base::BindLambdaForTesting(
      [&](VirtualFidoDevice* _) -> bool { return touch_callback(1); });
  device_2.state->simulate_press_callback = base::BindLambdaForTesting(
      [&](VirtualFidoDevice* _) -> bool { return touch_callback(2); });

  auto discovery =
      std::make_unique<device::test::MultipleVirtualFidoDeviceFactory>();
  discovery->AddDevice(std::move(device_1));
  discovery->AddDevice(std::move(device_2));
  AuthenticatorEnvironmentImpl::GetInstance()
      ->ReplaceDefaultDiscoveryFactoryForTesting(std::move(discovery));

  test_client_.expected = {
      {PINReason::kChallenge, kTestPIN16, device::kMaxPinRetries}};
  EXPECT_EQ(AuthenticatorMakeCredential().status, AuthenticatorStatus::SUCCESS);
}

class InternalUVAuthenticatorImplTest : public UVAuthenticatorImplTest {
 public:
  struct TestCase {
    const bool fingerprints_enrolled;
    const bool supports_pin;
    const device::UserVerificationRequirement uv;
  };

  InternalUVAuthenticatorImplTest() = default;

  void SetUp() override {
    UVAuthenticatorImplTest::SetUp();
    NavigateAndCommit(GURL(kTestOrigin1));
  }

  std::vector<TestCase> GetTestCases() {
    std::vector<TestCase> test_cases;
    for (const bool fingerprints_enrolled : {true, false}) {
      for (const bool supports_pin : {true, false}) {
        // Avoid just testing for PIN.
        if (!fingerprints_enrolled && supports_pin)
          continue;
        for (const auto uv : {device::UserVerificationRequirement::kDiscouraged,
                              device::UserVerificationRequirement::kPreferred,
                              device::UserVerificationRequirement::kRequired}) {
          test_cases.push_back({fingerprints_enrolled, supports_pin, uv});
        }
      }
    }
    return test_cases;
  }

  void ConfigureDevice(const TestCase& test_case) {
    device::VirtualCtap2Device::Config config;
    config.internal_uv_support = true;
    config.u2f_support = true;
    config.pin_support = test_case.supports_pin;
    virtual_device_factory_->mutable_state()->pin = kTestPIN;
    virtual_device_factory_->mutable_state()->pin_retries =
        device::kMaxPinRetries;
    virtual_device_factory_->mutable_state()->fingerprints_enrolled =
        test_case.fingerprints_enrolled;
    virtual_device_factory_->SetCtap2Config(config);
    SCOPED_TRACE(::testing::Message() << "fingerprints_enrolled="
                                      << test_case.fingerprints_enrolled);
    SCOPED_TRACE(::testing::Message()
                 << "supports_pin=" << test_case.supports_pin);
    SCOPED_TRACE(UVToString(test_case.uv));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InternalUVAuthenticatorImplTest);
};

TEST_F(InternalUVAuthenticatorImplTest, MakeCredential) {
  for (const auto test_case : GetTestCases()) {
    ConfigureDevice(test_case);

    auto options = make_credential_options(test_case.uv);
    // UV cannot be satisfied without fingerprints.
    const bool should_timeout =
        !test_case.fingerprints_enrolled &&
        test_case.uv == device::UserVerificationRequirement::kRequired;
    if (should_timeout) {
      options->timeout = base::TimeDelta::FromMilliseconds(100);
    }

    MakeCredentialResult result =
        AuthenticatorMakeCredential(std::move(options));

    if (should_timeout) {
      EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, result.status);
    } else {
      EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
      EXPECT_EQ(test_case.fingerprints_enrolled, HasUV(result.response));
    }
  }
}

// Test falling back to PIN for devices that support internal user verification
// but not uv token.
TEST_F(InternalUVAuthenticatorImplTest, MakeCredentialFallBackToPin) {
  device::VirtualCtap2Device::Config config;
  config.internal_uv_support = true;
  config.pin_support = true;
  config.user_verification_succeeds = false;
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->pin_retries =
      device::kMaxPinRetries;
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;
  virtual_device_factory_->SetCtap2Config(config);

  auto options =
      make_credential_options(device::UserVerificationRequirement::kRequired);

  MakeCredentialResult result = AuthenticatorMakeCredential(std::move(options));

  EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
  EXPECT_TRUE(HasUV(result.response));
  EXPECT_TRUE(test_client_.collected_pin);
  EXPECT_EQ(device::kMinPinLength, test_client_.min_pin_length);
}

TEST_F(InternalUVAuthenticatorImplTest, MakeCredentialCryptotoken) {
  NavigateAndCommit(GURL(kCryptotokenOrigin));

  for (const auto fingerprints_enrolled : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "fingerprints_enrolled=" << fingerprints_enrolled);
    virtual_device_factory_->mutable_state()->fingerprints_enrolled =
        fingerprints_enrolled;
    EXPECT_EQ(AuthenticatorMakeCredential(
                  make_credential_options(
                      device::UserVerificationRequirement::kPreferred))
                  .status,
              AuthenticatorStatus::SUCCESS);
    // The credential should have been created over U2F.
    for (const auto& registration :
         virtual_device_factory_->mutable_state()->registrations) {
      EXPECT_TRUE(registration.second.is_u2f);
    }
  }
}

// Test making a credential on an authenticator that supports biometric
// enrollment but has no fingerprints enrolled.
TEST_F(InternalUVAuthenticatorImplTest, MakeCredentialInlineBioEnrollment) {
  device::VirtualCtap2Device::Config config;
  config.internal_uv_support = true;
  config.pin_support = true;
  config.user_verification_succeeds = true;
  config.bio_enrollment_support = true;
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->pin_retries =
      device::kMaxPinRetries;
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = false;
  virtual_device_factory_->SetCtap2Config(config);

  MakeCredentialResult result = AuthenticatorMakeCredential(
      make_credential_options(device::UserVerificationRequirement::kRequired));

  EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
  EXPECT_TRUE(HasUV(result.response));
  EXPECT_TRUE(test_client_.collected_pin);
  EXPECT_EQ(device::kMinPinLength, test_client_.min_pin_length);
  EXPECT_TRUE(test_client_.did_bio_enrollment);
  EXPECT_TRUE(virtual_device_factory_->mutable_state()->fingerprints_enrolled);
}

// Test making a credential skipping biometric enrollment during credential
// creation.
TEST_F(InternalUVAuthenticatorImplTest, MakeCredentialSkipInlineBioEnrollment) {
  test_client_.cancel_bio_enrollment = true;

  device::VirtualCtap2Device::Config config;
  config.internal_uv_support = true;
  config.pin_support = true;
  config.user_verification_succeeds = true;
  config.bio_enrollment_support = true;
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->pin_retries =
      device::kMaxPinRetries;
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = false;
  virtual_device_factory_->SetCtap2Config(config);

  MakeCredentialResult result = AuthenticatorMakeCredential(
      make_credential_options(device::UserVerificationRequirement::kRequired));

  EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
  EXPECT_TRUE(HasUV(result.response));
  EXPECT_TRUE(test_client_.collected_pin);
  EXPECT_EQ(device::kMinPinLength, test_client_.min_pin_length);
  EXPECT_TRUE(test_client_.did_bio_enrollment);
  EXPECT_FALSE(virtual_device_factory_->mutable_state()->fingerprints_enrolled);
}

TEST_F(InternalUVAuthenticatorImplTest, MakeCredUvNotRqd) {
  // Test that on an authenticator with the makeCredUvNotRqd option enabled,
  // non-discoverable credentials can be created without requiring UV or a PIN.
  for (bool discoverable : {false, true}) {
    for (bool request_uv : {false, true}) {
      SCOPED_TRACE(testing::Message() << "discoverable=" << discoverable
                                      << " request_uv=" << request_uv);

      test_client_.web_authentication_delegate.supports_resident_keys = true;
      ResetVirtualDevice();
      device::VirtualCtap2Device::Config config;
      config.u2f_support = true;
      config.internal_uv_support = true;
      config.user_verification_succeeds = true;
      config.pin_support = true;
      config.resident_key_support = true;
      config.pin_uv_auth_token_support = true;
      config.allow_non_resident_credential_creation_without_uv = true;
      config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
      virtual_device_factory_->SetCtap2Config(config);
      virtual_device_factory_->mutable_state()->pin = kTestPIN;
      virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;

      PublicKeyCredentialCreationOptionsPtr request = make_credential_options();
      request->authenticator_selection
          ->SetUserVerificationRequirementForTesting(
              request_uv ? device::UserVerificationRequirement::kPreferred
                         : device::UserVerificationRequirement::kDiscouraged);
      request->authenticator_selection->SetResidentKeyForTesting(
          discoverable ? device::ResidentKeyRequirement::kPreferred
                       : device::ResidentKeyRequirement::kDiscouraged);

      MakeCredentialResult result =
          AuthenticatorMakeCredential(std::move(request));
      EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
      EXPECT_EQ(HasUV(result.response), discoverable || request_uv);
      EXPECT_FALSE(test_client_.collected_pin);
      // Requests shouldn't fall back to creating U2F credentials.
      EXPECT_FALSE(virtual_device_factory_->mutable_state()
                       ->registrations.begin()
                       ->second.is_u2f);
    }
  }
}

TEST_F(InternalUVAuthenticatorImplTest, GetAssertion) {
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      get_credential_options()->allow_credentials[0].id(),
      kTestRelyingPartyId));

  for (const auto test_case : GetTestCases()) {
    ConfigureDevice(test_case);
    // Without a fingerprint enrolled we assume that a UV=required request
    // cannot be satisfied by an authenticator that cannot do UV. It is
    // possible for a credential to be created without UV and then later
    // asserted with UV=required, but that would be bizarre behaviour from
    // an RP and we currently don't worry about it.
    const bool should_be_unrecognized =
        !test_case.fingerprints_enrolled &&
        test_case.uv == device::UserVerificationRequirement::kRequired;

    GetAssertionResult result =
        AuthenticatorGetAssertion(get_credential_options(test_case.uv));

    if (should_be_unrecognized) {
      EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, result.status);
    } else {
      EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
      EXPECT_EQ(
          test_case.fingerprints_enrolled &&
              test_case.uv != device::UserVerificationRequirement::kDiscouraged,
          HasUV(result.response));
    }
  }
}

// Test falling back to PIN for devices that support internal user verification
// but not uv token.
TEST_F(InternalUVAuthenticatorImplTest, GetAssertionFallbackToPIN) {
  device::VirtualCtap2Device::Config config;
  config.internal_uv_support = true;
  config.pin_support = true;
  config.user_verification_succeeds = false;
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  virtual_device_factory_->mutable_state()->pin_retries =
      device::kMaxPinRetries;
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;
  virtual_device_factory_->SetCtap2Config(config);

  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      get_credential_options()->allow_credentials[0].id(),
      kTestRelyingPartyId));

  GetAssertionResult result = AuthenticatorGetAssertion(
      get_credential_options(device::UserVerificationRequirement::kRequired));

  EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
  EXPECT_TRUE(HasUV(result.response));
  EXPECT_TRUE(test_client_.collected_pin);
  EXPECT_EQ(device::kMinPinLength, test_client_.min_pin_length);
}

TEST_F(InternalUVAuthenticatorImplTest, GetAssertionCryptotoken) {
  NavigateAndCommit(GURL(kCryptotokenOrigin));
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      get_credential_options()->allow_credentials[0].id(),
      kTestRelyingPartyId));

  for (const auto fingerprints_enrolled : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "fingerprints_enrolled=" << fingerprints_enrolled);
    virtual_device_factory_->mutable_state()->fingerprints_enrolled =
        fingerprints_enrolled;
    EXPECT_EQ(AuthenticatorGetAssertion(
                  get_credential_options(
                      device::UserVerificationRequirement::kPreferred))
                  .status,
              AuthenticatorStatus::SUCCESS);
  }
}

class UVTokenAuthenticatorImplTest : public UVAuthenticatorImplTest {
 public:
  UVTokenAuthenticatorImplTest() = default;
  UVTokenAuthenticatorImplTest(const UVTokenAuthenticatorImplTest&) = delete;

  void SetUp() override {
    UVAuthenticatorImplTest::SetUp();
    device::VirtualCtap2Device::Config config;
    config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
    config.internal_uv_support = true;
    config.pin_uv_auth_token_support = true;
    virtual_device_factory_->SetCtap2Config(config);
    NavigateAndCommit(GURL(kTestOrigin1));
  }
};

TEST_F(UVTokenAuthenticatorImplTest, GetAssertionUVToken) {
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      get_credential_options()->allow_credentials[0].id(),
      kTestRelyingPartyId));

  for (const auto fingerprints_enrolled : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "fingerprints_enrolled=" << fingerprints_enrolled);
    virtual_device_factory_->mutable_state()->fingerprints_enrolled =
        fingerprints_enrolled;

    for (auto uv : {device::UserVerificationRequirement::kDiscouraged,
                    device::UserVerificationRequirement::kPreferred,
                    device::UserVerificationRequirement::kRequired}) {
      SCOPED_TRACE(UVToString(uv));

      // Without a fingerprint enrolled we assume that a UV=required request
      // cannot be satisfied by an authenticator that cannot do UV. It is
      // possible for a credential to be created without UV and then later
      // asserted with UV=required, but that would be bizarre behaviour from
      // an RP and we currently don't worry about it.
      const bool should_be_unrecognized =
          !fingerprints_enrolled &&
          uv == device::UserVerificationRequirement::kRequired;

      GetAssertionResult result =
          AuthenticatorGetAssertion(get_credential_options(uv));

      if (should_be_unrecognized) {
        EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, result.status);
      } else {
        EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
        EXPECT_EQ(fingerprints_enrolled &&
                      uv != device::UserVerificationRequirement::kDiscouraged,
                  HasUV(result.response));
      }
    }
  }
}

// Test exhausting all internal user verification attempts on an authenticator
// that does not support PINs.
TEST_F(UVTokenAuthenticatorImplTest, GetAssertionUvFails) {
  device::VirtualCtap2Device::Config config;
  config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
  config.internal_uv_support = true;
  config.pin_uv_auth_token_support = true;
  config.user_verification_succeeds = false;
  config.pin_support = false;
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      get_credential_options()->allow_credentials[0].id(),
      kTestRelyingPartyId));

  int expected_retries = 5;
  virtual_device_factory_->mutable_state()->uv_retries = expected_retries;
  virtual_device_factory_->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting([&](device::VirtualFidoDevice* device) {
        EXPECT_EQ(--expected_retries,
                  virtual_device_factory_->mutable_state()->uv_retries);
        return true;
      });

  EXPECT_EQ(AuthenticatorGetAssertion(get_credential_options()).status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
  EXPECT_EQ(0, expected_retries);
}

// Test exhausting all internal user verification attempts on an authenticator
// that supports PINs.
TEST_F(UVTokenAuthenticatorImplTest, GetAssertionFallBackToPin) {
  device::VirtualCtap2Device::Config config;
  config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
  config.internal_uv_support = true;
  config.pin_uv_auth_token_support = true;
  config.user_verification_succeeds = false;
  config.pin_support = true;
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      get_credential_options()->allow_credentials[0].id(),
      kTestRelyingPartyId));

  int taps = 0;
  virtual_device_factory_->mutable_state()->uv_retries = 5;
  virtual_device_factory_->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting([&](device::VirtualFidoDevice* device) {
        ++taps;
        return true;
      });

  EXPECT_EQ(AuthenticatorGetAssertion(get_credential_options()).status,
            AuthenticatorStatus::SUCCESS);
  // 5 retries + 1 tap for the actual get assertion request.
  EXPECT_EQ(taps, 6);
  EXPECT_TRUE(test_client_.collected_pin);
  EXPECT_EQ(device::kMinPinLength, test_client_.min_pin_length);
  EXPECT_EQ(5, virtual_device_factory_->mutable_state()->uv_retries);
}

// Tests that a device supporting UV token with UV blocked at the start of a get
// assertion request gets a touch and then falls back to PIN.
TEST_F(UVTokenAuthenticatorImplTest, GetAssertionUvBlockedFallBackToPin) {
  device::VirtualCtap2Device::Config config;
  config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
  config.internal_uv_support = true;
  config.pin_uv_auth_token_support = true;
  config.user_verification_succeeds = false;
  config.pin_support = true;

  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->uv_retries = 0;
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      get_credential_options()->allow_credentials[0].id(),
      kTestRelyingPartyId));

  EXPECT_EQ(AuthenticatorGetAssertion(get_credential_options()).status,
            AuthenticatorStatus::SUCCESS);
  EXPECT_TRUE(test_client_.collected_pin);
  EXPECT_EQ(device::kMinPinLength, test_client_.min_pin_length);
  EXPECT_EQ(5, virtual_device_factory_->mutable_state()->uv_retries);
}

TEST_F(UVTokenAuthenticatorImplTest, MakeCredentialUVToken) {
  for (const auto fingerprints_enrolled : {false, true}) {
    SCOPED_TRACE(::testing::Message()
                 << "fingerprints_enrolled=" << fingerprints_enrolled);
    virtual_device_factory_->mutable_state()->fingerprints_enrolled =
        fingerprints_enrolled;

    for (const auto uv : {device::UserVerificationRequirement::kDiscouraged,
                          device::UserVerificationRequirement::kPreferred,
                          device::UserVerificationRequirement::kRequired}) {
      SCOPED_TRACE(UVToString(uv));

      // UV cannot be satisfied without fingerprints.
      const bool should_timeout =
          !fingerprints_enrolled &&
          uv == device::UserVerificationRequirement::kRequired;

      if (should_timeout) {
        EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR,
                  AuthenticatorMakeCredentialAndWaitForTimeout(
                      make_credential_options(uv))
                      .status);
      } else {
        MakeCredentialResult result =
            AuthenticatorMakeCredential(make_credential_options(uv));
        EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
        EXPECT_EQ(fingerprints_enrolled, HasUV(result.response));
      }
    }
  }
}

// Test exhausting all internal user verification attempts on an authenticator
// that does not support PINs.
TEST_F(UVTokenAuthenticatorImplTest, MakeCredentialUvFails) {
  device::VirtualCtap2Device::Config config;
  config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
  config.internal_uv_support = true;
  config.pin_uv_auth_token_support = true;
  config.user_verification_succeeds = false;
  config.pin_support = false;
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      get_credential_options()->allow_credentials[0].id(),
      kTestRelyingPartyId));

  int expected_retries = 5;
  virtual_device_factory_->mutable_state()->uv_retries = expected_retries;
  virtual_device_factory_->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting([&](device::VirtualFidoDevice* device) {
        EXPECT_EQ(--expected_retries,
                  virtual_device_factory_->mutable_state()->uv_retries);
        return true;
      });

  EXPECT_EQ(AuthenticatorMakeCredential(make_credential_options()).status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
  EXPECT_EQ(0, expected_retries);
}

// Test exhausting all internal user verification attempts on an authenticator
// that supports PINs.
TEST_F(UVTokenAuthenticatorImplTest, MakeCredentialFallBackToPin) {
  device::VirtualCtap2Device::Config config;
  config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
  config.internal_uv_support = true;
  config.pin_uv_auth_token_support = true;
  config.user_verification_succeeds = false;
  config.pin_support = true;
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      get_credential_options()->allow_credentials[0].id(),
      kTestRelyingPartyId));

  int taps = 0;
  virtual_device_factory_->mutable_state()->uv_retries = 5;
  virtual_device_factory_->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting([&](device::VirtualFidoDevice* device) {
        ++taps;
        return true;
      });

  EXPECT_EQ(AuthenticatorMakeCredential(make_credential_options()).status,
            AuthenticatorStatus::SUCCESS);
  // 5 retries + 1 tap for the actual get assertion request.
  EXPECT_EQ(taps, 6);
  EXPECT_TRUE(test_client_.collected_pin);
  EXPECT_EQ(device::kMinPinLength, test_client_.min_pin_length);
  EXPECT_EQ(5, virtual_device_factory_->mutable_state()->uv_retries);
}

// Tests that a device supporting UV token with UV blocked at the start of a get
// assertion request gets a touch and then falls back to PIN.
TEST_F(UVTokenAuthenticatorImplTest, MakeCredentialUvBlockedFallBackToPin) {
  device::VirtualCtap2Device::Config config;
  config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
  config.internal_uv_support = true;
  config.pin_uv_auth_token_support = true;
  config.user_verification_succeeds = false;
  config.pin_support = true;

  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->uv_retries = 0;
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;
  virtual_device_factory_->mutable_state()->pin = kTestPIN;
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      get_credential_options()->allow_credentials[0].id(),
      kTestRelyingPartyId));

  EXPECT_EQ(AuthenticatorMakeCredential(make_credential_options()).status,
            AuthenticatorStatus::SUCCESS);
  EXPECT_TRUE(test_client_.collected_pin);
  EXPECT_EQ(device::kMinPinLength, test_client_.min_pin_length);
  EXPECT_EQ(5, virtual_device_factory_->mutable_state()->uv_retries);
}

class BlockingAuthenticatorRequestDelegate
    : public AuthenticatorRequestClientDelegate {
 public:
  explicit BlockingAuthenticatorRequestDelegate(
      base::OnceClosure* const callback)
      : callback_(callback) {}

  void RegisterActionCallbacks(
      base::OnceClosure cancel_callback,
      base::RepeatingClosure start_over_callback,
      device::FidoRequestHandlerBase::RequestCallback request_callback,
      base::RepeatingClosure bluetooth_adapter_power_on_callback) override {
    cancel_callback_ = std::move(cancel_callback);
  }

  bool DoesBlockRequestOnFailure(InterestingFailureReason reason) override {
    if (callback_ && *callback_) {
      std::move(*callback_).Run();
    }
    return true;
  }

  void Cancel() { std::move(cancel_callback_).Run(); }

 private:
  base::OnceClosure* const callback_;
  base::OnceClosure cancel_callback_;
};

class BlockingDelegateContentBrowserClient : public ContentBrowserClient {
 public:
  explicit BlockingDelegateContentBrowserClient(
      base::OnceClosure* const callback)
      : callback_(callback) {}

  WebAuthenticationDelegate* GetWebAuthenticationDelegate() override {
    return &web_authentication_delegate_;
  }

  std::unique_ptr<AuthenticatorRequestClientDelegate>
  GetWebAuthenticationRequestDelegate(
      RenderFrameHost* render_frame_host) override {
    auto ret =
        std::make_unique<BlockingAuthenticatorRequestDelegate>(callback_);
    delegate_ = ret.get();
    return ret;
  }

  void Cancel() { delegate_->Cancel(); }

 private:
  TestWebAuthenticationDelegate web_authentication_delegate_;
  base::OnceClosure* const callback_;
  BlockingAuthenticatorRequestDelegate* delegate_ = nullptr;
};

class BlockingDelegateAuthenticatorImplTest : public AuthenticatorImplTest {
 public:
  BlockingDelegateAuthenticatorImplTest() = default;

  BlockingDelegateAuthenticatorImplTest(
      const BlockingDelegateAuthenticatorImplTest&) = delete;
  BlockingDelegateAuthenticatorImplTest& operator=(
      const BlockingDelegateAuthenticatorImplTest&) = delete;

  void SetUp() override {
    AuthenticatorImplTest::SetUp();
    old_client_ = SetBrowserClientForTesting(&test_client_);
    NavigateAndCommit(GURL(kTestOrigin1));
  }

  void TearDown() override {
    SetBrowserClientForTesting(old_client_);
    AuthenticatorImplTest::TearDown();
  }

 protected:
  base::OnceClosure blocked_on_error_callback_;
  BlockingDelegateContentBrowserClient test_client_{
      &blocked_on_error_callback_};

 private:
  ContentBrowserClient* old_client_ = nullptr;
};

TEST_F(BlockingDelegateAuthenticatorImplTest, PostCancelMessage) {
  // Create a fingerprint-reading device and a UP-only device. Advance the
  // first till it's waiting for a fingerprint then simulate a touch on the
  // UP device that claims that it failed due to an excluded credential.
  // When the error is showing in the UI, have the fingerprint device resolve
  // the UV with an error. Don't crash (crbug.com/1225899).

  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  options->exclude_credentials = GetTestCredentials();

  device::test::MultipleVirtualFidoDeviceFactory::DeviceDetails device_1;
  scoped_refptr<VirtualFidoDevice::State> state_1 = device_1.state;
  device_1.state->simulate_press_callback =
      base::BindLambdaForTesting([&](VirtualFidoDevice* _) -> bool {
        // Drop all makeCredential requests. The reply will be sent when
        // the second authenticator is asked for a fingerprint.
        return false;
      });

  device::test::MultipleVirtualFidoDeviceFactory::DeviceDetails device_2;
  scoped_refptr<VirtualFidoDevice::State> state_2 = device_2.state;
  device_2.config.internal_uv_support = true;
  device_2.config.pin_support = true;
  device_2.config.pin_uv_auth_token_support = true;
  device_2.config.ctap2_versions = {device::Ctap2Version::kCtap2_1};
  device_2.state->pin = kTestPIN;
  device_2.state->fingerprints_enrolled = true;
  device_2.state->uv_retries = 8;
  device_2.state->simulate_press_callback =
      base::BindLambdaForTesting([&](VirtualFidoDevice* _) -> bool {
        // If asked for a fingerprint, fail the makeCredential request by
        // simulating a matched excluded credential by the other authenticator.
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(std::move(state_1->transact_callback),
                                      std::vector<uint8_t>{static_cast<uint8_t>(
                                          device::CtapDeviceResponseCode::
                                              kCtap2ErrCredentialExcluded)}));
        return false;
      });

  auto discovery =
      std::make_unique<device::test::MultipleVirtualFidoDeviceFactory>();
  discovery->AddDevice(std::move(device_1));
  discovery->AddDevice(std::move(device_2));
  AuthenticatorEnvironmentImpl::GetInstance()
      ->ReplaceDefaultDiscoveryFactoryForTesting(std::move(discovery));

  blocked_on_error_callback_ = base::BindLambdaForTesting([&]() {
    // When the UI should show an error, have the second authenticator reply
    // to the fingerprint touch and cancel the transaction.
    std::move(state_2->transact_callback)
        .Run(std::vector<uint8_t>{static_cast<uint8_t>(
            device::CtapDeviceResponseCode::kCtap2ErrOperationDenied)});
    test_client_.Cancel();
  });

  EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
            AuthenticatorStatus::CREDENTIAL_EXCLUDED);
}

// ResidentKeyTestAuthenticatorRequestDelegate is a delegate that:
//   a) always returns |kTestPIN| when asked for a PIN.
//   b) sorts potential resident-key accounts by user ID, maps them to a string
//      form ("<hex user ID>:<user name>:<display name>"), joins the strings
//      with "/", and compares the result against |expected_accounts|.
//   c) auto-selects the account with the user ID matching |selected_user_id|.
class ResidentKeyTestAuthenticatorRequestDelegate
    : public AuthenticatorRequestClientDelegate {
 public:
  ResidentKeyTestAuthenticatorRequestDelegate(
      std::string expected_accounts,
      std::vector<uint8_t> selected_user_id,
      absl::optional<InterestingFailureReason>* failure_reason,
      bool* is_conditional)
      : expected_accounts_(expected_accounts),
        selected_user_id_(selected_user_id),
        failure_reason_(failure_reason),
        is_conditional_(is_conditional) {}

  ResidentKeyTestAuthenticatorRequestDelegate(
      const ResidentKeyTestAuthenticatorRequestDelegate&) = delete;
  ResidentKeyTestAuthenticatorRequestDelegate& operator=(
      const ResidentKeyTestAuthenticatorRequestDelegate&) = delete;

  bool SupportsPIN() const override { return true; }

  void CollectPIN(
      CollectPINOptions options,
      base::OnceCallback<void(std::u16string)> provide_pin_cb) override {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(provide_pin_cb), kTestPIN16));
  }

  void FinishCollectToken() override {}

  void SelectAccount(
      std::vector<device::AuthenticatorGetAssertionResponse> responses,
      base::OnceCallback<void(device::AuthenticatorGetAssertionResponse)>
          callback) override {
    std::sort(responses.begin(), responses.end(),
              [](const device::AuthenticatorGetAssertionResponse& a,
                 const device::AuthenticatorGetAssertionResponse& b) {
                return a.user_entity->id < b.user_entity->id;
              });

    std::vector<std::string> string_reps;
    std::transform(
        responses.begin(), responses.end(), std::back_inserter(string_reps),
        [](const device::AuthenticatorGetAssertionResponse& response) {
          const device::PublicKeyCredentialUserEntity& user =
              response.user_entity.value();
          return base::HexEncode(user.id.data(), user.id.size()) + ":" +
                 user.name.value_or("") + ":" + user.display_name.value_or("");
        });

    EXPECT_EQ(expected_accounts_, base::JoinString(string_reps, "/"));

    const auto selected = std::find_if(
        responses.begin(), responses.end(),
        [this](const device::AuthenticatorGetAssertionResponse& response) {
          return response.user_entity->id == selected_user_id_;
        });
    ASSERT_TRUE(selected != responses.end());

    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::move(*selected)));
  }

  bool DoesBlockRequestOnFailure(InterestingFailureReason reason) override {
    *failure_reason_ = reason;
    return AuthenticatorRequestClientDelegate::DoesBlockRequestOnFailure(
        reason);
  }

  void SetConditionalRequest(bool is_conditional) override {
    *is_conditional_ = is_conditional;
  }

 private:
  const std::string expected_accounts_;
  const std::vector<uint8_t> selected_user_id_;
  absl::optional<InterestingFailureReason>* const failure_reason_;
  bool* const is_conditional_;
};

class ResidentKeyTestAuthenticatorContentBrowserClient
    : public ContentBrowserClient {
 public:
  ResidentKeyTestAuthenticatorContentBrowserClient() {
    web_authentication_delegate.supports_resident_keys = true;
  }

  WebAuthenticationDelegate* GetWebAuthenticationDelegate() override {
    return &web_authentication_delegate;
  }

  std::unique_ptr<AuthenticatorRequestClientDelegate>
  GetWebAuthenticationRequestDelegate(
      RenderFrameHost* render_frame_host) override {
    return std::make_unique<ResidentKeyTestAuthenticatorRequestDelegate>(
        expected_accounts, selected_user_id, &failure_reason, &is_conditional);
  }

  TestWebAuthenticationDelegate web_authentication_delegate;

  std::string expected_accounts;
  std::vector<uint8_t> selected_user_id;
  bool is_conditional = false;
  absl::optional<AuthenticatorRequestClientDelegate::InterestingFailureReason>
      failure_reason;
};

class ResidentKeyAuthenticatorImplTest : public UVAuthenticatorImplTest {
 public:
  ResidentKeyAuthenticatorImplTest() = default;

  void SetUp() override {
    UVAuthenticatorImplTest::SetUp();
    old_client_ = SetBrowserClientForTesting(&test_client_);
    device::VirtualCtap2Device::Config config;
    config.pin_support = true;
    config.resident_key_support = true;
    virtual_device_factory_->SetCtap2Config(config);
    virtual_device_factory_->mutable_state()->pin = kTestPIN;
    virtual_device_factory_->mutable_state()->pin_retries =
        device::kMaxPinRetries;
    NavigateAndCommit(GURL(kTestOrigin1));
  }

  void TearDown() override {
    SetBrowserClientForTesting(old_client_);
    UVAuthenticatorImplTest::TearDown();
  }

 protected:
  ResidentKeyTestAuthenticatorContentBrowserClient test_client_;

  static PublicKeyCredentialCreationOptionsPtr make_credential_options(
      device::ResidentKeyRequirement resident_key =
          device::ResidentKeyRequirement::kRequired) {
    PublicKeyCredentialCreationOptionsPtr options =
        UVAuthenticatorImplTest::make_credential_options();
    options->authenticator_selection->SetResidentKeyForTesting(resident_key);
    options->user.id = {1, 2, 3, 4};
    return options;
  }

  static PublicKeyCredentialRequestOptionsPtr get_credential_options() {
    PublicKeyCredentialRequestOptionsPtr options =
        UVAuthenticatorImplTest::get_credential_options();
    options->allow_credentials.clear();
    return options;
  }

 private:
  ContentBrowserClient* old_client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(ResidentKeyAuthenticatorImplTest);
};

TEST_F(ResidentKeyAuthenticatorImplTest, MakeCredentialRkRequired) {
  for (const bool internal_uv : {false, true}) {
    SCOPED_TRACE(::testing::Message() << "internal_uv=" << internal_uv);

    if (internal_uv) {
      device::VirtualCtap2Device::Config config;
      config.resident_key_support = true;
      config.internal_uv_support = true;
      virtual_device_factory_->SetCtap2Config(config);
      virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;
    }

    MakeCredentialResult result =
        AuthenticatorMakeCredential(make_credential_options());

    EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
    EXPECT_TRUE(HasUV(result.response));
    ASSERT_EQ(1u,
              virtual_device_factory_->mutable_state()->registrations.size());
    const device::VirtualFidoDevice::RegistrationData& registration =
        virtual_device_factory_->mutable_state()->registrations.begin()->second;
    EXPECT_TRUE(registration.is_resident);
    ASSERT_TRUE(registration.user.has_value());
    const auto options = make_credential_options();
    EXPECT_EQ(options->user.name, registration.user->name);
    EXPECT_EQ(options->user.display_name, registration.user->display_name);
    EXPECT_EQ(options->user.id, registration.user->id);
    EXPECT_EQ(options->user.icon_url, registration.user->icon_url);
  }
}

TEST_F(ResidentKeyAuthenticatorImplTest, MakeCredentialRkPreferred) {
  for (const bool supports_rk : {false, true}) {
    SCOPED_TRACE(::testing::Message() << "supports_rk=" << supports_rk);
    ResetVirtualDevice();

    device::VirtualCtap2Device::Config config;
    config.internal_uv_support = true;
    config.resident_key_support = supports_rk;
    virtual_device_factory_->SetCtap2Config(config);
    virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;

    MakeCredentialResult result = AuthenticatorMakeCredential(
        make_credential_options(device::ResidentKeyRequirement::kPreferred));

    ASSERT_EQ(AuthenticatorStatus::SUCCESS, result.status);
    EXPECT_TRUE(HasUV(result.response));
    ASSERT_EQ(1u,
              virtual_device_factory_->mutable_state()->registrations.size());
    const device::VirtualFidoDevice::RegistrationData& registration =
        virtual_device_factory_->mutable_state()->registrations.begin()->second;
    EXPECT_EQ(registration.is_resident, supports_rk);
  }
}

TEST_F(ResidentKeyAuthenticatorImplTest, MakeCredentialRkPreferredStorageFull) {
  // Making a credential on an authenticator with full storage falls back to
  // making a non-resident key.
  for (bool is_ctap_2_1 : {false, true}) {
    ResetVirtualDevice();

    size_t num_taps = 0;
    virtual_device_factory_->mutable_state()->simulate_press_callback =
        base::BindLambdaForTesting(
            [&num_taps](device::VirtualFidoDevice* device) {
              num_taps++;
              return true;
            });

    device::VirtualCtap2Device::Config config;
    if (is_ctap_2_1) {
      config.ctap2_versions = {std::begin(device::kCtap2Versions2_1),
                               std::end(device::kCtap2Versions2_1)};
    }

    config.internal_uv_support = true;
    config.resident_key_support = true;
    config.resident_credential_storage = 0;
    virtual_device_factory_->SetCtap2Config(config);
    virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;

    MakeCredentialResult result = AuthenticatorMakeCredential(
        make_credential_options(device::ResidentKeyRequirement::kPreferred));

    ASSERT_EQ(AuthenticatorStatus::SUCCESS, result.status);
    EXPECT_TRUE(HasUV(result.response));
    ASSERT_EQ(1u,
              virtual_device_factory_->mutable_state()->registrations.size());
    const device::VirtualFidoDevice::RegistrationData& registration =
        virtual_device_factory_->mutable_state()->registrations.begin()->second;
    EXPECT_EQ(registration.is_resident, false);
    // In CTAP 2.0, the first request with rk=false fails due to exhausted
    // storage and then needs to be retried with rk=false, requiring a second
    // tap. In 2.1 remaining storage capacity can be checked up front such that
    // the request is sent with rk=false right away.
    EXPECT_EQ(num_taps, is_ctap_2_1 ? 1u : 2u);
  }
}

TEST_F(ResidentKeyAuthenticatorImplTest, MakeCredentialRkPreferredSetsPIN) {
  device::VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.internal_uv_support = false;
  config.resident_key_support = true;
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->pin = "";

  MakeCredentialResult result = AuthenticatorMakeCredential(
      make_credential_options(device::ResidentKeyRequirement::kPreferred));

  EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
  EXPECT_TRUE(HasUV(result.response));
  ASSERT_EQ(1u, virtual_device_factory_->mutable_state()->registrations.size());
  const device::VirtualFidoDevice::RegistrationData& registration =
      virtual_device_factory_->mutable_state()->registrations.begin()->second;
  EXPECT_EQ(registration.is_resident, true);
  EXPECT_EQ(virtual_device_factory_->mutable_state()->pin, kTestPIN);
}

TEST_F(ResidentKeyAuthenticatorImplTest, StorageFull) {
  device::VirtualCtap2Device::Config config;
  config.resident_key_support = true;
  config.internal_uv_support = true;
  config.resident_credential_storage = 1;
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;

  // Add a resident key to fill the authenticator.
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      /*credential_id=*/{{4, 3, 2, 1}}, kTestRelyingPartyId,
      /*user_id=*/{{1, 1, 1, 1}}, "test@example.com", "Test User"));

  EXPECT_EQ(AuthenticatorMakeCredential(make_credential_options()).status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
  ASSERT_TRUE(test_client_.failure_reason.has_value());
  EXPECT_EQ(AuthenticatorRequestClientDelegate::InterestingFailureReason::
                kStorageFull,
            test_client_.failure_reason);
}

TEST_F(ResidentKeyAuthenticatorImplTest, GetAssertionSingleNoPII) {
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      /*credential_id=*/{{4, 3, 2, 1}}, kTestRelyingPartyId,
      /*user_id=*/{{1, 2, 3, 4}}, absl::nullopt, absl::nullopt));

  // |SelectAccount| should not be called when there's only a single response
  // with no identifying user info because the UI is bad in that case: we can
  // only display the single choice of "Unknown user".
  test_client_.expected_accounts = "<invalid>";
  GetAssertionResult result =
      AuthenticatorGetAssertion(get_credential_options());

  EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
  EXPECT_TRUE(HasUV(result.response));
}

TEST_F(ResidentKeyAuthenticatorImplTest, GetAssertionSingleWithPII) {
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      /*credential_id=*/{{4, 3, 2, 1}}, kTestRelyingPartyId,
      /*user_id=*/{{1, 2, 3, 4}}, absl::nullopt, "Test User"));

  // |SelectAccount| should be called when PII is available.
  test_client_.expected_accounts = "01020304::Test User";
  test_client_.selected_user_id = {1, 2, 3, 4};
  GetAssertionResult result =
      AuthenticatorGetAssertion(get_credential_options());
  EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
  EXPECT_TRUE(HasUV(result.response));
}

TEST_F(ResidentKeyAuthenticatorImplTest, GetAssertionMulti) {
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      /*credential_id=*/{{4, 3, 2, 1}}, kTestRelyingPartyId,
      /*user_id=*/{{1, 2, 3, 4}}, "test@example.com", "Test User"));
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      /*credential_id=*/{{4, 3, 2, 2}}, kTestRelyingPartyId,
      /*user_id=*/{{5, 6, 7, 8}}, "test2@example.com", "Test User 2"));

  test_client_.expected_accounts =
      "01020304:test@example.com:Test User/"
      "05060708:test2@example.com:Test User 2";
  test_client_.selected_user_id = {1, 2, 3, 4};

  GetAssertionResult result =
      AuthenticatorGetAssertion(get_credential_options());

  EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
  EXPECT_TRUE(HasUV(result.response));
}

TEST_F(ResidentKeyAuthenticatorImplTest, GetAssertionUVDiscouraged) {
  device::VirtualCtap2Device::Config config;
  config.resident_key_support = true;
  config.internal_uv_support = true;
  config.u2f_support = true;
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;

  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      /*credential_id=*/{{4, 3, 2, 1}}, kTestRelyingPartyId,
      /*user_id=*/{{1, 2, 3, 4}}, absl::nullopt, absl::nullopt));

  // |SelectAccount| should not be called when there's only a single response
  // without identifying information.
  test_client_.expected_accounts = "<invalid>";
  PublicKeyCredentialRequestOptionsPtr options(get_credential_options());
  options->user_verification =
      device::UserVerificationRequirement::kDiscouraged;

  GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));

  EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
  // The UV=discouraged should have been ignored for a resident-credential
  // request.
  EXPECT_TRUE(HasUV(result.response));
}

static const char* BlobSupportDescription(device::LargeBlobSupport support) {
  switch (support) {
    case device::LargeBlobSupport::kNotRequested:
      return "Blob not requested";
    case device::LargeBlobSupport::kPreferred:
      return "Blob preferred";
    case device::LargeBlobSupport::kRequired:
      return "Blob required";
  }
}

TEST_F(ResidentKeyAuthenticatorImplTest, MakeCredentialLargeBlob) {
  const auto BlobRequired = device::LargeBlobSupport::kRequired;
  const auto BlobPreferred = device::LargeBlobSupport::kPreferred;
  const auto BlobNotRequested = device::LargeBlobSupport::kNotRequested;

  constexpr struct {
    bool large_blob_support;
    bool rk_required;
    device::LargeBlobSupport large_blob_enable;
    bool request_success;
    bool did_create_large_blob;
  } kLargeBlobTestCases[] = {
      // clang-format off
      // support, rk,    enabled,          success, did create
      { true,     true,  BlobRequired,     true,    true},
      { true,     true,  BlobPreferred,    true,    true},
      { true,     true,  BlobNotRequested, true,    false},
      { true,     false, BlobRequired,     false,   false},
      { true,     false, BlobPreferred,    true,    false},
      { true,     true,  BlobNotRequested, true,    false},
      { false,    true,  BlobRequired,     false,   false},
      { false,    true,  BlobPreferred,    true,    false},
      { true,     true,  BlobNotRequested, true,    false},
      // clang-format on
  };
  for (auto& test : kLargeBlobTestCases) {
    SCOPED_TRACE(::testing::Message() << "support=" << test.large_blob_support);
    SCOPED_TRACE(::testing::Message() << "rk_required=" << test.rk_required);
    SCOPED_TRACE(::testing::Message()
                 << "enabled="
                 << BlobSupportDescription(test.large_blob_enable));
    SCOPED_TRACE(::testing::Message() << "success=" << test.request_success);
    SCOPED_TRACE(::testing::Message()
                 << "did create=" << test.did_create_large_blob);

    device::VirtualCtap2Device::Config config;
    config.pin_support = true;
    config.pin_uv_auth_token_support = true;
    config.resident_key_support = true;
    config.ctap2_versions = {std::begin(device::kCtap2Versions2_1),
                             std::end(device::kCtap2Versions2_1)};
    config.large_blob_support = test.large_blob_support;
    virtual_device_factory_->SetCtap2Config(config);

    PublicKeyCredentialCreationOptionsPtr options = make_credential_options(
        test.rk_required ? device::ResidentKeyRequirement::kRequired
                         : device::ResidentKeyRequirement::kDiscouraged);
    options->large_blob_enable = test.large_blob_enable;
    MakeCredentialResult result =
        AuthenticatorMakeCredential(std::move(options));

    if (test.request_success) {
      ASSERT_EQ(AuthenticatorStatus::SUCCESS, result.status);
      ASSERT_EQ(1u,
                virtual_device_factory_->mutable_state()->registrations.size());
      const device::VirtualFidoDevice::RegistrationData& registration =
          virtual_device_factory_->mutable_state()
              ->registrations.begin()
              ->second;
      EXPECT_EQ(test.did_create_large_blob,
                registration.large_blob_key.has_value());
      EXPECT_EQ(test.large_blob_enable != BlobNotRequested,
                result.response->echo_large_blob);
      EXPECT_EQ(test.did_create_large_blob,
                result.response->supports_large_blob);
    } else {
      ASSERT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, result.status);
      ASSERT_EQ(0u,
                virtual_device_factory_->mutable_state()->registrations.size());
    }
    virtual_device_factory_->mutable_state()->registrations.clear();
    virtual_device_factory_->mutable_state()->ClearLargeBlobs();
  }
}

TEST_F(ResidentKeyAuthenticatorImplTest, GetAssertionLargeBlobRead) {
  constexpr struct {
    bool large_blob_support;
    bool large_blob_set;
    bool large_blob_key_set;
    bool did_read_large_blob;
  } kLargeBlobTestCases[] = {
      // clang-format off
      // support,  set,   key_set, did_read
       { true,     true,  true,    true  },
       { true,     false, false,   false },
       { true,     false, true,    false },
       { false,    false, false,   false },
      // clang-format on
  };
  for (auto& test : kLargeBlobTestCases) {
    SCOPED_TRACE(::testing::Message() << "support=" << test.large_blob_support);
    SCOPED_TRACE(::testing::Message() << "set=" << test.large_blob_set);
    SCOPED_TRACE(::testing::Message() << "key_set=" << test.large_blob_key_set);
    SCOPED_TRACE(::testing::Message()
                 << "did_read=" << test.did_read_large_blob);

    const std::vector<uint8_t> large_blob = {'b', 'l', 'o', 'b'};
    device::VirtualCtap2Device::Config config;
    config.pin_support = true;
    config.pin_uv_auth_token_support = true;
    config.resident_key_support = true;
    config.ctap2_versions = {std::begin(device::kCtap2Versions2_1),
                             std::end(device::kCtap2Versions2_1)};
    config.large_blob_support = test.large_blob_support;
    virtual_device_factory_->SetCtap2Config(config);
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
        /*credential_id=*/{{4, 3, 2, 1}}, kTestRelyingPartyId,
        /*user_id=*/{{1, 2, 3, 4}}, absl::nullopt, absl::nullopt));

    if (test.large_blob_set) {
      virtual_device_factory_->mutable_state()->InjectLargeBlob(
          &virtual_device_factory_->mutable_state()
               ->registrations.begin()
               ->second,
          CompressLargeBlob(large_blob));
    } else if (test.large_blob_key_set) {
      virtual_device_factory_->mutable_state()
          ->registrations.begin()
          ->second.large_blob_key = {{0}};
    }

    PublicKeyCredentialRequestOptionsPtr options = get_credential_options();
    options->large_blob_read = true;
    GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));

    ASSERT_EQ(AuthenticatorStatus::SUCCESS, result.status);
    EXPECT_TRUE(result.response->echo_large_blob);
    EXPECT_FALSE(result.response->echo_large_blob_written);
    if (test.did_read_large_blob) {
      EXPECT_EQ(large_blob, *result.response->large_blob);
    } else {
      EXPECT_FALSE(result.response->large_blob.has_value());
    }
    virtual_device_factory_->mutable_state()->registrations.clear();
    virtual_device_factory_->mutable_state()->ClearLargeBlobs();
  }
}

TEST_F(ResidentKeyAuthenticatorImplTest, GetAssertionLargeBlobWrite) {
  constexpr struct {
    bool large_blob_support;
    bool large_blob_set;
    bool large_blob_key_set;
    bool did_write_large_blob;
  } kLargeBlobTestCases[] = {
      // clang-format off
      // support,  set,   key_set, did_write
       { true,     true,  true,    true  },
       { true,     false, false,   false },
       { true,     false, true,    true  },
       { false,    false, false,   false },
      // clang-format on
  };
  for (auto& test : kLargeBlobTestCases) {
    SCOPED_TRACE(::testing::Message() << "support=" << test.large_blob_support);
    SCOPED_TRACE(::testing::Message() << "set=" << test.large_blob_set);
    SCOPED_TRACE(::testing::Message() << "key_set=" << test.large_blob_key_set);
    SCOPED_TRACE(::testing::Message()
                 << "did_write=" << test.did_write_large_blob);

    const std::vector<uint8_t> large_blob = {'b', 'l', 'o', 'b'};
    device::VirtualCtap2Device::Config config;
    config.pin_support = true;
    config.pin_uv_auth_token_support = true;
    config.resident_key_support = true;
    config.ctap2_versions = {std::begin(device::kCtap2Versions2_1),
                             std::end(device::kCtap2Versions2_1)};
    config.large_blob_support = test.large_blob_support;
    virtual_device_factory_->SetCtap2Config(config);
    const std::vector<uint8_t> cred_id = {4, 3, 2, 1};
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
        cred_id, kTestRelyingPartyId,
        /*user_id=*/{{1, 2, 3, 4}}, absl::nullopt, absl::nullopt));

    if (test.large_blob_set) {
      virtual_device_factory_->mutable_state()->InjectLargeBlob(
          &virtual_device_factory_->mutable_state()
               ->registrations.begin()
               ->second,
          CompressLargeBlob(large_blob));
    } else if (test.large_blob_key_set) {
      virtual_device_factory_->mutable_state()
          ->registrations.begin()
          ->second.large_blob_key = {{0}};
    }

    PublicKeyCredentialRequestOptionsPtr options = get_credential_options();
    options->allow_credentials = {device::PublicKeyCredentialDescriptor(
        device::CredentialType::kPublicKey, cred_id)};
    options->large_blob_write = large_blob;
    GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));

    ASSERT_EQ(AuthenticatorStatus::SUCCESS, result.status);
    EXPECT_TRUE(result.response->echo_large_blob);
    EXPECT_FALSE(result.response->large_blob.has_value());
    EXPECT_TRUE(result.response->echo_large_blob_written);
    EXPECT_EQ(test.did_write_large_blob, result.response->large_blob_written);
    if (test.did_write_large_blob) {
      absl::optional<std::vector<uint8_t>> compressed_blob =
          virtual_device_factory_->mutable_state()->GetLargeBlob(
              virtual_device_factory_->mutable_state()
                  ->registrations.begin()
                  ->second);
      EXPECT_EQ(large_blob, UncompressLargeBlob(*compressed_blob));
    }
    virtual_device_factory_->mutable_state()->registrations.clear();
    virtual_device_factory_->mutable_state()->ClearLargeBlobs();
  }
}

static const char* ProtectionPolicyDescription(
    blink::mojom::ProtectionPolicy p) {
  switch (p) {
    case blink::mojom::ProtectionPolicy::UNSPECIFIED:
      return "UNSPECIFIED";
    case blink::mojom::ProtectionPolicy::NONE:
      return "NONE";
    case blink::mojom::ProtectionPolicy::UV_OR_CRED_ID_REQUIRED:
      return "UV_OR_CRED_ID_REQUIRED";
    case blink::mojom::ProtectionPolicy::UV_REQUIRED:
      return "UV_REQUIRED";
  }
}

static const char* CredProtectDescription(device::CredProtect cred_protect) {
  switch (cred_protect) {
    case device::CredProtect::kUVOptional:
      return "UV optional";
    case device::CredProtect::kUVOrCredIDRequired:
      return "UV or cred ID required";
    case device::CredProtect::kUVRequired:
      return "UV required";
  }
}

TEST_F(ResidentKeyAuthenticatorImplTest, CredProtectRegistration) {
  const auto UNSPECIFIED = blink::mojom::ProtectionPolicy::UNSPECIFIED;
  const auto NONE = blink::mojom::ProtectionPolicy::NONE;
  const auto UV_OR_CRED =
      blink::mojom::ProtectionPolicy::UV_OR_CRED_ID_REQUIRED;
  const auto UV_REQ = blink::mojom::ProtectionPolicy::UV_REQUIRED;
  const int kOk = 0;
  const int kNonsense = 1;
  const int kNotAllow = 2;

  const struct {
    bool supported_by_authenticator;
    bool is_resident;
    blink::mojom::ProtectionPolicy protection;
    bool enforce;
    bool uv;
    int expected_outcome;
    blink::mojom::ProtectionPolicy resulting_policy;
  } kExpectations[] = {
      // clang-format off
    // Support | Resdnt | Level      | Enf  |  UV  || Result   | Prot level
    {  false,   false,   UNSPECIFIED, false, false,   kOk,       NONE},
    {  false,   false,   UNSPECIFIED, true,  false,   kNonsense, UNSPECIFIED},
    {  false,   false,   NONE,        false, false,   kNonsense, UNSPECIFIED},
    {  false,   false,   NONE,        true,  false,   kNonsense, UNSPECIFIED},
    {  false,   false,   UV_OR_CRED,  false, false,   kOk,       NONE},
    {  false,   false,   UV_OR_CRED,  true,  false,   kNotAllow, UNSPECIFIED},
    {  false,   false,   UV_OR_CRED,  false, true,    kOk,       NONE},
    {  false,   false,   UV_OR_CRED,  true,  true,    kNotAllow, UNSPECIFIED},
    {  false,   false,   UV_REQ,      false, false,   kNonsense, UNSPECIFIED},
    {  false,   false,   UV_REQ,      false, true,    kOk,       NONE},
    {  false,   false,   UV_REQ,      true,  false,   kNonsense, UNSPECIFIED},
    {  false,   false,   UV_REQ,      true,  true,    kNotAllow, UNSPECIFIED},
    {  false,   true,    UNSPECIFIED, false, false,   kOk,       NONE},
    {  false,   true,    UNSPECIFIED, true,  false,   kNonsense, UNSPECIFIED},
    {  false,   true,    NONE,        false, false,   kOk,       NONE},
    {  false,   true,    NONE,        true,  false,   kNonsense, UNSPECIFIED},
    {  false,   true,    UV_OR_CRED,  false, false,   kOk,       NONE},
    {  false,   true,    UV_OR_CRED,  true,  false,   kNotAllow, UNSPECIFIED},
    {  false,   true,    UV_REQ,      false, false,   kNonsense, UNSPECIFIED},
    {  false,   true,    UV_REQ,      false, true,    kOk,       NONE},
    {  false,   true,    UV_REQ,      true,  false,   kNonsense, UNSPECIFIED},
    {  false,   true,    UV_REQ,      true,  true,    kNotAllow, UNSPECIFIED},

    // For the case where the authenticator supports credProtect we do not
    // repeat the cases above that are |kNonsense| on the assumption that
    // authenticator support is irrelevant. Therefore these are just the non-
    // kNonsense cases from the prior block.
    {  true,    false,   UNSPECIFIED, false, false,   kOk,       NONE},
    {  true,    false,   UV_OR_CRED,  false, false,   kOk,       UV_OR_CRED},
    {  true,    false,   UV_OR_CRED,  true,  false,   kOk,       UV_OR_CRED},
    {  true,    false,   UV_OR_CRED,  false, true,    kOk,       UV_OR_CRED},
    {  true,    false,   UV_OR_CRED,  true,  true,    kOk,       UV_OR_CRED},
    {  true,    false,   UV_REQ,      false, true,    kOk,       UV_REQ},
    {  true,    false,   UV_REQ,      true,  true,    kOk,       UV_REQ},
    {  true,    true,    UNSPECIFIED, false, false,   kOk,       UV_OR_CRED},
    {  true,    true,    NONE,        false, false,   kOk,       NONE},
    {  true,    true,    UV_OR_CRED,  false, false,   kOk,       UV_OR_CRED},
    {  true,    true,    UV_OR_CRED,  true,  false,   kOk,       UV_OR_CRED},
    {  true,    true,    UV_REQ,      false, true,    kOk,       UV_REQ},
    {  true,    true,    UV_REQ,      true,  true,    kOk,       UV_REQ},
      // clang-format on
  };

  for (const auto& test : kExpectations) {
    device::VirtualCtap2Device::Config config;
    config.pin_support = true;
    config.resident_key_support = true;
    config.cred_protect_support = test.supported_by_authenticator;
    virtual_device_factory_->SetCtap2Config(config);
    virtual_device_factory_->mutable_state()->registrations.clear();

    SCOPED_TRACE(::testing::Message() << "uv=" << test.uv);
    SCOPED_TRACE(::testing::Message() << "enforce=" << test.enforce);
    SCOPED_TRACE(::testing::Message()
                 << "level=" << ProtectionPolicyDescription(test.protection));
    SCOPED_TRACE(::testing::Message() << "resident=" << test.is_resident);
    SCOPED_TRACE(::testing::Message()
                 << "support=" << test.supported_by_authenticator);

    PublicKeyCredentialCreationOptionsPtr options = make_credential_options();
    options->authenticator_selection->SetResidentKeyForTesting(
        test.is_resident ? device::ResidentKeyRequirement::kRequired
                         : device::ResidentKeyRequirement::kDiscouraged);
    options->protection_policy = test.protection;
    options->enforce_protection_policy = test.enforce;
    options->authenticator_selection->SetUserVerificationRequirementForTesting(
        test.uv ? device::UserVerificationRequirement::kRequired
                : device::UserVerificationRequirement::kDiscouraged);

    AuthenticatorStatus status =
        AuthenticatorMakeCredential(std::move(options)).status;

    switch (test.expected_outcome) {
      case kOk: {
        EXPECT_EQ(AuthenticatorStatus::SUCCESS, status);
        ASSERT_EQ(
            1u, virtual_device_factory_->mutable_state()->registrations.size());
        const device::CredProtect result =
            virtual_device_factory_->mutable_state()
                ->registrations.begin()
                ->second.protection;

        switch (test.resulting_policy) {
          case UNSPECIFIED:
            NOTREACHED();
            break;
          case NONE:
            EXPECT_EQ(device::CredProtect::kUVOptional, result);
            break;
          case UV_OR_CRED:
            EXPECT_EQ(device::CredProtect::kUVOrCredIDRequired, result);
            break;
          case UV_REQ:
            EXPECT_EQ(device::CredProtect::kUVRequired, result);
            break;
        }
        break;
      }
      case kNonsense:
        EXPECT_EQ(AuthenticatorStatus::PROTECTION_POLICY_INCONSISTENT, status);
        break;
      case kNotAllow:
        EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, status);
        break;
      default:
        NOTREACHED();
    }
  }
}

TEST_F(ResidentKeyAuthenticatorImplTest, AuthenticatorSetsCredProtect) {
  // Some authenticators are expected to set the credProtect extension ad
  // libitum. Therefore we should only require that the returned extension is at
  // least as restrictive as requested, but perhaps not exactly equal.
  constexpr blink::mojom::ProtectionPolicy kMojoLevels[] = {
      blink::mojom::ProtectionPolicy::NONE,
      blink::mojom::ProtectionPolicy::UV_OR_CRED_ID_REQUIRED,
      blink::mojom::ProtectionPolicy::UV_REQUIRED,
  };
  constexpr device::CredProtect kDeviceLevels[] = {
      device::CredProtect::kUVOptional,
      device::CredProtect::kUVOrCredIDRequired,
      device::CredProtect::kUVRequired,
  };

  for (int requested_level = 0; requested_level < 3; requested_level++) {
    for (int forced_level = 1; forced_level < 3; forced_level++) {
      SCOPED_TRACE(::testing::Message() << "requested=" << requested_level);
      SCOPED_TRACE(::testing::Message() << "forced=" << forced_level);
      device::VirtualCtap2Device::Config config;
      config.pin_support = true;
      config.resident_key_support = true;
      config.cred_protect_support = true;
      config.force_cred_protect = kDeviceLevels[forced_level];
      virtual_device_factory_->SetCtap2Config(config);
      virtual_device_factory_->mutable_state()->registrations.clear();

      PublicKeyCredentialCreationOptionsPtr options = make_credential_options();
      options->authenticator_selection->SetResidentKeyForTesting(
          device::ResidentKeyRequirement::kRequired);
      options->protection_policy = kMojoLevels[requested_level];
      options->authenticator_selection
          ->SetUserVerificationRequirementForTesting(
              device::UserVerificationRequirement::kRequired);

      AuthenticatorStatus status =
          AuthenticatorMakeCredential(std::move(options)).status;

      if (requested_level <= forced_level) {
        EXPECT_EQ(AuthenticatorStatus::SUCCESS, status);
        ASSERT_EQ(
            1u, virtual_device_factory_->mutable_state()->registrations.size());
        const absl::optional<device::CredProtect> result =
            virtual_device_factory_->mutable_state()
                ->registrations.begin()
                ->second.protection;
        EXPECT_EQ(*result, config.force_cred_protect);
      } else {
        EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR, status);
      }
    }
  }
}

TEST_F(ResidentKeyAuthenticatorImplTest, AuthenticatorDefaultCredProtect) {
  // Some authenticators may have a default credProtect level that isn't
  // kUVOptional. This has complex interactions that are tested here.
  constexpr struct {
    blink::mojom::ProtectionPolicy requested_level;
    device::CredProtect authenticator_default;
    device::CredProtect result;
  } kExpectations[] = {
      // Standard case: normal authenticator and nothing specified. Chrome sets
      // a default of kUVOrCredIDRequired for discoverable credentials.
      {
          blink::mojom::ProtectionPolicy::UNSPECIFIED,
          device::CredProtect::kUVOptional,
          device::CredProtect::kUVOrCredIDRequired,
      },
      // Chrome's default of |kUVOrCredIDRequired| should not prevent a site
      // from requesting |kUVRequired| from a normal authenticator.
      {
          blink::mojom::ProtectionPolicy::UV_REQUIRED,
          device::CredProtect::kUVOptional,
          device::CredProtect::kUVRequired,
      },
      // Authenticator has a non-standard default, which should work fine.
      {
          blink::mojom::ProtectionPolicy::UNSPECIFIED,
          device::CredProtect::kUVOrCredIDRequired,
          device::CredProtect::kUVOrCredIDRequired,
      },
      // Authenticators can have a default of kUVRequired, but Chrome has a
      // default of kUVOrCredIDRequired for discoverable credentials. We should
      // not get a lesser protection level because of that.
      {
          blink::mojom::ProtectionPolicy::UNSPECIFIED,
          device::CredProtect::kUVRequired,
          device::CredProtect::kUVRequired,
      },
      // Site should be able to explicitly set credProtect kUVOptional despite
      // an authenticator default.
      {
          blink::mojom::ProtectionPolicy::NONE,
          device::CredProtect::kUVOrCredIDRequired,
          device::CredProtect::kUVOptional,
      },
  };

  device::VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.resident_key_support = true;
  config.cred_protect_support = true;

  for (const auto& test : kExpectations) {
    config.default_cred_protect = test.authenticator_default;
    virtual_device_factory_->SetCtap2Config(config);
    virtual_device_factory_->mutable_state()->registrations.clear();

    SCOPED_TRACE(::testing::Message()
                 << "result=" << CredProtectDescription(test.result));
    SCOPED_TRACE(::testing::Message()
                 << "default="
                 << CredProtectDescription(test.authenticator_default));
    SCOPED_TRACE(::testing::Message()
                 << "request="
                 << ProtectionPolicyDescription(test.requested_level));

    PublicKeyCredentialCreationOptionsPtr options = make_credential_options();
    options->authenticator_selection->SetResidentKeyForTesting(
        device::ResidentKeyRequirement::kRequired);
    options->protection_policy = test.requested_level;
    options->authenticator_selection->SetUserVerificationRequirementForTesting(
        device::UserVerificationRequirement::kRequired);

    EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
              AuthenticatorStatus::SUCCESS);
    ASSERT_EQ(1u,
              virtual_device_factory_->mutable_state()->registrations.size());
    const device::CredProtect result = virtual_device_factory_->mutable_state()
                                           ->registrations.begin()
                                           ->second.protection;

    EXPECT_EQ(result, test.result) << CredProtectDescription(result);
  }
}

TEST_F(ResidentKeyAuthenticatorImplTest, ProtectedNonResidentCreds) {
  // Until we have UVToken, there's a danger that we'll preflight UV-required
  // credential IDs such that the authenticator denies knowledge of all of them
  // for silent requests and then we fail the whole request.
  device::VirtualCtap2Device::Config config;
  config.pin_support = true;
  config.resident_key_support = true;
  config.cred_protect_support = true;
  virtual_device_factory_->SetCtap2Config(config);
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      /*credential_id=*/{{4, 3, 2, 1}}, kTestRelyingPartyId));
  ASSERT_EQ(1u, virtual_device_factory_->mutable_state()->registrations.size());
  virtual_device_factory_->mutable_state()
      ->registrations.begin()
      ->second.protection = device::CredProtect::kUVRequired;

  // |SelectAccount| should not be called when there's only a single response.
  test_client_.expected_accounts = "<invalid>";

  PublicKeyCredentialRequestOptionsPtr options = get_credential_options();
  options->allow_credentials = GetTestCredentials(5);
  options->allow_credentials[0].GetIdForTesting() = {4, 3, 2, 1};

  GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));

  EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
  EXPECT_TRUE(HasUV(result.response));
}

TEST_F(ResidentKeyAuthenticatorImplTest, WithAppIDExtension) {
  // Setting an AppID value for a resident-key request should be ignored.
  device::VirtualCtap2Device::Config config;
  config.u2f_support = true;
  config.pin_support = true;
  config.resident_key_support = true;
  config.cred_protect_support = true;
  virtual_device_factory_->SetCtap2Config(config);
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      /*credential_id=*/{{4, 3, 2, 1}}, kTestRelyingPartyId,
      /*user_id=*/{{1, 2, 3, 4}}, absl::nullopt, absl::nullopt));

  // |SelectAccount| should not be called when there's only a single response
  // without identifying information.
  test_client_.expected_accounts = "<invalid>";

  PublicKeyCredentialRequestOptionsPtr options = get_credential_options();
  options->appid = kTestOrigin1;

  GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));

  EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
  EXPECT_TRUE(HasUV(result.response));
}

#if defined(OS_WIN)
// Requests with a credProtect extension that have |enforce_protection_policy|
// set should be rejected if the Windows WebAuthn API doesn't support
// credProtect.
TEST_F(ResidentKeyAuthenticatorImplTest, WinCredProtectApiVersion) {
  // The canned response returned by the Windows API fake is for acme.com.
  fake_win_webauthn_api_.set_available(true);
  NavigateAndCommit(GURL("https://acme.com"));
  for (const bool supports_cred_protect : {false, true}) {
    SCOPED_TRACE(testing::Message()
                 << "supports_cred_protect: " << supports_cred_protect);

    fake_win_webauthn_api_.set_version(supports_cred_protect
                                           ? WEBAUTHN_API_VERSION_2
                                           : WEBAUTHN_API_VERSION_1);

    PublicKeyCredentialCreationOptionsPtr options = make_credential_options();
    options->relying_party = device::PublicKeyCredentialRpEntity();
    options->relying_party.id = device::test_data::kRelyingPartyId;
    options->relying_party.name = "";
    options->authenticator_selection->SetUserVerificationRequirementForTesting(
        device::UserVerificationRequirement::kRequired);
    options->authenticator_selection->SetResidentKeyForTesting(
        device::ResidentKeyRequirement::kRequired);
    options->protection_policy =
        blink::mojom::ProtectionPolicy::UV_OR_CRED_ID_REQUIRED;
    options->enforce_protection_policy = true;

    EXPECT_EQ(AuthenticatorMakeCredential(std::move(options)).status,
              supports_cred_protect ? AuthenticatorStatus::SUCCESS
                                    : AuthenticatorStatus::NOT_ALLOWED_ERROR);
  }
}
#endif  // defined(OS_WIN)

TEST_F(ResidentKeyAuthenticatorImplTest, PRFExtension) {
  NavigateAndCommit(GURL(kTestOrigin1));

  absl::optional<device::PublicKeyCredentialDescriptor> credential;
  for (bool hmac_secret_supported : {false, true}) {
    // Setting the PRF extension on an authenticator that doesn't support it
    // should cause the extension to be echoed, but with enabled=false.
    // Otherwise, enabled should be true.
    device::VirtualCtap2Device::Config config;
    config.hmac_secret_support = hmac_secret_supported;
    config.max_credential_count_in_list = 3;
    config.max_credential_id_length = 256;
    config.pin_support = true;
    config.resident_key_support = true;
    virtual_device_factory_->SetCtap2Config(config);

    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->prf_enable = true;
    options->authenticator_selection->SetResidentKeyForTesting(
        hmac_secret_supported ? device::ResidentKeyRequirement::kRequired
                              : device::ResidentKeyRequirement::kDiscouraged);
    options->user.id = {1, 2, 3, 4};
    options->user.name = "name";
    options->user.display_name = "displayName";
    MakeCredentialResult result =
        AuthenticatorMakeCredential(std::move(options));
    EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);

    ASSERT_TRUE(result.response->echo_prf);
    ASSERT_EQ(result.response->prf, hmac_secret_supported);

    if (hmac_secret_supported) {
      device::AuthenticatorData auth_data =
          AuthDataFromMakeCredentialResponse(result.response);
      credential.emplace(device::CredentialType::kPublicKey,
                         auth_data.GetCredentialId());
    }
  }

  auto assertion = [&](std::vector<blink::mojom::PRFValuesPtr> inputs,
                       unsigned allow_list_size =
                           1) -> blink::mojom::PRFValuesPtr {
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->prf = true;
    options->prf_inputs = std::move(inputs);
    options->allow_credentials.clear();
    if (allow_list_size >= 1) {
      for (unsigned i = 0; i < allow_list_size - 1; i++) {
        std::vector<uint8_t> random_credential_id(32, static_cast<uint8_t>(i));
        options->allow_credentials.emplace_back(
            device::CredentialType::kPublicKey,
            std::move(random_credential_id));
      }
      options->allow_credentials.push_back(*credential);
    }

    GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));

    EXPECT_EQ(result.status, AuthenticatorStatus::SUCCESS);
    CHECK(result.response->prf_results);
    CHECK(!result.response->prf_results->id);
    return std::move(result.response->prf_results);
  };

  const std::vector<uint8_t> salt1(32, 1);
  const std::vector<uint8_t> salt2(32, 2);
  std::vector<uint8_t> salt1_eval;
  std::vector<uint8_t> salt2_eval;

  {
    auto prf_value = blink::mojom::PRFValues::New();
    prf_value->first = salt1;
    std::vector<blink::mojom::PRFValuesPtr> inputs;
    inputs.emplace_back(std::move(prf_value));
    auto result = assertion(std::move(inputs));
    salt1_eval = std::move(result->first);
  }

  // The result should be consistent
  {
    auto prf_value = blink::mojom::PRFValues::New();
    prf_value->first = salt1;
    std::vector<blink::mojom::PRFValuesPtr> inputs;
    inputs.emplace_back(std::move(prf_value));
    auto result = assertion(std::move(inputs));
    ASSERT_EQ(result->first, salt1_eval);
  }

  // Should be able to evaluate two points at once.
  {
    auto prf_value = blink::mojom::PRFValues::New();
    prf_value->first = salt1;
    prf_value->second = salt2;
    std::vector<blink::mojom::PRFValuesPtr> inputs;
    inputs.emplace_back(std::move(prf_value));
    auto result = assertion(std::move(inputs));
    ASSERT_EQ(result->first, salt1_eval);
    ASSERT_TRUE(result->second);
    salt2_eval = std::move(*result->second);
    ASSERT_NE(salt1_eval, salt2_eval);
  }

  // Should be consistent if swapped.
  {
    auto prf_value = blink::mojom::PRFValues::New();
    prf_value->first = salt2;
    prf_value->second = salt1;
    std::vector<blink::mojom::PRFValuesPtr> inputs;
    inputs.emplace_back(std::move(prf_value));
    auto result = assertion(std::move(inputs));
    ASSERT_EQ(result->first, salt2_eval);
    ASSERT_TRUE(result->second);
    ASSERT_EQ(*result->second, salt1_eval);
  }

  // Should still trigger if the credential ID is specified
  {
    auto prf_value = blink::mojom::PRFValues::New();
    prf_value->id.emplace(credential->id());
    prf_value->first = salt1;
    prf_value->second = salt2;
    std::vector<blink::mojom::PRFValuesPtr> inputs;
    inputs.emplace_back(std::move(prf_value));
    auto result = assertion(std::move(inputs));
    ASSERT_EQ(result->first, salt1_eval);
    ASSERT_TRUE(result->second);
    ASSERT_EQ(*result->second, salt2_eval);
  }

  // And the specified credential ID should override any default inputs.
  {
    auto prf_value1 = blink::mojom::PRFValues::New();
    prf_value1->first = std::vector<uint8_t>(32, 3);
    auto prf_value2 = blink::mojom::PRFValues::New();
    prf_value2->id.emplace(credential->id());
    prf_value2->first = salt1;
    prf_value2->second = salt2;
    std::vector<blink::mojom::PRFValuesPtr> inputs;
    inputs.emplace_back(std::move(prf_value1));
    inputs.emplace_back(std::move(prf_value2));
    auto result = assertion(std::move(inputs));
    ASSERT_EQ(result->first, salt1_eval);
    ASSERT_TRUE(result->second);
    ASSERT_EQ(*result->second, salt2_eval);
  }

  // ... and that should still be true if there there are lots of dummy entries
  // in the allowlist. Note that the virtual authenticator was configured such
  // that this will cause multiple batches.
  {
    auto prf_value = blink::mojom::PRFValues::New();
    prf_value->id.emplace(credential->id());
    prf_value->first = salt1;
    prf_value->second = salt2;
    std::vector<blink::mojom::PRFValuesPtr> inputs;
    inputs.emplace_back(std::move(prf_value));
    auto result = assertion(std::move(inputs), /*allowlist_size=*/20);
    ASSERT_EQ(result->first, salt1_eval);
    ASSERT_TRUE(result->second);
    ASSERT_EQ(*result->second, salt2_eval);
  }

  // Default PRF values should be passed down when the allowlist is empty.
  {
    auto prf_value = blink::mojom::PRFValues::New();
    prf_value->first = salt1;
    prf_value->second = salt2;
    test_client_.expected_accounts = "01020304:name:displayName";
    test_client_.selected_user_id = {1, 2, 3, 4};
    std::vector<blink::mojom::PRFValuesPtr> inputs;
    inputs.emplace_back(std::move(prf_value));
    auto result = assertion(std::move(inputs), /*allowlist_size=*/0);
    ASSERT_EQ(result->first, salt1_eval);
    ASSERT_TRUE(result->second);
    ASSERT_EQ(*result->second, salt2_eval);
  }

  // And the default PRF values should be used if none of the specific values
  // match.
  {
    auto prf_value1 = blink::mojom::PRFValues::New();
    prf_value1->first = salt1;
    auto prf_value2 = blink::mojom::PRFValues::New();
    prf_value2->first = std::vector<uint8_t>(32, 3);
    prf_value2->id = std::vector<uint8_t>(32, 4);
    std::vector<blink::mojom::PRFValuesPtr> inputs;
    inputs.emplace_back(std::move(prf_value1));
    inputs.emplace_back(std::move(prf_value2));
    auto result = assertion(std::move(inputs), /*allowlist_size=*/20);
    ASSERT_EQ(result->first, salt1_eval);
    ASSERT_FALSE(result->second);
  }
}

TEST_F(ResidentKeyAuthenticatorImplTest, ConditionalUI) {
  device::VirtualCtap2Device::Config config;
  config.resident_key_support = true;
  config.internal_uv_support = true;
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;

  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      /*credential_id=*/{{4, 3, 2, 1}}, kTestRelyingPartyId,
      /*user_id=*/{{1, 2, 3, 4}}, absl::nullopt, absl::nullopt));

  // |SelectAccount| should not be called when there's only a single response
  // without identifying information.
  test_client_.expected_accounts = "<invalid>";
  PublicKeyCredentialRequestOptionsPtr options(get_credential_options());
  options->is_conditional = true;
  GetAssertionResult result = AuthenticatorGetAssertion(std::move(options));
  EXPECT_EQ(AuthenticatorStatus::SUCCESS, result.status);
  EXPECT_TRUE(test_client_.is_conditional);
}

TEST_F(ResidentKeyAuthenticatorImplTest, NoDiscoverableCredentialsViaCable) {
  // caBLE devices never support discoverable credentials currently and we
  // shouldn't offer them for such requests.

  EnableFeature(features::kWebAuthCable);
  EnableFeature(device::kWebAuthCableSecondFactor);
  NavigateAndCommit(GURL(kTestOrigin1));

  device::VirtualCtap2Device::Config config;
  config.resident_key_support = true;
  config.internal_uv_support = true;
  virtual_device_factory_->SetCtap2Config(config);
  virtual_device_factory_->mutable_state()->fingerprints_enrolled = true;

  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectResidentKey(
      /*credential_id=*/{{4, 3, 2, 1}}, kTestRelyingPartyId,
      /*user_id=*/{{1, 2, 3, 4}}, absl::nullopt, absl::nullopt));

  for (const auto transport :
       {device::FidoTransportProtocol::kUsbHumanInterfaceDevice,
        device::FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy}) {
    virtual_device_factory_->SetTransport(transport);
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->allow_credentials.clear();

    if (transport == device::FidoTransportProtocol::kUsbHumanInterfaceDevice) {
      EXPECT_EQ(AuthenticatorGetAssertion(std::move(options)).status,
                AuthenticatorStatus::SUCCESS);
    } else {
      EXPECT_EQ(
          AuthenticatorGetAssertionAndWaitForTimeout(std::move(options)).status,
          AuthenticatorStatus::NOT_ALLOWED_ERROR);
    }
  }
}

class InternalAuthenticatorImplTest : public AuthenticatorTestBase {
 protected:
  InternalAuthenticatorImplTest() = default;

  void TearDown() override {
    // The |RenderFrameHost| must outlive |AuthenticatorImpl|.
    internal_authenticator_impl_.reset();
    AuthenticatorTestBase::TearDown();
  }

  void NavigateAndCommit(const GURL& url) {
    // The |RenderFrameHost| must outlive |AuthenticatorImpl|.
    internal_authenticator_impl_.reset();
    RenderViewHostTestHarness::NavigateAndCommit(url);
  }

  InternalAuthenticatorImpl* GetAuthenticator(
      const url::Origin& effective_origin_url) {
    internal_authenticator_impl_ =
        std::make_unique<InternalAuthenticatorImpl>(main_rfh());
    internal_authenticator_impl_->SetEffectiveOrigin(effective_origin_url);
    return internal_authenticator_impl_.get();
  }

 protected:
  std::unique_ptr<InternalAuthenticatorImpl> internal_authenticator_impl_;
};

// Verify behavior for various combinations of origins and RP IDs.
TEST_F(InternalAuthenticatorImplTest, MakeCredentialOriginAndRpIds) {
  // These instances should return security errors (for circumstances
  // that would normally crash the renderer).
  for (auto test_case : kInvalidRelyingPartyTestCases) {
    SCOPED_TRACE(std::string(test_case.claimed_authority) + " " +
                 std::string(test_case.origin));

    GURL origin = GURL(test_case.origin);
    if (url::Origin::Create(origin).opaque()) {
      // Opaque origins will cause DCHECK to fail.
      continue;
    }

    NavigateAndCommit(origin);
    InternalAuthenticatorImpl* authenticator =
        GetAuthenticator(url::Origin::Create(origin));
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->relying_party.id = test_case.claimed_authority;
    TestMakeCredentialCallback callback_receiver;
    authenticator->MakeCredential(std::move(options),
                                  callback_receiver.callback());
    callback_receiver.WaitForCallback();
    EXPECT_EQ(test_case.expected_status, callback_receiver.status());
  }

  // These instances should bypass security errors, by setting the effective
  // origin to a valid one.
  for (auto test_case : kValidRelyingPartyTestCases) {
    SCOPED_TRACE(std::string(test_case.claimed_authority) + " " +
                 std::string(test_case.origin));

    NavigateAndCommit(GURL("https://this.isthewrong.origin"));
    auto* authenticator =
        GetAuthenticator(url::Origin::Create(GURL(test_case.origin)));
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->relying_party.id = test_case.claimed_authority;

    ResetVirtualDevice();
    TestMakeCredentialCallback callback_receiver;
    authenticator->MakeCredential(std::move(options),
                                  callback_receiver.callback());
    callback_receiver.WaitForCallback();
    EXPECT_EQ(test_case.expected_status, callback_receiver.status());
  }
}

// Verify behavior for various combinations of origins and RP IDs.
TEST_F(InternalAuthenticatorImplTest, GetAssertionOriginAndRpIds) {
  // These instances should return security errors (for circumstances
  // that would normally crash the renderer).
  for (const OriginClaimedAuthorityPair& test_case :
       kInvalidRelyingPartyTestCases) {
    SCOPED_TRACE(std::string(test_case.claimed_authority) + " " +
                 std::string(test_case.origin));

    GURL origin = GURL(test_case.origin);
    if (url::Origin::Create(origin).opaque()) {
      // Opaque origins will cause DCHECK to fail.
      continue;
    }

    NavigateAndCommit(origin);
    InternalAuthenticatorImpl* authenticator =
        GetAuthenticator(url::Origin::Create(origin));
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->relying_party_id = test_case.claimed_authority;

    TestGetAssertionCallback callback_receiver;
    authenticator->GetAssertion(std::move(options),
                                callback_receiver.callback());
    callback_receiver.WaitForCallback();
    EXPECT_EQ(test_case.expected_status, callback_receiver.status());
  }

  // These instances should bypass security errors, by setting the effective
  // origin to a valid one.
  for (const OriginClaimedAuthorityPair& test_case :
       kValidRelyingPartyTestCases) {
    SCOPED_TRACE(std::string(test_case.claimed_authority) + " " +
                 std::string(test_case.origin));

    NavigateAndCommit(GURL("https://this.isthewrong.origin"));
    InternalAuthenticatorImpl* authenticator =
        GetAuthenticator(url::Origin::Create(GURL(test_case.origin)));
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->relying_party_id = test_case.claimed_authority;

    ResetVirtualDevice();
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        options->allow_credentials[0].id(), test_case.claimed_authority));
    TestGetAssertionCallback callback_receiver;
    authenticator->GetAssertion(std::move(options),
                                callback_receiver.callback());
    callback_receiver.WaitForCallback();
    EXPECT_EQ(test_case.expected_status, callback_receiver.status());
  }
}

#if defined(OS_MAC)
class TouchIdAuthenticatorImplTest : public AuthenticatorImplTest {
 public:
  void SetUp() override {
    AuthenticatorImplTest::SetUp();
    test_client_.GetTestWebAuthenticationDelegate()
        ->touch_id_authenticator_config.emplace();
    old_client_ = SetBrowserClientForTesting(&test_client_);
  }

  void TearDown() override {
    SetBrowserClientForTesting(old_client_);
    AuthenticatorImplTest::TearDown();
  }

 private:
  TestAuthenticatorContentBrowserClient test_client_;
  ContentBrowserClient* old_client_ = nullptr;
};

TEST_F(TouchIdAuthenticatorImplTest, IsUVPAA) {
  NavigateAndCommit(GURL(kTestOrigin1));
  mojo::Remote<blink::mojom::Authenticator> authenticator =
      ConnectToAuthenticator();

  if (__builtin_available(macOS 10.12.2, *)) {
    for (const bool touch_id_available : {false, true}) {
      SCOPED_TRACE(::testing::Message()
                   << "touch_id_available=" << touch_id_available);
      device::fido::mac::ScopedTouchIdTestEnvironment touch_id_test_environment;
      touch_id_test_environment.SetTouchIdAvailable(touch_id_available);
      TestIsUvpaaCallback cb;
      authenticator->IsUserVerifyingPlatformAuthenticatorAvailable(
          cb.callback());
      cb.WaitForCallback();
      EXPECT_EQ(touch_id_available, cb.value());
    }
  }
}
#endif  // defined(OS_MAC)

class CableV2AuthenticatorImplTest : public AuthenticatorImplTest {
 public:
  CableV2AuthenticatorImplTest()
      : network_context_(device::cablev2::NewMockTunnelServer(
            base::BindRepeating(&CableV2AuthenticatorImplTest::OnContact,
                                base::Unretained(this)))),
        browser_client_(base::BindRepeating(
            &CableV2AuthenticatorImplTest::MaybeContactPhones,
            base::Unretained(this))) {}

  void SetUp() override {
    AuthenticatorImplTest::SetUp();

    EnableFeature(features::kWebAuthCable);
    EnableFeature(device::kWebAuthPhoneSupport);
    NavigateAndCommit(GURL(kTestOrigin1));

    old_client_ = SetBrowserClientForTesting(&browser_client_);

    bssl::UniquePtr<EC_GROUP> p256(
        EC_GROUP_new_by_curve_name(NID_X9_62_prime256v1));
    bssl::UniquePtr<EC_KEY> peer_identity(EC_KEY_derive_from_secret(
        p256.get(), zero_seed_.data(), zero_seed_.size()));
    CHECK_EQ(sizeof(peer_identity_x962_),
             EC_POINT_point2oct(
                 p256.get(), EC_KEY_get0_public_key(peer_identity.get()),
                 POINT_CONVERSION_UNCOMPRESSED, peer_identity_x962_,
                 sizeof(peer_identity_x962_), /*ctx=*/nullptr));

    std::tie(ble_advert_callback_, ble_advert_events_) =
        device::cablev2::Discovery::AdvertEventStream::New();
  }

  void TearDown() override {
    SetBrowserClientForTesting(old_client_);
    AuthenticatorImplTest::TearDown();
  }

  base::RepeatingCallback<void(device::cablev2::PairingEvent)>
  GetPairingCallback() {
    return base::BindRepeating(&CableV2AuthenticatorImplTest::OnPairingEvent,
                               base::Unretained(this));
  }

 protected:
  class DiscoveryFactory : public device::FidoDiscoveryFactory {
   public:
    explicit DiscoveryFactory(
        std::unique_ptr<device::cablev2::Discovery> discovery)
        : discovery_(std::move(discovery)) {}

    std::vector<std::unique_ptr<device::FidoDiscoveryBase>> Create(
        device::FidoTransportProtocol transport) override {
      if (transport !=
              device::FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy ||
          !discovery_) {
        return {};
      }

      return SingleDiscovery(std::move(discovery_));
    }

   private:
    std::unique_ptr<device::cablev2::Discovery> discovery_;
  };

  class ContactWhenReadyAuthenticatorRequestDelegate
      : public AuthenticatorRequestClientDelegate {
   public:
    explicit ContactWhenReadyAuthenticatorRequestDelegate(
        base::RepeatingClosure callback)
        : callback_(callback) {}
    ~ContactWhenReadyAuthenticatorRequestDelegate() override = default;

    void OnTransportAvailabilityEnumerated(
        device::FidoRequestHandlerBase::TransportAvailabilityInfo) override {
      callback_.Run();
    }

   private:
    base::RepeatingClosure callback_;
  };

  class ContactWhenReadyContentBrowserClient : public ContentBrowserClient {
   public:
    explicit ContactWhenReadyContentBrowserClient(
        base::RepeatingClosure callback)
        : callback_(callback) {}

    std::unique_ptr<AuthenticatorRequestClientDelegate>
    GetWebAuthenticationRequestDelegate(
        RenderFrameHost* render_frame_host) override {
      return std::make_unique<ContactWhenReadyAuthenticatorRequestDelegate>(
          callback_);
    }

   private:
    base::RepeatingClosure callback_;
  };

  // MaybeContactPhones is called when OnTransportAvailabilityEnumerated is
  // called by the request handler.
  void MaybeContactPhones() {
    if (maybe_contact_phones_callback_) {
      std::move(maybe_contact_phones_callback_).Run();
    }
  }

  void OnContact(
      base::span<const uint8_t, device::cablev2::kTunnelIdSize> tunnel_id,
      base::span<const uint8_t, device::cablev2::kPairingIDSize> pairing_id,
      base::span<const uint8_t, device::cablev2::kClientNonceSize> client_nonce,
      const std::string& request_type_hint) {
    std::move(contact_callback_)
        .Run(tunnel_id, pairing_id, client_nonce, request_type_hint);
  }

  void OnPairingEvent(device::cablev2::PairingEvent event) {
    if (auto* disabled_public_key_index = absl::get_if<size_t>(&event)) {
      // When testing failed contacts, only a single pairing is supported
      // otherwise a more complex way of handling the indexes will be needed.
      CHECK(*disabled_public_key_index == 0 && pairings_.size() == 1);
      pairings_.clear();
    } else if (auto* pairing =
                   absl::get_if<std::unique_ptr<device::cablev2::Pairing>>(
                       &event)) {
      pairings_.emplace_back(std::move(*pairing));
    } else {
      CHECK(false);
    }
  }

  const std::array<uint8_t, device::cablev2::kRootSecretSize> root_secret_ = {
      0};
  const std::array<uint8_t, device::cablev2::kQRKeySize> qr_generator_key_ = {
      0};
  const std::array<uint8_t, device::cablev2::kQRSecretSize> zero_qr_secret_ = {
      0};
  const std::array<uint8_t, device::cablev2::kQRSeedSize> zero_seed_ = {0};

  std::unique_ptr<network::mojom::NetworkContext> network_context_;
  uint8_t peer_identity_x962_[device::kP256X962Length] = {0};
  device::VirtualCtap2Device virtual_device_;
  std::vector<std::unique_ptr<device::cablev2::Pairing>> pairings_;
  base::OnceCallback<void(
      base::span<const uint8_t, device::cablev2::kTunnelIdSize> tunnel_id,
      base::span<const uint8_t, device::cablev2::kPairingIDSize> pairing_id,
      base::span<const uint8_t, device::cablev2::kClientNonceSize> client_nonce,
      const std::string& request_type_hint)>
      contact_callback_;

  std::unique_ptr<device::cablev2::Discovery::AdvertEventStream>
      ble_advert_events_;
  device::cablev2::Discovery::AdvertEventStream::Callback ble_advert_callback_;

  ContactWhenReadyContentBrowserClient browser_client_;
  ContentBrowserClient* old_client_ = nullptr;
  base::OnceClosure maybe_contact_phones_callback_;
};

TEST_F(CableV2AuthenticatorImplTest, QRBasedWithNoPairing) {
  auto discovery = std::make_unique<device::cablev2::Discovery>(
      device::FidoRequestType::kGetAssertion, network_context_.get(),
      qr_generator_key_, std::move(ble_advert_events_),
      /*pairings=*/std::vector<std::unique_ptr<device::cablev2::Pairing>>(),
      /*contact_device_stream=*/nullptr,
      /*extension_contents=*/std::vector<device::CableDiscoveryData>(),
      GetPairingCallback());

  AuthenticatorEnvironmentImpl::GetInstance()
      ->ReplaceDefaultDiscoveryFactoryForTesting(
          std::make_unique<DiscoveryFactory>(std::move(discovery)));

  std::unique_ptr<device::cablev2::authenticator::Transaction> transaction =
      device::cablev2::authenticator::TransactFromQRCode(
          device::cablev2::authenticator::NewMockPlatform(
              std::move(ble_advert_callback_), &virtual_device_),
          network_context_.get(), root_secret_, "Test Authenticator",
          zero_qr_secret_, peer_identity_x962_,
          /*contact_id=*/absl::nullopt);

  EXPECT_EQ(AuthenticatorMakeCredential().status, AuthenticatorStatus::SUCCESS);
  EXPECT_EQ(pairings_.size(), 0u);
}

TEST_F(CableV2AuthenticatorImplTest, PairingBased) {
  // First do unpaired exchange to get pairing data.
  auto discovery = std::make_unique<device::cablev2::Discovery>(
      device::FidoRequestType::kGetAssertion, network_context_.get(),
      qr_generator_key_, std::move(ble_advert_events_),
      /*pairings=*/std::vector<std::unique_ptr<device::cablev2::Pairing>>(),
      /*contact_device_stream=*/nullptr,
      /*extension_contents=*/std::vector<device::CableDiscoveryData>(),
      GetPairingCallback());

  AuthenticatorEnvironmentImpl::GetInstance()
      ->ReplaceDefaultDiscoveryFactoryForTesting(
          std::make_unique<DiscoveryFactory>(std::move(discovery)));

  const std::vector<uint8_t> contact_id(/*count=*/200, /*value=*/1);
  std::unique_ptr<device::cablev2::authenticator::Transaction> transaction =
      device::cablev2::authenticator::TransactFromQRCode(
          device::cablev2::authenticator::NewMockPlatform(
              std::move(ble_advert_callback_), &virtual_device_),
          network_context_.get(), root_secret_, "Test Authenticator",
          zero_qr_secret_, peer_identity_x962_, contact_id);

  EXPECT_EQ(AuthenticatorMakeCredential().status, AuthenticatorStatus::SUCCESS);
  EXPECT_EQ(pairings_.size(), 1u);

  // Now do a pairing-based exchange. Generate a random request type hint to
  // ensure that all values work.
  device::FidoRequestType request_type =
      device::FidoRequestType::kMakeCredential;
  std::string expected_request_type_string = "mc";
  if (base::RandDouble() < 0.5) {
    request_type = device::FidoRequestType::kGetAssertion;
    expected_request_type_string = "ga";
  }

  std::tie(ble_advert_callback_, ble_advert_events_) =
      device::cablev2::Discovery::EventStream<
          base::span<const uint8_t, device::cablev2::kAdvertSize>>::New();
  auto callback_and_event_stream =
      device::cablev2::Discovery::EventStream<size_t>::New();
  discovery = std::make_unique<device::cablev2::Discovery>(
      request_type, network_context_.get(), qr_generator_key_,
      std::move(ble_advert_events_), std::move(pairings_),
      std::move(callback_and_event_stream.second),
      /*extension_contents=*/std::vector<device::CableDiscoveryData>(),
      GetPairingCallback());

  maybe_contact_phones_callback_ =
      base::BindLambdaForTesting([&callback_and_event_stream]() {
        callback_and_event_stream.first.Run(0);
      });

  const std::array<uint8_t, device::cablev2::kRoutingIdSize> routing_id = {0};
  bool contact_callback_was_called = false;
  // When the |cablev2::Discovery| starts it'll make a connection to the tunnel
  // service with the contact ID from the pairing data. This will be handled by
  // the |TestNetworkContext| and turned into a call to |contact_callback_|.
  // This simulates the tunnel server sending a cloud message to a phone. Given
  // the information from the connection, a transaction can be created.
  contact_callback_ = base::BindLambdaForTesting(
      [this, &transaction, routing_id, contact_id, &contact_callback_was_called,
       &expected_request_type_string](
          base::span<const uint8_t, device::cablev2::kTunnelIdSize> tunnel_id,
          base::span<const uint8_t, device::cablev2::kPairingIDSize> pairing_id,
          base::span<const uint8_t, device::cablev2::kClientNonceSize>
              client_nonce,
          const std::string& request_type_hint) -> void {
        contact_callback_was_called = true;
        CHECK_EQ(request_type_hint, expected_request_type_string);
        transaction = device::cablev2::authenticator::TransactFromFCM(
            device::cablev2::authenticator::NewMockPlatform(
                std::move(ble_advert_callback_), &virtual_device_),
            network_context_.get(), root_secret_, routing_id, tunnel_id,
            pairing_id, client_nonce, contact_id);
      });

  AuthenticatorEnvironmentImpl::GetInstance()
      ->ReplaceDefaultDiscoveryFactoryForTesting(
          std::make_unique<DiscoveryFactory>(std::move(discovery)));

  EXPECT_EQ(AuthenticatorMakeCredential().status, AuthenticatorStatus::SUCCESS);
  EXPECT_TRUE(contact_callback_was_called);
}

static std::unique_ptr<device::cablev2::Pairing> DummyPairing() {
  auto ret = std::make_unique<device::cablev2::Pairing>();
  ret->tunnel_server_domain = "example.com";
  ret->contact_id = {1, 2, 3, 4, 5};
  ret->id = {6, 7, 8, 9};
  ret->secret = {10, 11, 12, 13};
  std::fill(ret->peer_public_key_x962.begin(), ret->peer_public_key_x962.end(),
            22);
  ret->name = __func__;

  return ret;
}

TEST_F(CableV2AuthenticatorImplTest, ContactIDDisabled) {
  std::vector<std::unique_ptr<device::cablev2::Pairing>> pairings;
  pairings.emplace_back(DummyPairing());
  // Passing |nullopt| as the callback here causes all contact IDs to be
  // rejected.
  auto network_context = device::cablev2::NewMockTunnelServer(absl::nullopt);
  auto callback_and_event_stream =
      device::cablev2::Discovery::EventStream<size_t>::New();
  auto discovery = std::make_unique<device::cablev2::Discovery>(
      device::FidoRequestType::kGetAssertion, network_context.get(),
      qr_generator_key_, std::move(ble_advert_events_), std::move(pairings),
      std::move(callback_and_event_stream.second),
      /*extension_contents=*/std::vector<device::CableDiscoveryData>(),
      GetPairingCallback());

  AuthenticatorEnvironmentImpl::GetInstance()
      ->ReplaceDefaultDiscoveryFactoryForTesting(
          std::make_unique<DiscoveryFactory>(std::move(discovery)));

  maybe_contact_phones_callback_ =
      base::BindLambdaForTesting([&callback_and_event_stream]() {
        callback_and_event_stream.first.Run(0);
      });

  pairings_.emplace_back(DummyPairing());
  ASSERT_EQ(pairings_.size(), 1u);
  EXPECT_EQ(AuthenticatorMakeCredentialAndWaitForTimeout(
                GetTestPublicKeyCredentialCreationOptions())
                .status,
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
  // The pairing should be been erased because of the signal from the tunnel
  // server.
  ASSERT_EQ(pairings_.size(), 0u);
}

}  // namespace content
