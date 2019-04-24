// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/content_mock_cert_verifier.h"

#include "base/command_line.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/network_service_util.h"
#include "content/public/common/service_manager_connection.h"
#include "content/public/common/service_names.mojom.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/network_service_test_helper.h"
#include "mojo/public/cpp/bindings/sync_call_restrictions.h"
#include "services/network/network_context.h"
#include "services/network/public/cpp/features.h"
#include "services/service_manager/public/cpp/connector.h"

namespace content {

ContentMockCertVerifier::CertVerifier::CertVerifier(
    net::MockCertVerifier* verifier)
    : verifier_(verifier) {}

ContentMockCertVerifier::CertVerifier::~CertVerifier() = default;

void ContentMockCertVerifier::CertVerifier::set_default_result(
    int default_result) {
  verifier_->set_default_result(default_result);

  if (!base::FeatureList::IsEnabled(network::features::kNetworkService) ||
      IsInProcessNetworkService()) {
    return;
  }

  EnsureNetworkServiceTestInitialized();
  mojo::ScopedAllowSyncCallForTesting allow_sync_call;
  network_service_test_->MockCertVerifierSetDefaultResult(default_result);
}

void ContentMockCertVerifier::CertVerifier::AddResultForCert(
    scoped_refptr<net::X509Certificate> cert,
    const net::CertVerifyResult& verify_result,
    int rv) {
  AddResultForCertAndHost(cert, "*", verify_result, rv);
}

void ContentMockCertVerifier::CertVerifier::AddResultForCertAndHost(
    scoped_refptr<net::X509Certificate> cert,
    const std::string& host_pattern,
    const net::CertVerifyResult& verify_result,
    int rv) {
  verifier_->AddResultForCertAndHost(cert, host_pattern, verify_result, rv);

  if (!base::FeatureList::IsEnabled(network::features::kNetworkService) ||
      IsInProcessNetworkService()) {
    return;
  }

  EnsureNetworkServiceTestInitialized();
  mojo::ScopedAllowSyncCallForTesting allow_sync_call;
  network_service_test_->MockCertVerifierAddResultForCertAndHost(
      cert, host_pattern, verify_result, rv);
}

void ContentMockCertVerifier::CertVerifier::
    EnsureNetworkServiceTestInitialized() {
  if (!network_service_test_) {
    ServiceManagerConnection::GetForProcess()->GetConnector()->BindInterface(
        mojom::kNetworkServiceName, &network_service_test_);
  }
  // TODO(crbug.com/901026): Make sure the network process is started to avoid a
  // deadlock on Android.
  network_service_test_.FlushForTesting();
}

ContentMockCertVerifier::ContentMockCertVerifier()
    : mock_cert_verifier_(new net::MockCertVerifier()),
      cert_verifier_(mock_cert_verifier_.get()) {}

ContentMockCertVerifier::~ContentMockCertVerifier() {}

void ContentMockCertVerifier::SetUpCommandLine(
    base::CommandLine* command_line) {
  // Check here instead of the constructor since some tests may set the feature
  // flag in their constructor.
  if (!base::FeatureList::IsEnabled(network::features::kNetworkService) ||
      IsInProcessNetworkService()) {
    return;
  }

  // Enable the MockCertVerifier in the network process via a switch. This is
  // because it's too early to call the service manager at this point (it's not
  // created yet), and by the time we can call the service manager in
  // SetUpOnMainThread the main profile has already been created.
  command_line->AppendSwitch(switches::kUseMockCertVerifierForTesting);
}

void ContentMockCertVerifier::SetUpInProcessBrowserTestFixture() {
  if (IsInProcessNetworkService()) {
    network::NetworkContext::SetCertVerifierForTesting(
        mock_cert_verifier_.get());
  }
}

void ContentMockCertVerifier::TearDownInProcessBrowserTestFixture() {
  if (IsInProcessNetworkService())
    network::NetworkContext::SetCertVerifierForTesting(nullptr);
}

ContentMockCertVerifier::CertVerifier*
ContentMockCertVerifier::mock_cert_verifier() {
  return &cert_verifier_;
}

}  // namespace content
