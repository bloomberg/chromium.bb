// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/files/file_util.h"
#include "base/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/test_completion_callback.h"
#include "net/cert/test_root_certs.h"
#include "net/cert/x509_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/gtest_util.h"
#include "net/test/test_data_directory.h"
#include "net/url_request/url_request_context.h"
#include "services/cert_verifier/cert_net_url_loader/cert_net_fetcher_url_loader.h"
#include "services/cert_verifier/cert_verifier_service_factory.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/mojom/cert_verifier_service.mojom.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/ssl_config_service_mojo.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cert_verifier {

// Base class for any tests that just need a NetworkService and a
// TaskEnvironment, and to create NetworkContexts using the NetworkService.
class SSLConfigServiceMojoTestWithCertVerifier
    : public testing::TestWithParam<bool> {
 public:
  SSLConfigServiceMojoTestWithCertVerifier()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO),
        service_(network::NetworkService::CreateForTesting()),
        cert_verifier_service_impl_(
            cert_verifier_service_remote_.BindNewPipeAndPassReceiver()) {}
  ~SSLConfigServiceMojoTestWithCertVerifier() override = default;

  void SetUp() override {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(
          network::features::kCertVerifierService);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          network::features::kCertVerifierService);
    }
  }

  void DestroyService() { service_.reset(); }

  network::mojom::NetworkContextParamsPtr CreateNetworkContextParams() {
    network::mojom::CertVerifierCreationParamsPtr
        cert_verifier_creation_params =
            network::mojom::CertVerifierCreationParams::New();
    network::mojom::CertVerifierParamsPtr cert_verifier_params;
    if (base::FeatureList::IsEnabled(network::features::kCertVerifierService)) {
      mojo::PendingRemote<mojom::CertVerifierService> cv_service_remote;

      auto cv_service_remote_params =
          network::mojom::CertVerifierServiceRemoteParams::New();

      // Create a cert verifier service.
      cert_verifier_service_impl_.GetNewCertVerifierForTesting(
          cv_service_remote.InitWithNewPipeAndPassReceiver(),
          std::move(cert_verifier_creation_params),
          &cert_net_fetcher_url_loader_);

      cv_service_remote_params->cert_verifier_service =
          std::move(cv_service_remote);

      cert_verifier_params =
          network::mojom::CertVerifierParams::NewRemoteParams(
              std::move(cv_service_remote_params));
    } else {
      cert_verifier_params =
          network::mojom::CertVerifierParams::NewCreationParams(
              std::move(cert_verifier_creation_params));
    }

    network::mojom::NetworkContextParamsPtr params =
        network::mojom::NetworkContextParams::New();
    params->cert_verifier_params = std::move(cert_verifier_params);
    // Use a fixed proxy config, to avoid dependencies on local network
    // configuration.
    params->initial_proxy_config =
        net::ProxyConfigWithAnnotation::CreateDirect();
    return params;
  }

  void CreateNetworkContext(network::mojom::NetworkContextParamsPtr params) {
    network_context_ = std::make_unique<network::NetworkContext>(
        service_.get(), network_context_remote_.BindNewPipeAndPassReceiver(),
        std::move(params));
  }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

  network::NetworkService* service() const { return service_.get(); }

  network::NetworkContext* network_context() { return network_context_.get(); }

  mojo::Remote<network::mojom::NetworkContext>& network_context_remote() {
    return network_context_remote_;
  }

  CertNetFetcherURLLoader* cert_net_fetcher_url_loader() {
    return cert_net_fetcher_url_loader_.get();
  }

 private:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<network::NetworkService> service_;
  base::test::ScopedFeatureList scoped_feature_list_;

  mojo::Remote<network::mojom::NetworkContext> network_context_remote_;
  std::unique_ptr<network::NetworkContext> network_context_;

  mojo::Remote<mojom::CertVerifierServiceFactory> cert_verifier_service_remote_;
  CertVerifierServiceFactoryImpl cert_verifier_service_impl_;
  scoped_refptr<CertNetFetcherURLLoader> cert_net_fetcher_url_loader_;
};

#if !defined(OS_IOS) && !defined(OS_ANDROID)
TEST_P(SSLConfigServiceMojoTestWithCertVerifier, CRLSetIsApplied) {
  mojo::Remote<network::mojom::SSLConfigClient> ssl_config_client;

  network::mojom::NetworkContextParamsPtr context_params =
      CreateNetworkContextParams();
  context_params->ssl_config_client_receiver =
      ssl_config_client.BindNewPipeAndPassReceiver();
  CreateNetworkContext(std::move(context_params));

  network::SSLConfigServiceMojo* config_service =
      static_cast<network::SSLConfigServiceMojo*>(
          network_context()->url_request_context()->ssl_config_service());

  scoped_refptr<net::X509Certificate> root_cert =
      net::CreateCertificateChainFromFile(
          net::GetTestCertsDirectory(), "root_ca_cert.pem",
          net::X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  ASSERT_TRUE(root_cert);
  net::ScopedTestRoot test_root(root_cert.get());

  scoped_refptr<net::X509Certificate> leaf_cert =
      net::CreateCertificateChainFromFile(
          net::GetTestCertsDirectory(), "ok_cert.pem",
          net::X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
  ASSERT_TRUE(leaf_cert);

  // Ensure that |leaf_cert| is trusted without any CRLSet explicitly
  // configured.
  net::TestCompletionCallback callback1;
  net::CertVerifyResult cert_verify_result1;
  std::unique_ptr<net::CertVerifier::Request> request1;
  int result =
      network_context()->url_request_context()->cert_verifier()->Verify(
          net::CertVerifier::RequestParams(leaf_cert, "127.0.0.1",
                                           /*flags=*/0,
                                           /*ocsp_response=*/std::string(),
                                           /*sct_list=*/std::string()),
          &cert_verify_result1, callback1.callback(), &request1,
          net::NetLogWithSource());
  ASSERT_THAT(callback1.GetResult(result), net::test::IsOk());

  // Load a CRLSet that removes trust in |leaf_cert| by SPKI.
  scoped_refptr<net::CRLSet> crl_set;
  std::string crl_set_bytes;
  ASSERT_TRUE(base::ReadFileToString(
      net::GetTestCertsDirectory().AppendASCII("crlset_by_leaf_spki.raw"),
      &crl_set_bytes));
  ASSERT_TRUE(net::CRLSet::ParseAndStoreUnparsedData(crl_set_bytes, &crl_set));

  config_service->OnNewCRLSet(crl_set);

  // Ensure that |leaf_cert| is revoked, due to the CRLSet being applied.
  net::TestCompletionCallback callback2;
  net::CertVerifyResult cert_verify_result2;
  std::unique_ptr<net::CertVerifier::Request> request2;
  result = network_context()->url_request_context()->cert_verifier()->Verify(
      net::CertVerifier::RequestParams(leaf_cert, "127.0.0.1",
                                       /*flags=*/0,
                                       /*ocsp_response=*/std::string(),
                                       /*sct_list=*/std::string()),
      &cert_verify_result2, callback2.callback(), &request2,
      net::NetLogWithSource());
  ASSERT_THAT(callback2.GetResult(result),
              net::test::IsError(net::ERR_CERT_REVOKED));
}

INSTANTIATE_TEST_SUITE_P(All,
                         SSLConfigServiceMojoTestWithCertVerifier,
                         ::testing::Bool());
#endif  // !defined(OS_IOS) && !defined(OS_ANDROID)

}  // namespace cert_verifier
