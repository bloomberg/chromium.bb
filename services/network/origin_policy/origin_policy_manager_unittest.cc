// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/test/scoped_task_environment.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/network_context.h"
#include "services/network/network_service.h"
#include "services/network/origin_policy/origin_policy_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

void DummyRetrieveOriginPolicyCallback(
    const network::mojom::OriginPolicyPtr result) {}

}  // namespace

class OriginPolicyManagerTest : public testing::Test {
 public:
  OriginPolicyManagerTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO) {
    network_service_ = NetworkService::CreateForTesting();

    auto context_params = mojom::NetworkContextParams::New();
    // Use a fixed proxy config, to avoid dependencies on local network
    // configuration.
    context_params->initial_proxy_config =
        net::ProxyConfigWithAnnotation::CreateDirect();
    network_context_ = std::make_unique<NetworkContext>(
        network_service_.get(), mojo::MakeRequest(&network_context_ptr_),
        std::move(context_params));
    manager_ = std::make_unique<OriginPolicyManager>(
        network_context_->CreateUrlLoaderFactoryForNetworkService());
  }

  const url::Origin& test_server_origin() const { return test_server_origin_; }

  OriginPolicyManager* manager() { return manager_.get(); }

  NetworkContext* network_context() { return network_context_.get(); }

  void WaitUntilResponseHandled() { response_run_loop.Run(); }

 protected:
  // testing::Test implementation.
  void SetUp() override {
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &OriginPolicyManagerTest::HandleResponse, base::Unretained(this)));

    EXPECT_TRUE(test_server_.Start());

    test_server_origin_ = url::Origin::Create(test_server_.base_url());
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleResponse(
      const net::test_server::HttpRequest& request) {
    response_run_loop.Quit();
    std::unique_ptr<net::test_server::BasicHttpResponse> response =
        std::make_unique<net::test_server::BasicHttpResponse>();

    if (request.relative_url == "/.well-known/origin-policy/policy-1") {
      response->set_code(net::HTTP_OK);
      response->set_content("manifest-1");
    } else if (request.relative_url == "/.well-known/origin-policy/policy-2") {
      response->set_code(net::HTTP_OK);
      response->set_content("manifest-2");
    } else if (request.relative_url ==
               "/.well-known/origin-policy/redirect-policy") {
      response->set_code(net::HTTP_FOUND);
      response->AddCustomHeader(
          "Location",
          test_server_.GetURL("/.well-known/origin-policy/policy-1").spec());
    } else if (request.relative_url ==
               "/.well-known/origin-policy/policy/policy-3") {
      response->set_code(net::HTTP_OK);
      response->set_content("manifest-3");
    } else if (request.relative_url == "/.well-known/delayed") {
      return std::make_unique<net::test_server::HungResponse>();
    } else {
      response->set_code(net::HTTP_NOT_FOUND);
    }

    return std::move(response);
  }

  base::test::ScopedTaskEnvironment scoped_task_environment_;

  std::unique_ptr<NetworkService> network_service_;
  std::unique_ptr<NetworkContext> network_context_;
  mojom::NetworkContextPtr network_context_ptr_;
  std::unique_ptr<OriginPolicyManager> manager_;
  net::test_server::EmbeddedTestServer test_server_;
  url::Origin test_server_origin_;
  base::RunLoop response_run_loop;

  DISALLOW_COPY_AND_ASSIGN(OriginPolicyManagerTest);
};

TEST_F(OriginPolicyManagerTest, AddBinding) {
  mojom::OriginPolicyManagerPtr origin_policy_ptr;
  mojom::OriginPolicyManagerRequest origin_policy_request(
      mojo::MakeRequest(&origin_policy_ptr));

  EXPECT_EQ(0u, manager()->GetBindingsForTesting().size());

  manager()->AddBinding(std::move(origin_policy_request));

  EXPECT_EQ(1u, manager()->GetBindingsForTesting().size());
}

TEST_F(OriginPolicyManagerTest, ParseHeaders) {
  const struct {
    const char* header;
    const char* expected_policy_version;
    const char* expected_report_to;
  } kTests[] = {
      // The common cases: We expect >99% of headers to look like these:
      {"policy=policy", "policy", ""},
      {"policy=policy, report-to=endpoint", "policy", "endpoint"},
      {"", "", ""},
      {"report-to=endpoint", "", ""},

      // Delete a policy. This better work.
      {"0", "0", ""},
      {"policy=0", "0", ""},
      {"policy=\"0\"", "0", ""},
      {"policy=0, report-to=endpoint", "0", "endpoint"},

      // Order, please!
      {"policy=policy, report-to=endpoint", "policy", "endpoint"},
      {"report-to=endpoint, policy=policy", "policy", "endpoint"},

      // Quoting:
      {"policy=\"policy\"", "policy", ""},
      {"policy=\"policy\", report-to=endpoint", "policy", "endpoint"},
      {"policy=\"policy\", report-to=\"endpoint\"", "policy", "endpoint"},
      {"policy=policy, report-to=\"endpoint\"", "policy", "endpoint"},

      // Whitespace, and funky but valid syntax:
      {"  policy  =   policy  ", "policy", ""},
      {" policy = \t policy ", "policy", ""},
      {" policy \t= \t \"policy\"  ", "policy", ""},
      {" policy = \" policy \" ", "policy", ""},
      {" , policy = policy , report-to=endpoint , ", "policy", "endpoint"},

      // Valid policy, invalid report-to:
      {"policy=policy, report-to endpoint", "", ""},
      {"policy=policy, report-to=here, report-to=there", "", ""},
      {"policy=policy, \"report-to\"=endpoint", "", ""},

      // Invalid policy, valid report-to:
      {"policy=policy1, policy=policy2", "", ""},
      {"policy, report-to=r", "", ""},

      // Invalid everything:
      {"one two three", "", ""},
      {"one, two, three", "", ""},
      {"policy report-to=endpoint", "", ""},
      {"policy=policy report-to=endpoint", "", ""},

      // Forward compatibility, ignore unknown keywords:
      {"policy=pol, report-to=endpoint, unknown=keyword", "pol", "endpoint"},
      {"unknown=keyword, policy=pol, report-to=endpoint", "pol", "endpoint"},
      {"policy=pol, unknown=keyword", "pol", ""},
      {"policy=policy, report_to=endpoint", "policy", ""},
      {"policy=policy, reportto=endpoint", "policy", ""},

      // Using relative paths
      {"policy=..", "", ""},
      {"policy=../some-policy", "", ""},
      {"policy=policy/../some-policy", "", ""},
      {"policy=.., report-to=endpoint", "", ""},
      {"report-to=endpoint, policy=..", "", ""},
      {"policy=. .", "", ""},
      {"policy= ..", "", ""},
      {"policy=.. ", "", ""},
      {"policy=.", "", ""},
      {"policy=policy/.", "", ""},
      {"policy=./some-policy", "", ""},

      // Repeated arguments
      {"policy=p1, policy=p2", "", ""},
      {"policy=, policy=p2", "", ""},
      {"report-to=r1, report-to=r2", "", ""},
      {"report-to=, report-to=r2", "", ""},
      {"policy=, policy=p2, report-to=r1", "", ""},
      {"policy=, policy=, report-to=r1", "", ""},
      {"policy=p1, report-to=r1, report-to=r2", "", ""},
  };
  for (const auto& test : kTests) {
    SCOPED_TRACE(test.header);
    OriginPolicyManager::OriginPolicyHeaderValues result = OriginPolicyManager::
        GetRequestedPolicyAndReportGroupFromHeaderStringForTesting(test.header);
    EXPECT_EQ(test.expected_policy_version, result.policy_version);
    EXPECT_EQ(test.expected_report_to, result.report_to);
  }
}

// Helper class for starting saving a retrieved policy result
class TestOriginPolicyManagerResult {
 public:
  TestOriginPolicyManagerResult() {}

  void RetrieveOriginPolicy(const std::string& header_value,
                            OriginPolicyManagerTest* fixture) {
    fixture->manager()->RetrieveOriginPolicy(
        fixture->test_server_origin(), header_value,
        base::BindOnce(&TestOriginPolicyManagerResult::Callback,
                       base::Unretained(this)));
    run_loop_.Run();
  }

  const mojom::OriginPolicy* origin_policy_result() const {
    return origin_policy_result_.get();
  }

 private:
  void Callback(const mojom::OriginPolicyPtr result) {
    origin_policy_result_ = result.Clone();
    run_loop_.Quit();
  }

  base::RunLoop run_loop_;
  mojom::OriginPolicyPtr origin_policy_result_;

  DISALLOW_COPY_AND_ASSIGN(TestOriginPolicyManagerResult);
};

TEST_F(OriginPolicyManagerTest, EndToEndPolicyRetrieve) {
  const struct {
    std::string header;
    mojom::OriginPolicyState expected_state;
    std::string expected_raw_policy;
  } kTests[] = {
      {"policy=policy-1", mojom::OriginPolicyState::kLoaded, "manifest-1"},
      {"policy=policy-2", mojom::OriginPolicyState::kLoaded, "manifest-2"},
      {"policy=redirect-policy", mojom::OriginPolicyState::kInvalidRedirect,
       ""},
      {"policy=policy-2, report-to=endpoint", mojom::OriginPolicyState::kLoaded,
       "manifest-2"},

      {"", mojom::OriginPolicyState::kCannotLoadPolicy, ""},
      {"unknown=keyword", mojom::OriginPolicyState::kCannotLoadPolicy, ""},
      {"report_to=endpoint", mojom::OriginPolicyState::kCannotLoadPolicy, ""},
      {"policy=policy/policy-3", mojom::OriginPolicyState::kCannotLoadPolicy,
       ""},

      {"policy=../some-policy", mojom::OriginPolicyState::kCannotLoadPolicy,
       ""},
      {"policy=..", mojom::OriginPolicyState::kCannotLoadPolicy, ""},
      {"policy=.", mojom::OriginPolicyState::kCannotLoadPolicy, ""},
      {"policy=something-else/..", mojom::OriginPolicyState::kCannotLoadPolicy,
       ""},
  };

  for (const auto& test : kTests) {
    SCOPED_TRACE(test.header);

    TestOriginPolicyManagerResult tester;
    tester.RetrieveOriginPolicy(test.header, this);
    EXPECT_EQ(test.expected_state, tester.origin_policy_result()->state);
    if (test.expected_raw_policy.empty()) {
      EXPECT_FALSE(tester.origin_policy_result()->contents);
    } else {
      EXPECT_EQ(test.expected_raw_policy,
                tester.origin_policy_result()->contents->raw_policy);
    }
  }
}

// Destroying un-invoked OnceCallback while the binding they are on is still
// live results in a DCHECK. This tests this scenario by destroying the manager
// while it's in the middle of fetching a policy that has a delay on the
// response. The manager will be destroyed before the return callback is called.
TEST_F(OriginPolicyManagerTest, DestroyWhileCallbackUninvoked) {
  {
    mojom::OriginPolicyManagerPtr origin_policy_ptr;

    OriginPolicyManager manager(
        network_context()->CreateUrlLoaderFactoryForNetworkService());

    manager.AddBinding(mojo::MakeRequest(&origin_policy_ptr));

    // This fetch will still be ongoing when the manager is destroyed.
    origin_policy_ptr->RetrieveOriginPolicy(
        test_server_origin(), "policy=delayed",
        base::BindOnce(&DummyRetrieveOriginPolicyCallback));

    WaitUntilResponseHandled();
  }
  // At this point if we have not hit the DCHECK in OnIsConnectedComplete, then
  // the test has passed.
}

}  // namespace network
