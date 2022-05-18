// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sstream>
#include <vector>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/install_verifier.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/browser/authenticator_environment.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "device/fido/cable/cable_discovery_data.h"
#include "device/fido/cable/v2_test_util.h"
#include "device/fido/features.h"
#include "device/fido/virtual_fido_device.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "device/fido/virtual_u2f_device.h"
#include "extensions/common/extension_builder.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/network_context.mojom-forward.h"
#include "url/gurl.h"

namespace {

// This file tests WebAuthn features that depend on specific //chrome behaviour.
// Tests that don't depend on that should go into
// content/browser/webauth/webauth_browsertest.cc.

class WebAuthnBrowserTest : public InProcessBrowserTest {
 public:
  WebAuthnBrowserTest() = default;

  WebAuthnBrowserTest(const WebAuthnBrowserTest&) = delete;
  WebAuthnBrowserTest& operator=(const WebAuthnBrowserTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    CHECK(https_server_.Start());
    host_resolver()->AddRule("*", "127.0.0.1");
  }

 protected:
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

static constexpr char kGetAssertionCredID1234[] = R"((() => {
  let cred_id = new Uint8Array([1,2,3,4]);
  navigator.credentials.get({ publicKey: {
    challenge: cred_id,
    timeout: 10000,
    userVerification: 'discouraged',
    allowCredentials: [{type: 'public-key', id: cred_id}],
  }}).then(c => window.domAutomationController.send('webauthn: OK'),
           e => window.domAutomationController.send('error ' + e));
})())";

IN_PROC_BROWSER_TEST_F(WebAuthnBrowserTest, ChromeExtensions) {
  // Test that WebAuthn works inside of Chrome extensions. WebAuthn is based on
  // Relying Party IDs, which are domain names. But Chrome extensions don't have
  // domain names therefore the origin is used in their case.
  //
  // This test creates and installs an extension and then loads an HTML page
  // from inside that extension. A WebAuthn call is injected into that context
  // and it should get an assertion from a credential that's injected into the
  // virtual authenticator, scoped to the origin string.
  base::ScopedAllowBlockingForTesting allow_blocking;
  extensions::ScopedInstallVerifierBypassForTest install_verifier_bypass;

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  static constexpr char kPageFile[] = "page.html";

  std::vector<base::Value> resources;
  resources.emplace_back(std::string(kPageFile));
  static constexpr char kContents[] = R"(
<html>
  <head>
    <title>WebAuthn in extensions test</title>
  </head>
  <body>
  </body>
</html>
)";
  WriteFile(temp_dir.GetPath().AppendASCII(kPageFile), kContents,
            sizeof(kContents) - 1);

  extensions::ExtensionBuilder builder("test");
  builder.SetPath(temp_dir.GetPath())
      .SetVersion("1.0")
      .SetLocation(extensions::mojom::ManifestLocation::kExternalPolicyDownload)
      .SetManifestKey("web_accessible_resources", std::move(resources));

  extensions::ExtensionService* service =
      extensions::ExtensionSystem::Get(browser()->profile())
          ->extension_service();
  scoped_refptr<const extensions::Extension> extension = builder.Build();
  service->OnExtensionInstalled(extension.get(), syncer::StringOrdinal(), 0);

  auto virtual_device_factory =
      std::make_unique<device::test::VirtualFidoDeviceFactory>();
  const GURL url = extension->GetResourceURL(kPageFile);
  auto extension_id = url.host();
  static const uint8_t kCredentialID[] = {1, 2, 3, 4};
  virtual_device_factory->mutable_state()->InjectRegistration(
      kCredentialID, "chrome-extension://" + extension_id);

  content::AuthenticatorEnvironment::GetInstance()
      ->ReplaceDefaultDiscoveryFactoryForTesting(
          std::move(virtual_device_factory));

  EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  std::string result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      kGetAssertionCredID1234, &result));

  EXPECT_EQ("webauthn: OK", result);
}

// WebAuthnCableExtension exercises code paths where a server sends a caBLEv2
// extension in a get() request.
class WebAuthnCableExtension : public WebAuthnBrowserTest {
 public:
  WebAuthnCableExtension() {
    scoped_feature_list_.InitWithFeatures(
        {device::kWebAuthCableExtensionAnywhere}, {});
  }

 protected:
  static constexpr char kRequest[] = R"((() => {
    navigator.credentials.get({
      publicKey: {
        timeout: 1000,
        challenge: new Uint8Array([
            0x79, 0x50, 0x68, 0x71, 0xDA, 0xEE, 0xEE, 0xB9,
            0x94, 0xC3, 0xC2, 0x15, 0x67, 0x65, 0x26, 0x22,
            0xE3, 0xF3, 0xAB, 0x3B, 0x78, 0x2E, 0xD5, 0x6F,
            0x81, 0x26, 0xE2, 0xA6, 0x01, 0x7D, 0x74, 0x50
        ]).buffer,
        allowCredentials: [{
          type: 'public-key',
          id: new Uint8Array([1, 2, 3, 4]).buffer,
        }],
        userVerification: 'discouraged',

        extensions: {
          "cableAuthentication": [{
            version: 2,
            sessionPreKey: new Uint8Array([$1]).buffer,
            clientEid: new Uint8Array([$2]),
            authenticatorEid: new Uint8Array(),
          }],
        },
      },
    }).then(c => window.domAutomationController.send('webauthn: OK'),
            e => window.domAutomationController.send('error ' + e));
  })())";

  void MaybeInstall() {
    if (installed_) {
      return;
    }
    installed_ = true;

    EXPECT_TRUE(ui_test_utils::NavigateToURL(
        browser(), https_server_.GetURL("www.example.com", "/title1.html")));

    auto virtual_device_factory =
        std::make_unique<device::test::VirtualFidoDeviceFactory>();
    static const uint8_t kCredentialID[] = {1, 2, 3, 4};
    virtual_device_factory->mutable_state()->InjectRegistration(
        kCredentialID, "www.example.com");
    content::AuthenticatorEnvironment::GetInstance()
        ->ReplaceDefaultDiscoveryFactoryForTesting(
            std::move(virtual_device_factory));

    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(&observer_);
  }

  void DoRequest(std::string server_link_data, std::string experiment_data) {
    MaybeInstall();

    const std::string request = base::ReplaceStringPlaceholders(
        kRequest, {server_link_data, experiment_data}, nullptr);

    std::string result;
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        browser()->tab_strip_model()->GetActiveWebContents(), request,
        &result));

    EXPECT_EQ("webauthn: OK", result);
  }

  class ExtensionObserver
      : public ChromeAuthenticatorRequestDelegate::TestObserver {
   public:
    void Created(ChromeAuthenticatorRequestDelegate* delegate) override{};

    std::vector<std::unique_ptr<device::cablev2::Pairing>>
    GetCablePairingsFromSyncedDevices() override {
      return {};
    };

    void OnTransportAvailabilityEnumerated(
        ChromeAuthenticatorRequestDelegate* delegate,
        device::FidoRequestHandlerBase::TransportAvailabilityInfo* tai)
        override{};

    void UIShown(ChromeAuthenticatorRequestDelegate* delegate) override{};

    void CableV2ExtensionSeen(
        base::span<const uint8_t> server_link_data,
        base::span<const uint8_t> experiments,
        AuthenticatorRequestDialogModel::ExperimentServerLinkSheet exp_sheet,
        AuthenticatorRequestDialogModel::ExperimentServerLinkTitle exp_title)
        override {
      extensions_.emplace_back(
          base::HexEncode(server_link_data) + ":" +
          base::HexEncode(experiments) + ":" +
          base::NumberToString(static_cast<int>(exp_sheet)) + ":" +
          base::NumberToString(static_cast<int>(exp_title)));
    }

    std::vector<std::string> extensions_;
  };

  bool installed_ = false;
  ExtensionObserver observer_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(WebAuthnCableExtension, ServerLink) {
  DoRequest("1,2,3,4", "5,6,7,8");

  ASSERT_EQ(observer_.extensions_.size(), 1u);
  EXPECT_EQ(observer_.extensions_[0], "01020304:05060708:1:11");
}

IN_PROC_BROWSER_TEST_F(WebAuthnCableExtension, ServerLinkExperiments) {
  constexpr struct {
    const char* experiment_data;
    const char* expected;
  } kTests[] = {
      {"0", "01020304:00:1:11"},  // invalid; not a multiple of 4 bytes
      {"0,0,0,0", "01020304:00000000:1:11"},  // unknown value
      {"0,0,0,1", "01020304:00000001:1:11"},
      {"0,0,0,2", "01020304:00000002:2:11"},
      {"0,0,0,3", "01020304:00000003:3:11"},
      {"0,0,0,4", "01020304:00000004:4:11"},
      {"0,0,0,5", "01020304:00000005:5:11"},
      {"0,0,0,6", "01020304:00000006:6:11"},
      {"0,0,0,7", "01020304:00000007:1:11"},                  // unknown value
      {"0,0,0,1,0,0,0,2", "01020304:0000000100000002:1:11"},  // conflicting
      {"0,0,0,10", "01020304:0000000A:1:11"},                 // unknown value
      {"0,0,0,11", "01020304:0000000B:1:11"},
      {"0,0,0,12", "01020304:0000000C:1:12"},
      {"0,0,0,13", "01020304:0000000D:1:11"},  // unknown value
      {"0,0,0,3,0,0,0,12", "01020304:000000030000000C:3:12"},
  };

  unsigned test_no = 0;
  for (const auto& test : kTests) {
    observer_.extensions_.clear();

    SCOPED_TRACE(test_no++);
    SCOPED_TRACE(test.experiment_data);
    DoRequest("1,2,3,4", test.experiment_data);
    ASSERT_EQ(observer_.extensions_.size(), 1u);
    EXPECT_EQ(observer_.extensions_[0], test.expected);
  }
}

// WebAuthnCableSecondFactor primarily exercises
// ChromeAuthenticatorRequestDelegate and AuthenticatorRequestDialogModel. It
// mocks out the discovery process and thus allows the caBLE UI to be tested.
// It uses a trace-based approach: events are recorded (as strings) in an event
// trace which is then compared against the expected trace at the end.
class WebAuthnCableSecondFactor : public WebAuthnBrowserTest {
 public:
  WebAuthnCableSecondFactor() {
    scoped_feature_list_.InitWithFeatures({features::kWebAuthCable}, {});
    // This makes it a little easier to compare against.
    trace_ << std::endl;
  }

  std::ostringstream& trace() { return trace_; }

  AuthenticatorRequestDialogModel*& model() { return model_; }

 protected:
  // DiscoveryFactory vends a single discovery that doesn't discover anything
  // until requested to. The authenticator that is then discovered is a virtual
  // authenticator that serves simply to end the overall WebAuthn request.
  // Otherwise, DiscoveryFactory is responsible for tracing the caBLEv2 Pairing
  // objects and driving the simulation when the UI requests that a phone be
  // triggered.
  class DiscoveryFactory : public device::FidoDiscoveryFactory {
   public:
    explicit DiscoveryFactory(WebAuthnCableSecondFactor* test)
        : parent_(test) {}

    std::vector<std::unique_ptr<device::FidoDiscoveryBase>> Create(
        device::FidoTransportProtocol transport) override {
      if (transport !=
          device::FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy) {
        return {};
      }

      auto discovery = std::make_unique<PendingDiscovery>(
          device::FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy);
      add_authenticator_callback_ = discovery->GetAddAuthenticatorCallback();
      return SingleDiscovery(std::move(discovery));
    }

    void set_cable_data(
        device::FidoRequestType request_type,
        std::vector<device::CableDiscoveryData> cable_data,
        const absl::optional<std::array<uint8_t, device::cablev2::kQRKeySize>>&
            qr_generator_key,
        std::vector<std::unique_ptr<device::cablev2::Pairing>> v2_pairings)
        override {
      for (const auto& pairing : v2_pairings) {
        parent_->trace() << "PAIRING: " << pairing->name << " "
                         << base::HexEncode(base::span<const uint8_t>(
                                                pairing->peer_public_key_x962)
                                                .subspan(0, 4))
                         << " " << base::HexEncode(pairing->id) << std::endl;
      }
    }

    void set_cable_invalidated_pairing_callback(
        base::RepeatingCallback<void(size_t)> callback) override {
      invalid_pairing_callback_ = std::move(callback);
    }

    base::RepeatingCallback<void(size_t)> get_cable_contact_callback()
        override {
      return base::BindLambdaForTesting([this](size_t n) {
        parent_->trace() << "CONTACT: phone_instance=" << n
                         << " step=" << contact_step_number_ << std::endl;

        switch (contact_step_number_) {
          case 0:
            // Simiulate the first tunnel failing with a Gone status. This
            // should trigger a fallback to the second-priority phone with the
            // same name.
            base::SequencedTaskRunnerHandle::Get()->PostTask(
                FROM_HERE, base::BindLambdaForTesting([this, n]() {
                  invalid_pairing_callback_.Run(n);
                }));
            break;

          case 1:
            // Simulate the user clicking back and trying the phone again. This
            // should fallback to the lower-priority phone with the same name.
            base::SequencedTaskRunnerHandle::Get()->PostTask(
                FROM_HERE, base::BindLambdaForTesting([this]() {
                  parent_->model()->ContactPhoneForTesting("name2");
                }));
            break;

          case 2:
            // Try some other phones.
            base::SequencedTaskRunnerHandle::Get()->PostTask(
                FROM_HERE, base::BindLambdaForTesting([this]() {
                  parent_->model()->ContactPhoneForTesting("zzz");
                }));
            break;

          case 3:
            // Try some other phones.
            base::SequencedTaskRunnerHandle::Get()->PostTask(
                FROM_HERE, base::BindLambdaForTesting([this]() {
                  parent_->model()->ContactPhoneForTesting("aaa");
                }));
            break;

          case 4:
            // All done. Discover a virtual authenticator in order to resolve
            // the request.
            add_authenticator_callback_.Run();
            break;

          default:
            CHECK(false);
        }

        contact_step_number_++;
      });
    }

   private:
    // PendingDiscovery yields a single virtual authenticator when requested to
    // do so by calling the result of |GetAddAuthenticatorCallback|.
    class PendingDiscovery : public device::FidoDeviceDiscovery,
                             public base::SupportsWeakPtr<PendingDiscovery> {
     public:
      explicit PendingDiscovery(device::FidoTransportProtocol transport)
          : FidoDeviceDiscovery(transport) {}

      base::RepeatingClosure GetAddAuthenticatorCallback() {
        return base::BindRepeating(&PendingDiscovery::AddAuthenticator,
                                   AsWeakPtr());
      }

     protected:
      void StartInternal() override {
        base::SequencedTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::BindOnce(&PendingDiscovery::NotifyDiscoveryStarted,
                                      AsWeakPtr(), /*success=*/true));
      }

     private:
      void AddAuthenticator() {
        scoped_refptr<device::VirtualFidoDevice::State> state(
            new device::VirtualFidoDevice::State);
        static const uint8_t kCredentialID[] = {1, 2, 3, 4};
        state->InjectRegistration(kCredentialID, "www.example.com");

        AddDevice(std::make_unique<device::VirtualU2fDevice>(state));
      }
    };

    const raw_ptr<WebAuthnCableSecondFactor> parent_;
    base::RepeatingCallback<void(size_t)> invalid_pairing_callback_;
    base::RepeatingClosure add_authenticator_callback_;
    int contact_step_number_ = 0;
  };

  class DelegateObserver
      : public ChromeAuthenticatorRequestDelegate::TestObserver {
   public:
    explicit DelegateObserver(WebAuthnCableSecondFactor* test)
        : parent_(test) {}

    void Created(ChromeAuthenticatorRequestDelegate* delegate) override {
      // Only a single delegate should be observed.
      CHECK(!parent_->model());
    }

    std::vector<std::unique_ptr<device::cablev2::Pairing>>
    GetCablePairingsFromSyncedDevices() override {
      std::vector<std::unique_ptr<device::cablev2::Pairing>> ret;

      ret.emplace_back(TestPhone("name1", /*public_key=*/0,
                                 /*last_updated=*/base::Time::FromTimeT(1),
                                 /*channel_priority=*/1));

      // The same public key as phone1, but a newer timestamp. It
      // should shadow the first.
      ret.emplace_back(TestPhone("name2", /*public_key=*/0,
                                 /*last_updated=*/base::Time::FromTimeT(2),
                                 /*channel_priority=*/1));

      // Same name as the second, but a higher channel priority. It should take
      // priority over it.
      ret.emplace_back(TestPhone("name2", /*public_key=*/1,
                                 /*last_updated=*/base::Time::FromTimeT(2),
                                 /*channel_priority=*/2));

      // Same name as second and third, but a newer timestamp than the third. It
      // should be tried first.
      ret.emplace_back(TestPhone("name2", /*public_key=*/2,
                                 /*last_updated=*/base::Time::FromTimeT(3),
                                 /*channel_priority=*/2));

      // A different device with a name that should sort first.
      ret.emplace_back(TestPhone("aaa", /*public_key=*/3,
                                 /*last_updated=*/base::Time::FromTimeT(3),
                                 /*channel_priority=*/2));

      // A different device with a name that should sort last.
      ret.emplace_back(TestPhone("zzz", /*public_key=*/4,
                                 /*last_updated=*/base::Time::FromTimeT(3),
                                 /*channel_priority=*/2));

      return ret;
    }

    void OnTransportAvailabilityEnumerated(
        ChromeAuthenticatorRequestDelegate* delegate,
        device::FidoRequestHandlerBase::TransportAvailabilityInfo* tai)
        override {
      tai->available_transports.insert(
          device::FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy);
      tai->is_ble_powered = true;
    }

    void UIShown(ChromeAuthenticatorRequestDelegate* delegate) override {
      parent_->model() = delegate->dialog_model();
      LOG(ERROR) << static_cast<void*>(parent_->model());

      for (const auto& name : parent_->model()->paired_phone_names()) {
        parent_->trace() << "UINAME: " << name << std::endl;
      }

      // Simulate a click on the transport selection sheet.
      parent_->model()->ContactPhoneForTesting("name2");
    }

    void CableV2ExtensionSeen(
        base::span<const uint8_t> server_link_data,
        base::span<const uint8_t> experiments,
        AuthenticatorRequestDialogModel::ExperimentServerLinkSheet,
        AuthenticatorRequestDialogModel::ExperimentServerLinkTitle) override {}

   private:
    std::unique_ptr<device::cablev2::Pairing> TestPhone(const char* name,
                                                        uint8_t public_key,
                                                        base::Time last_updated,
                                                        int channel_priority) {
      auto phone = std::make_unique<device::cablev2::Pairing>();
      phone->name = name;
      phone->contact_id = {10, 11, 12};
      phone->id = {4, 5, 6};
      std::fill(phone->peer_public_key_x962.begin(),
                phone->peer_public_key_x962.end(), public_key);
      phone->last_updated = last_updated;
      phone->channel_priority = channel_priority;
      return phone;
    }

    const raw_ptr<WebAuthnCableSecondFactor> parent_;
  };

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::ostringstream trace_;
  AuthenticatorRequestDialogModel* model_ = nullptr;
};

// TODO(https://crbug.com/1219708): this test is flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_Test DISABLED_Test
#else
#define MAYBE_Test Test
#endif
IN_PROC_BROWSER_TEST_F(WebAuthnCableSecondFactor, MAYBE_Test) {
  DelegateObserver observer(this);
  ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(&observer);
  content::AuthenticatorEnvironment::GetInstance()
      ->ReplaceDefaultDiscoveryFactoryForTesting(
          std::make_unique<DiscoveryFactory>(this));

  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), https_server_.GetURL("www.example.com", "/title1.html")));

  std::string result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      browser()->tab_strip_model()->GetActiveWebContents(),
      kGetAssertionCredID1234, &result));

  constexpr char kExpectedTrace[] = R"(
PAIRING: aaa 03030303 040506
PAIRING: name2 02020202 040506
PAIRING: name2 01010101 040506
PAIRING: name2 00000000 040506
PAIRING: zzz 04040404 040506
UINAME: aaa
UINAME: name2
UINAME: zzz
CONTACT: phone_instance=1 step=0
CONTACT: phone_instance=2 step=1
CONTACT: phone_instance=3 step=2
CONTACT: phone_instance=4 step=3
CONTACT: phone_instance=0 step=4
)";
  EXPECT_EQ(kExpectedTrace, trace_.str());
  EXPECT_EQ("webauthn: OK", result);
}

}  // namespace
