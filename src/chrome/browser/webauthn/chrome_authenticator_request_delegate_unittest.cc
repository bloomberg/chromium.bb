// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/webauthn_pref_names.h"
#include "chrome/browser/webauthn/webauthn_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/web_contents_tester.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/features.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_device_authenticator.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/test_callback_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include "device/fido/win/authenticator.h"
#include "device/fido/win/fake_webauthn_api.h"
#include "third_party/microsoft_webauthn/webauthn.h"
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
#include "device/fido/mac/authenticator_config.h"
#endif  // BUILDFLAG(IS_MAC)

namespace {

class ChromeAuthenticatorRequestDelegateTest
    : public ChromeRenderViewHostTestHarness {};

class TestAuthenticatorModelObserver final
    : public AuthenticatorRequestDialogModel::Observer {
 public:
  explicit TestAuthenticatorModelObserver(
      AuthenticatorRequestDialogModel* model)
      : model_(model) {
    last_step_ = model_->current_step();
  }

  AuthenticatorRequestDialogModel::Step last_step() { return last_step_; }

  // AuthenticatorRequestDialogModel::Observer:
  void OnStepTransition() override { last_step_ = model_->current_step(); }

  void OnModelDestroyed(AuthenticatorRequestDialogModel* model) override {
    model_ = nullptr;
  }

 private:
  raw_ptr<AuthenticatorRequestDialogModel> model_;
  AuthenticatorRequestDialogModel::Step last_step_;
};

TEST_F(ChromeAuthenticatorRequestDelegateTest, IndividualAttestation) {
  static const struct TestCase {
    std::string name;
    std::string origin;
    std::string rp_id;
    std::string enterprise_attestation_switch_value;
    std::vector<std::string> permit_attestation_policy_values;
    bool expected;
  } kTestCases[] = {
      {"Basic", "https://login.example.com", "example.com", "", {}, false},
      {"Policy permits RP ID",
       "https://login.example.com",
       "example.com",
       "",
       {"example.com", "other.com"},
       true},
      {"Policy doesn't permit RP ID",
       "https://login.example.com",
       "example.com",
       "",
       {"other.com", "login.example.com", "https://example.com",
        "http://example.com", "https://login.example.com", "com", "*"},
       false},
      {"Policy doesn't care about the origin",
       "https://login.example.com",
       "example.com",
       "",
       {"https://login.example.com", "https://example.com"},
       false},
      {"Switch permits origin",
       "https://login.example.com",
       "example.com",
       "https://login.example.com,https://other.com,xyz:/invalidorigin",
       {},
       true},
      {"Switch doesn't permit origin",
       "https://login.example.com",
       "example.com",
       "example.com,login.example.com,http://login.example.com,https://"
       "example.com,https://a.login.example.com,https://*.example.com",
       {},
       false},
  };
  for (const auto& test : kTestCases) {
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        webauthn::switches::kPermitEnterpriseAttestationOriginList,
        test.enterprise_attestation_switch_value);
    PrefService* prefs =
        Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();
    if (!test.permit_attestation_policy_values.empty()) {
      std::vector<base::Value> policy_values;
      for (const std::string& v : test.permit_attestation_policy_values)
        policy_values.emplace_back(v);
      prefs->Set(prefs::kSecurityKeyPermitAttestation,
                 base::Value(std::move(policy_values)));
    } else {
      prefs->ClearPref(prefs::kSecurityKeyPermitAttestation);
    }
    ChromeWebAuthenticationDelegate delegate;
    EXPECT_EQ(delegate.ShouldPermitIndividualAttestation(
                  GetBrowserContext(), url::Origin::Create(GURL(test.origin)),
                  test.rp_id),
              test.expected)
        << test.name;
  }
}

TEST_F(ChromeAuthenticatorRequestDelegateTest, CableConfiguration) {
  class DiscoveryFactory : public device::FidoDiscoveryFactory {
   public:
    void set_cable_data(
        device::FidoRequestType request_type,
        std::vector<device::CableDiscoveryData> cable_data,
        const absl::optional<std::array<uint8_t, device::cablev2::kQRKeySize>>&
            qr_generator_key,
        std::vector<std::unique_ptr<device::cablev2::Pairing>> v2_pairings)
        override {
      this->cable_data = std::move(cable_data);
      this->qr_key = qr_generator_key;
      this->v2_pairings = std::move(v2_pairings);
    }

    void set_android_accessory_params(
        mojo::Remote<device::mojom::UsbDeviceManager>,
        std::string aoa_request_description) override {
      this->aoa_configured = true;
    }

    std::vector<device::CableDiscoveryData> cable_data;
    absl::optional<std::array<uint8_t, device::cablev2::kQRKeySize>> qr_key;
    std::vector<std::unique_ptr<device::cablev2::Pairing>> v2_pairings;
    bool aoa_configured = false;
  };

  const std::array<uint8_t, 16> eid = {1, 2, 3, 4};
  const std::array<uint8_t, 32> prekey = {5, 6, 7, 8};
  const device::CableDiscoveryData v1_extension(
      device::CableDiscoveryData::Version::V1, eid, eid, prekey);

  device::CableDiscoveryData v2_extension;
  v2_extension.version = device::CableDiscoveryData::Version::V2;
  v2_extension.v2.emplace(prekey.begin(), prekey.end());

  enum class Result {
    kNone,
    kV1,
    kServerLink,
    k3rdParty,
  };

  // TODO(crbug.com/1052397): Revisit the macro expression once build flag
  // switch of lacros-chrome is complete. If updating this, also update
  // ChromeAuthenticatorRequestDelegate.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
  // On Linux, some configurations aren't supported because of bluez
  // limitations. This macro maps the expected result in that case.
#define NONE_ON_LINUX(r) (Result::kNone)
#else
#define NONE_ON_LINUX(r) (r)
#endif

  const struct {
    const char* origin;
    std::vector<device::CableDiscoveryData> extensions;
    Result expected_result;
  } kTests[] = {
      {
          "https://example.com",
          {},
          Result::k3rdParty,
      },
      {
          // Extensions should be ignored on a 3rd-party site.
          "https://example.com",
          {v1_extension},
          Result::k3rdParty,
      },
      {
          // Extensions should be ignored on a 3rd-party site.
          "https://example.com",
          {v2_extension},
          Result::k3rdParty,
      },
      {
          // a.g.c should not get caBLE without an extension
          "https://accounts.google.com",
          {},
          Result::kNone,
      },
      {
          "https://accounts.google.com",
          {v1_extension},
          NONE_ON_LINUX(Result::kV1),
      },
      {
          "https://accounts.google.com",
          {v2_extension},
          Result::kServerLink,
      },
  };

  unsigned test_case = 0;
  for (const auto& test : kTests) {
    SCOPED_TRACE(test_case);
    test_case++;

    DiscoveryFactory discovery_factory;
    ChromeAuthenticatorRequestDelegate delegate(main_rfh());
    delegate.SetRelyingPartyId(/*rp_id=*/"example.com");
    delegate.SetPassEmptyUsbDeviceManagerForTesting(true);
    delegate.ConfigureCable(url::Origin::Create(GURL(test.origin)),
                            device::FidoRequestType::kGetAssertion,
                            test.extensions, &discovery_factory);

    switch (test.expected_result) {
      case Result::kNone:
        EXPECT_FALSE(discovery_factory.qr_key.has_value());
        EXPECT_TRUE(discovery_factory.v2_pairings.empty());
        EXPECT_TRUE(discovery_factory.cable_data.empty());
        EXPECT_TRUE(discovery_factory.aoa_configured);
        break;

      case Result::kV1:
        EXPECT_FALSE(discovery_factory.qr_key.has_value());
        EXPECT_TRUE(discovery_factory.v2_pairings.empty());
        EXPECT_FALSE(discovery_factory.cable_data.empty());
        EXPECT_TRUE(discovery_factory.aoa_configured);
        EXPECT_EQ(delegate.dialog_model()->cable_ui_type(),
                  AuthenticatorRequestDialogModel::CableUIType::CABLE_V1);
        break;

      case Result::kServerLink:
        EXPECT_TRUE(discovery_factory.qr_key.has_value());
        EXPECT_TRUE(discovery_factory.v2_pairings.empty());
        EXPECT_FALSE(discovery_factory.cable_data.empty());
        EXPECT_TRUE(discovery_factory.aoa_configured);
        EXPECT_EQ(
            delegate.dialog_model()->cable_ui_type(),
            AuthenticatorRequestDialogModel::CableUIType::CABLE_V2_SERVER_LINK);
        break;

      case Result::k3rdParty:
        EXPECT_TRUE(discovery_factory.qr_key.has_value());
        EXPECT_TRUE(discovery_factory.v2_pairings.empty());
        EXPECT_TRUE(discovery_factory.cable_data.empty());
        EXPECT_TRUE(discovery_factory.aoa_configured);
        EXPECT_EQ(
            delegate.dialog_model()->cable_ui_type(),
            AuthenticatorRequestDialogModel::CableUIType::CABLE_V2_2ND_FACTOR);
        break;
    }
  }
}

TEST_F(ChromeAuthenticatorRequestDelegateTest, ConditionalUI) {
  // Enabling conditional mode should cause the modal dialog to stay hidden at
  // the beginning of a request. An omnibar icon might be shown instead.
  for (bool conditional_ui : {true, false}) {
    ChromeAuthenticatorRequestDelegate delegate(main_rfh());
    delegate.SetConditionalRequest(conditional_ui);
    delegate.SetRelyingPartyId(/*rp_id=*/"example.com");
    AuthenticatorRequestDialogModel* model = delegate.dialog_model();
    TestAuthenticatorModelObserver observer(model);
    model->AddObserver(&observer);
    EXPECT_EQ(observer.last_step(),
              AuthenticatorRequestDialogModel::Step::kNotStarted);
    delegate.OnTransportAvailabilityEnumerated(
        AuthenticatorRequestDialogModel::TransportAvailabilityInfo());
    EXPECT_EQ(observer.last_step() ==
                  AuthenticatorRequestDialogModel::Step::kLocationBarBubble,
              conditional_ui);
  }
}

TEST_F(ChromeAuthenticatorRequestDelegateTest,
       OverrideValidateDomainAndRelyingPartyIDTest) {
  constexpr char kTestExtensionOrigin[] = "chrome-extension://abcdef";
  static const struct {
    std::string rp_id;
    std::string origin;
    bool expected;
  } kTests[] = {
      {"example.com", "https://example.com", false},
      {"foo.com", "https://example.com", false},
      {"abcdef", kTestExtensionOrigin, true},
      {"abcdefg", kTestExtensionOrigin, false},
      {"example.com", kTestExtensionOrigin, false},
  };

  ChromeWebAuthenticationDelegate delegate;
  for (const auto& test : kTests) {
    EXPECT_EQ(delegate.OverrideCallerOriginAndRelyingPartyIdValidation(
                  GetBrowserContext(), url::Origin::Create(GURL(test.origin)),
                  test.rp_id),
              test.expected);
  }
}

TEST_F(ChromeAuthenticatorRequestDelegateTest, MaybeGetRelyingPartyIdOverride) {
  constexpr char kCryptotokenOrigin[] =
      "chrome-extension://kmendfapggjehodndflmmgagdbamhnfd";
  constexpr char kTestExtensionOrigin[] = "chrome-extension://abcdef";
  ChromeWebAuthenticationDelegate delegate;
  static const struct {
    std::string rp_id;
    std::string origin;
    absl::optional<std::string> expected;
  } kTests[] = {
      {"example.com", "https://example.com", absl::nullopt},
      {"foo.com", "https://example.com", absl::nullopt},
      {"foobar.com", kCryptotokenOrigin, absl::nullopt},
      {"abcdef", kTestExtensionOrigin, kTestExtensionOrigin},
      {"example.com", kTestExtensionOrigin, kTestExtensionOrigin},
  };
  for (const auto& test : kTests) {
    EXPECT_EQ(delegate.MaybeGetRelyingPartyIdOverride(
                  test.rp_id, url::Origin::Create(GURL(test.origin))),
              test.expected);
  }
}

#if BUILDFLAG(IS_MAC)
API_AVAILABLE(macos(10.12.2))
std::string TouchIdMetadataSecret(ChromeWebAuthenticationDelegate& delegate,
                                  content::BrowserContext* browser_context) {
  return delegate.GetTouchIdAuthenticatorConfig(browser_context)
      ->metadata_secret;
}

TEST_F(ChromeAuthenticatorRequestDelegateTest, TouchIdMetadataSecret) {
  if (__builtin_available(macOS 10.12.2, *)) {
    ChromeWebAuthenticationDelegate delegate;
    std::string secret = TouchIdMetadataSecret(delegate, GetBrowserContext());
    EXPECT_EQ(secret.size(), 32u);
    // The secret should be stable.
    EXPECT_EQ(secret, TouchIdMetadataSecret(delegate, GetBrowserContext()));
  }
}

TEST_F(ChromeAuthenticatorRequestDelegateTest,
       TouchIdMetadataSecret_EqualForSameProfile) {
  if (__builtin_available(macOS 10.12.2, *)) {
    // Different delegates on the same BrowserContext (Profile) should return
    // the same secret.
    ChromeWebAuthenticationDelegate delegate1;
    ChromeWebAuthenticationDelegate delegate2;
    EXPECT_EQ(TouchIdMetadataSecret(delegate1, GetBrowserContext()),
              TouchIdMetadataSecret(delegate2, GetBrowserContext()));
  }
}

TEST_F(ChromeAuthenticatorRequestDelegateTest,
       TouchIdMetadataSecret_NotEqualForDifferentProfiles) {
  if (__builtin_available(macOS 10.12.2, *)) {
    // Different profiles have different secrets.
    auto other_browser_context = CreateBrowserContext();
    ChromeWebAuthenticationDelegate delegate;
    EXPECT_NE(TouchIdMetadataSecret(delegate, GetBrowserContext()),
              TouchIdMetadataSecret(delegate, other_browser_context.get()));
    // Ensure this second secret is actually valid.
    EXPECT_EQ(
        32u,
        TouchIdMetadataSecret(delegate, other_browser_context.get()).size());
  }
}
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_WIN)

static constexpr char kRelyingPartyID[] = "example.com";

// Tests that ShouldReturnAttestation() returns with true if |authenticator|
// is the Windows native WebAuthn API with WEBAUTHN_API_VERSION_2 or higher,
// where Windows prompts for attestation in its own native UI.
//
// Ideally, this would also test the inverse case, i.e. that with
// WEBAUTHN_API_VERSION_1 Chrome's own attestation prompt is shown. However,
// there seems to be no good way to test AuthenticatorRequestDialogModel UI.
TEST_F(ChromeAuthenticatorRequestDelegateTest, ShouldPromptForAttestationWin) {
  ::device::FakeWinWebAuthnApi win_webauthn_api;
  win_webauthn_api.set_version(WEBAUTHN_API_VERSION_2);
  ::device::WinWebAuthnApiAuthenticator authenticator(
      /*current_window=*/nullptr, &win_webauthn_api);

  ::device::test::ValueCallbackReceiver<bool> cb;
  ChromeAuthenticatorRequestDelegate delegate(main_rfh());
  delegate.ShouldReturnAttestation(kRelyingPartyID, &authenticator,
                                   /*is_enterprise_attestation=*/false,
                                   cb.callback());
  cb.WaitForCallback();
  EXPECT_EQ(cb.value(), true);
}

class ChromeAuthenticatorRequestDelegateWindowsBehaviorTest
    : public ChromeAuthenticatorRequestDelegateTest {
 public:
  void CreateObjectsUnderTest() {
    delegate_.emplace(main_rfh());
    delegate_->SetRelyingPartyId("example.com");

    AuthenticatorRequestDialogModel* const model = delegate_->dialog_model();
    observer_.emplace(model);
    model->AddObserver(&observer_.value());
    CHECK_EQ(observer_->last_step(),
             AuthenticatorRequestDialogModel::Step::kNotStarted);
  }

  absl::optional<ChromeAuthenticatorRequestDelegate> delegate_;
  absl::optional<TestAuthenticatorModelObserver> observer_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      device::kWebAuthPhoneSupport};
};

TEST_F(ChromeAuthenticatorRequestDelegateWindowsBehaviorTest,
       CancelAfterMechanismSelection) {
  // Test that, on Windows, the `ChromeAuthenticatorRequestDelegate` should
  // remember whether the last successful operation was with the native API or
  // not and immediately trigger that UI for the next operation accordingly.

  // Setup the Windows native authenticator and configure caBLE such that adding
  // a phone is an option.
  AuthenticatorRequestDialogModel::TransportAvailabilityInfo tai;
  tai.has_win_native_api_authenticator = true;
  tai.win_native_api_authenticator_id = "ID";
  tai.available_transports.insert(
      device::FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy);

  CreateObjectsUnderTest();
  delegate_->dialog_model()->set_cable_transport_info(
      absl::nullopt, {}, base::DoNothing(), "fido:/1234");
  delegate_->OnTransportAvailabilityEnumerated(tai);

  // Since there are two options, the mechanism selection sheet should be shown.
  EXPECT_EQ(observer_->last_step(),
            AuthenticatorRequestDialogModel::Step::kMechanismSelection);

  // Simulate the Windows native API being used successfully.
  ChromeWebAuthenticationDelegate non_request_delegate;
  non_request_delegate.OperationSucceeded(profile(), /* used_win_api= */ true);

  CreateObjectsUnderTest();
  delegate_->dialog_model()->set_cable_transport_info(
      absl::nullopt, {}, base::DoNothing(), "fido:/1234");
  delegate_->OnTransportAvailabilityEnumerated(tai);

  // Since the Windows API was used successfully last time, it should jump
  // directly to the native UI this time.
  EXPECT_EQ(observer_->last_step(),
            AuthenticatorRequestDialogModel::Step::kNotStarted);

  // Simulate that caBLE was used successfully.
  non_request_delegate.OperationSucceeded(profile(), /* used_win_api= */ false);

  CreateObjectsUnderTest();
  delegate_->dialog_model()->set_cable_transport_info(
      absl::nullopt, {}, base::DoNothing(), "fido:/1234");
  delegate_->OnTransportAvailabilityEnumerated(tai);

  // Should show the mechanism selection sheet again.
  EXPECT_EQ(observer_->last_step(),
            AuthenticatorRequestDialogModel::Step::kMechanismSelection);
}

#endif  // BUILDFLAG(IS_WIN)

class CorpCrdOverrideOriginAndRpIdValidationTest
    : public ChromeAuthenticatorRequestDelegateTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      device::kWebAuthnGoogleCorpRemoteDesktopClientPrivilege};
};

TEST_F(CorpCrdOverrideOriginAndRpIdValidationTest, Test) {
  constexpr char kCorpCrdOrigin[] = "https://remotedesktop.corp.google.com";
  constexpr char kExampleOrigin[] = "https://example.com";
  constexpr char kTestRpId[] = "random.test.site.com";
  enum class Policy {
    kUnset,
    kDisabled,
    kEnabled,
  };
  struct {
    Policy policy_value;
    std::string switch_value;
    std::string origin;
    bool expected;
  } kTestCases[] = {
      // With the policy disabled, the override should be off.
      {Policy::kUnset, "", kCorpCrdOrigin, false},
      {Policy::kUnset, kExampleOrigin, kExampleOrigin, false},
      {Policy::kDisabled, "", kCorpCrdOrigin, false},
      {Policy::kDisabled, kExampleOrigin, kExampleOrigin, false},

      // The origin must match the hard-coded value from the policy or the
      // switch value exactly.
      {Policy::kEnabled, "", kCorpCrdOrigin, true},
      {Policy::kEnabled, kExampleOrigin, kCorpCrdOrigin, true},
      {Policy::kEnabled, kExampleOrigin, kExampleOrigin, true},
      {Policy::kEnabled, "", kExampleOrigin, false},
      {Policy::kEnabled, kExampleOrigin, "http://remotedesktop.corp.google.com",
       false},
      {Policy::kEnabled, kExampleOrigin, "https://remotedesktop.google.com",
       false},
      {Policy::kEnabled, kExampleOrigin, "https://google.com", false},
      {Policy::kEnabled, kExampleOrigin, "https://a.google.com", false},
      {Policy::kEnabled, kExampleOrigin, "https://sub.example.com", false},
      {Policy::kEnabled, kExampleOrigin, "http://example.com", false},
      {Policy::kEnabled, kExampleOrigin, "example.com2", false},

      // The switch takes exactly one origin. No lists, or wildcards allowed.
      {Policy::kEnabled, "https://example.com,https://other.com",
       kExampleOrigin, false},
      {Policy::kEnabled, "", kExampleOrigin, false},
      {Policy::kEnabled, "", kExampleOrigin, false},
      {Policy::kEnabled, "https://*", kExampleOrigin, false},
      {Policy::kEnabled, "*.example.com", kExampleOrigin, false},
      {Policy::kEnabled, "https://*.example.com", kExampleOrigin, false},
  };
  ChromeWebAuthenticationDelegate delegate;
  for (const auto& test : kTestCases) {
    PrefService* prefs =
        Profile::FromBrowserContext(GetBrowserContext())->GetPrefs();
    base::test::ScopedCommandLine scoped_command_line;
    scoped_command_line.GetProcessCommandLine()->AppendSwitchASCII(
        webauthn::switches::kRemoteProxiedRequestsAllowedAdditionalOrigin,
        test.switch_value);
    switch (test.policy_value) {
      case Policy::kUnset:
        prefs->ClearPref(webauthn::pref_names::kRemoteProxiedRequestsAllowed);
        break;
      case Policy::kDisabled:
        prefs->SetBoolean(webauthn::pref_names::kRemoteProxiedRequestsAllowed,
                          false);
        break;
      case Policy::kEnabled:
        prefs->SetBoolean(webauthn::pref_names::kRemoteProxiedRequestsAllowed,
                          true);
        break;
    }

    EXPECT_EQ(delegate.OverrideCallerOriginAndRelyingPartyIdValidation(
                  GetBrowserContext(), url::Origin::Create(GURL(test.origin)),
                  kTestRpId),
              test.expected);
  }
}

}  // namespace
