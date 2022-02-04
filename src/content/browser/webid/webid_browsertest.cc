// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/browser/webid/test/fake_identity_request_dialog_controller.h"
#include "content/browser/webid/test/webid_test_content_browser_client.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"
#include "url/origin.h"

using net::EmbeddedTestServer;
using net::HttpStatusCode;
using net::test_server::BasicHttpResponse;
using net::test_server::HttpMethod;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace content {

namespace {

constexpr char kRpHostName[] = "rp.example";
constexpr char kIdpOrigin[] = "https://idp.example.org";
constexpr char kExpectedWellKnownPath[] = "/.well-known/fedcm";
constexpr char kIdpEndpointRelativeValue[] = "/fedcm/sign-in";
constexpr char kTestWellKnownResponseBody[] =
    "{\"idp_endpoint\": \"/fedcm/sign-in\"}";
constexpr char kTestIdpEndpointBody[] = "{\"signin_url\": \"/fedcm/\"}";
constexpr char kTestContentType[] = "application/json";
constexpr char kIdpForbiddenHeader[] = "Sec-FedCM-CSRF";
// Value will be added here as token introspection is implemented.
constexpr char kIdToken[] = "[not a real token]";
constexpr char kIdpEndpointTokenResponse[] =
    "{\"id_token\": \"[not a real token]\"}";

// This class implements the IdP logic, and responds to requests sent to the
// test HTTP server.
class IdpTestServer {
 public:
  struct ResponseDetails {
    HttpStatusCode status_code;
    std::string body;
    std::string content_type;
  };

  IdpTestServer() = default;
  ~IdpTestServer() = default;

  IdpTestServer(const IdpTestServer&) = delete;
  IdpTestServer& operator=(const IdpTestServer&) = delete;

  std::unique_ptr<HttpResponse> HandleRequest(const HttpRequest& request) {
    // RP files are fetched from the /test base directory. Assume anything
    // to other paths is directed to the IdP.
    if (request.relative_url.rfind("/test", 0) == 0)
      return nullptr;

    if (request.all_headers.find(kIdpForbiddenHeader) != std::string::npos) {
      EXPECT_EQ(request.headers.at(kIdpForbiddenHeader).size(), 12ul);
    }

    auto response = std::make_unique<BasicHttpResponse>();
    if (IsWellKnownRequest(request)) {
      BuildResponseFromDetails(*response.get(), well_known_details_);
      return response;
    }

    if (IsIdpEndpointRequest(request)) {
      BuildResponseFromDetails(*response.get(), idp_endpoint_details_);
      return response;
    }

    return nullptr;
  }

  void SetWellKnownResponseDetails(ResponseDetails details) {
    well_known_details_ = details;
  }

  void SetIdpEndpointResponseDetails(ResponseDetails details) {
    idp_endpoint_details_ = details;
  }

 private:
  bool IsWellKnownRequest(const HttpRequest& request) {
    if (request.method == HttpMethod::METHOD_GET &&
        request.relative_url == kExpectedWellKnownPath) {
      return true;
    }
    return false;
  }

  bool IsIdpEndpointRequest(const HttpRequest& request) {
    if (request.method == HttpMethod::METHOD_GET &&
        request.relative_url.rfind(kIdpEndpointRelativeValue, 0) == 0 &&
        request.all_headers.find(kIdpForbiddenHeader) != std::string::npos) {
      return true;
    }
    return false;
  }

  void BuildResponseFromDetails(BasicHttpResponse& response,
                                const ResponseDetails& details) {
    response.set_code(details.status_code);
    response.set_content(details.body);
    response.set_content_type(details.content_type);
  }

  // Response values for the types of requests that are sent to the IdP.
  // These have default values that can be overridden for specific tests.
  ResponseDetails well_known_details_ = {
      net::HTTP_OK, kTestWellKnownResponseBody, kTestContentType};
  ResponseDetails idp_endpoint_details_ = {net::HTTP_OK, kTestIdpEndpointBody,
                                           kTestContentType};
};

}  // namespace

// TODO(yigu): Update the tests (e.g. well-known) to cover mediation mode.
class WebIdBrowserTest : public ContentBrowserTest {
 public:
  WebIdBrowserTest() = default;
  ~WebIdBrowserTest() override = default;

  WebIdBrowserTest(const WebIdBrowserTest&) = delete;
  WebIdBrowserTest& operator=(const WebIdBrowserTest&) = delete;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    idp_server_ = std::make_unique<IdpTestServer>();
    https_server().SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server().ServeFilesFromSourceDirectory(GetTestDataFilePath());
    https_server().RegisterRequestHandler(base::BindRepeating(
        &IdpTestServer::HandleRequest, base::Unretained(idp_server_.get())));
    ASSERT_TRUE(https_server().Start());

    EXPECT_TRUE(NavigateToURL(
        shell(), https_server().GetURL(kRpHostName, "/title1.html")));

    test_browser_client_ = std::make_unique<WebIdTestContentBrowserClient>();
    SetTestIdentityRequestDialogController(
        IdentityRequestDialogController::UserApproval::kApproved,
        IdentityRequestDialogController::UserApproval::kApproved);
    old_client_ = SetBrowserClientForTesting(test_browser_client_.get());
  }

  void TearDown() override {
    CHECK_EQ(SetBrowserClientForTesting(old_client_),
             test_browser_client_.get());
    ContentBrowserTest::TearDown();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    std::vector<base::Feature> features;

    // kSplitCacheByNetworkIsolationKey feature is needed to verify
    // that the network shard for fetching the .well-known file is different
    // from that used for other IdP transactions, to prevent data leakage.
    features.push_back(net::features::kSplitCacheByNetworkIsolationKey);
    features.push_back(features::kFedCm);
    scoped_feature_list_.InitWithFeatures(features, {});

    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  net::EmbeddedTestServer& https_server() { return https_server_; }

  std::string BaseIdpUrl() {
    return std::string(kIdpOrigin) + ":" +
           base::NumberToString(https_server().port());
  }

  std::string GetBasicRequestString() {
    return R"(
        (async () => {
          var x = (await navigator.credentials.get({
            federated: {
              providers: [{
                url: ')" +
           BaseIdpUrl() + R"(',
                clientId: 'client_id_1',
                nonce: '12345',
              }],
              mode: "permission",
            }
          }));
          return x;
        }) ()
    )";
  }

  IdpTestServer* idp_server() { return idp_server_.get(); }

  void SetTestIdentityRequestDialogController(
      IdentityRequestDialogController::UserApproval initial_permission_response,
      IdentityRequestDialogController::UserApproval token_exchange_response) {
    auto controller = std::make_unique<FakeIdentityRequestDialogController>(
        initial_permission_response, token_exchange_response, kIdToken);
    test_browser_client_->SetIdentityRequestDialogController(
        std::move(controller));
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  std::unique_ptr<IdpTestServer> idp_server_;
  std::unique_ptr<WebIdTestContentBrowserClient> test_browser_client_;
  raw_ptr<ContentBrowserClient> old_client_ = nullptr;
};

// Verify a standard login flow with IdP sign-in page.
IN_PROC_BROWSER_TEST_F(WebIdBrowserTest, FullLoginFlow) {
  EXPECT_EQ(std::string(kIdToken), EvalJs(shell(), GetBasicRequestString()));
}

// Verify abbreviated login flow where IdP returns a token from the
// |idp_endpoint|.
IN_PROC_BROWSER_TEST_F(WebIdBrowserTest, FastLoginFlow) {
  idp_server()->SetIdpEndpointResponseDetails(
      {net::HTTP_OK, kIdpEndpointTokenResponse, kTestContentType});

  EXPECT_EQ(std::string(kIdToken), EvalJs(shell(), GetBasicRequestString()));
}

// Verify full login flow where the IdP uses absolute rather than relative
// URLs.
IN_PROC_BROWSER_TEST_F(WebIdBrowserTest, AbsoluteURLs) {
  std::string idp_endpoint_absolute_url =
      BaseIdpUrl() + kIdpEndpointRelativeValue;
  std::string well_known_response_body =
      "{\"idp_endpoint\": \"" + idp_endpoint_absolute_url + "\"}";
  idp_server()->SetWellKnownResponseDetails(
      {net::HTTP_OK, well_known_response_body, kTestContentType});

  std::string signin_url_absolute_url = BaseIdpUrl() + "/fedcm";
  std::string idp_endpoint_response_body =
      "{\"signin_url\": \"" + signin_url_absolute_url + "\"}";
  idp_server()->SetIdpEndpointResponseDetails(
      {net::HTTP_OK, idp_endpoint_response_body, kTestContentType});

  EXPECT_EQ(std::string(kIdToken), EvalJs(shell(), GetBasicRequestString()));
}

// Simulate the user declining the permission dialog to allow the request to
// proceed.
IN_PROC_BROWSER_TEST_F(WebIdBrowserTest, InitialPermissionDeclined) {
  SetTestIdentityRequestDialogController(
      IdentityRequestDialogController::UserApproval::kDenied,
      IdentityRequestDialogController::UserApproval::kApproved);

  std::string expected_error =
      "a JavaScript error: \"AbortError: User "
      "declined the sign-in attempt.\"\n";
  EXPECT_EQ(expected_error, EvalJs(shell(), GetBasicRequestString()).error);
}

// Simulate the user declining to share the ID token after it has been
// provided.
// TODO(kenrb): Add a variant of this test that denies approval when the token
// has been provided from the idp_endpoint. Currently the permission prompt does
// not get displayed in that case.
IN_PROC_BROWSER_TEST_F(WebIdBrowserTest, TokenExchangePermissionDeclined) {
  SetTestIdentityRequestDialogController(
      IdentityRequestDialogController::UserApproval::kApproved,
      IdentityRequestDialogController::UserApproval::kDenied);

  std::string expected_error =
      "a JavaScript error: \"AbortError: User "
      "declined the sign-in attempt.\"\n";
  EXPECT_EQ(expected_error, EvalJs(shell(), GetBasicRequestString()).error);
}

// Verify an error is returned when WebID is not supported by the provided IdP.
IN_PROC_BROWSER_TEST_F(WebIdBrowserTest, WebIdNotSupported) {
  idp_server()->SetWellKnownResponseDetails({net::HTTP_NOT_FOUND, "", ""});

  std::string expected_error =
      "a JavaScript error: \"NetworkError: The provider's .well-known "
      "configuration cannot be found.\"\n";
  EXPECT_EQ(expected_error, EvalJs(shell(), GetBasicRequestString()).error);
}

// Verify an attempt to invoke WebID with an insecure IDP path fails.
IN_PROC_BROWSER_TEST_F(WebIdBrowserTest, FailsOnHTTP) {
  std::string script = R"(
        (async () => {
          var x = (await navigator.credentials.get({
            federated: {
              providers: [{
                url: 'http://idp.example)" +
                       base::NumberToString(https_server().port()) + R"(',
                clientId: 'client_id_1',
                nonce: '12345',
              }]
            }
          }));
          return x;
        }) ()
    )";

  std::string expected_error =
      "a JavaScript error: \"NetworkError: Error "
      "retrieving an id token.\"\n";
  EXPECT_EQ(expected_error, EvalJs(shell(), script).error);
}

}  // namespace content
