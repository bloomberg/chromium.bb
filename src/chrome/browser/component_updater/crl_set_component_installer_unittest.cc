// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/crl_set_component_installer.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/test_data_directory.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/network_service.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace component_updater {

class CRLSetComponentInstallerTest : public PlatformTest {
 public:
  CRLSetComponentInstallerTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        test_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        network_service_(std::make_unique<network::NetworkService>(nullptr)) {}

  void SetUp() override {
    PlatformTest::SetUp();

    test_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
    test_server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("chrome/test/data")));
    ASSERT_TRUE(test_server_.Start());

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    policy_ = std::make_unique<CRLSetPolicy>();
    policy_->SetNetworkServiceForTesting(network_service_.get());
  }

  void SimulateCrash() {
    network_service_.reset();
    network_service_ = std::make_unique<network::NetworkService>(nullptr);
    policy_->SetNetworkServiceForTesting(network_service_.get());
  }

  void LoadURL(const GURL& url) {
    network::ResourceRequest request;
    request.url = url;
    request.method = "GET";
    request.request_initiator = url::Origin();

    client_ = std::make_unique<network::TestURLLoaderClient>();
    mojo::Remote<network::mojom::URLLoaderFactory> loader_factory;
    network::mojom::URLLoaderFactoryParamsPtr params =
        network::mojom::URLLoaderFactoryParams::New();
    params->process_id = 0;
    params->is_corb_enabled = false;
    network_context_->CreateURLLoaderFactory(
        loader_factory.BindNewPipeAndPassReceiver(), std::move(params));
    loader_.reset();
    loader_factory->CreateLoaderAndStart(
        loader_.BindNewPipeAndPassReceiver(), 1, 1,
        network::mojom::kURLLoadOptionSendSSLInfoWithResponse |
            network::mojom::kURLLoadOptionSendSSLInfoForCertificateError,
        request, client_->CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
    client_->RunUntilComplete();
  }

  void InstallCRLSet(const base::FilePath& raw_crl_file) {
    base::CopyFile(raw_crl_file, temp_dir_.GetPath().AppendASCII("crl-set"));
    ASSERT_TRUE(policy_->VerifyInstallation(base::DictionaryValue(),
                                            temp_dir_.GetPath()));
    policy_->ComponentReady(base::Version("1.0"), temp_dir_.GetPath(),
                            std::make_unique<base::DictionaryValue>());
    task_environment_.RunUntilIdle();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  net::EmbeddedTestServer test_server_;

  std::unique_ptr<CRLSetPolicy> policy_;
  std::unique_ptr<network::TestURLLoaderClient> client_;
  std::unique_ptr<network::NetworkService> network_service_;
  mojo::Remote<network::mojom::NetworkContext> network_context_;
  mojo::Remote<network::mojom::URLLoader> loader_;
  base::ScopedTempDir temp_dir_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CRLSetComponentInstallerTest);
};

TEST_F(CRLSetComponentInstallerTest, ConfiguresOnInstall) {
  network_service_->CreateNetworkContext(
      network_context_.BindNewPipeAndPassReceiver(),
      network::mojom::NetworkContextParams::New());

  // Ensure the test server can load by default.
  LoadURL(test_server_.GetURL("/empty.html"));
  ASSERT_EQ(net::OK, client_->completion_status().error_code);

  // Simulate a CRLSet being installed.
  ASSERT_NO_FATAL_FAILURE(
      InstallCRLSet(net::GetTestCertsDirectory().AppendASCII(
          "crlset_known_interception_by_root.raw")));

  // Ensure the test server is now flagged as a known MITM certificate.
  LoadURL(test_server_.GetURL("/empty.html"));
  ASSERT_EQ(net::OK, client_->completion_status().error_code);
  ASSERT_TRUE(client_->ssl_info());
  EXPECT_TRUE(client_->ssl_info()->cert_status &
              net::CERT_STATUS_KNOWN_INTERCEPTION_DETECTED);
}

TEST_F(CRLSetComponentInstallerTest, ReconfiguresAfterRestartWithCRLSet) {
  network_service_->CreateNetworkContext(
      network_context_.BindNewPipeAndPassReceiver(),
      network::mojom::NetworkContextParams::New());

  // Ensure the test server can load by default.
  LoadURL(test_server_.GetURL("/empty.html"));
  ASSERT_EQ(net::OK, client_->completion_status().error_code);

  // Simulate a CRLSet being installed.
  ASSERT_NO_FATAL_FAILURE(
      InstallCRLSet(net::GetTestCertsDirectory().AppendASCII(
          "crlset_known_interception_by_root.raw")));

  // Ensure the test server is now flagged as a known MITM certificate.
  LoadURL(test_server_.GetURL("/empty.html"));
  ASSERT_EQ(net::OK, client_->completion_status().error_code);
  ASSERT_TRUE(client_->ssl_info());
  EXPECT_TRUE(client_->ssl_info()->cert_status &
              net::CERT_STATUS_KNOWN_INTERCEPTION_DETECTED);

  // Simulate a Network Service crash
  SimulateCrash();
  CRLSetPolicy::ReconfigureAfterNetworkRestart();
  task_environment_.RunUntilIdle();

  network_context_.reset();
  network_service_->CreateNetworkContext(
      network_context_.BindNewPipeAndPassReceiver(),
      network::mojom::NetworkContextParams::New());

  // Ensure the test server is still flagged even with a new context and
  // service.
  LoadURL(test_server_.GetURL("/empty.html"));
  ASSERT_EQ(net::OK, client_->completion_status().error_code);
  ASSERT_TRUE(client_->ssl_info());
  EXPECT_TRUE(client_->ssl_info()->cert_status &
              net::CERT_STATUS_KNOWN_INTERCEPTION_DETECTED);
}

TEST_F(CRLSetComponentInstallerTest, ReconfiguresAfterRestartWithNoCRLSet) {
  network_service_->CreateNetworkContext(
      network_context_.BindNewPipeAndPassReceiver(),
      network::mojom::NetworkContextParams::New());

  // Ensure the test server can load by default.
  LoadURL(test_server_.GetURL("/empty.html"));
  ASSERT_EQ(net::OK, client_->completion_status().error_code);

  // Simulate a Network Service crash
  SimulateCrash();
  CRLSetPolicy::ReconfigureAfterNetworkRestart();
  task_environment_.RunUntilIdle();

  network_context_.reset();
  network_service_->CreateNetworkContext(
      network_context_.BindNewPipeAndPassReceiver(),
      network::mojom::NetworkContextParams::New());

  // Ensure the test server can still load.
  LoadURL(test_server_.GetURL("/empty.html"));
  ASSERT_EQ(net::OK, client_->completion_status().error_code);
}

}  // namespace component_updater
