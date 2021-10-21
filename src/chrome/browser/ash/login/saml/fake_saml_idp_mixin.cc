// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/fake_saml_idp_mixin.h"

#include "base/base64.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/login/test/fake_gaia_mixin.h"
#include "chrome/browser/ash/login/users/test_users.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/dbus/attestation/fake_attestation_client.h"
#include "net/base/url_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using ::net::test_server::BasicHttpResponse;
using ::net::test_server::HttpRequest;
using ::net::test_server::HttpResponse;

// The header that the server returns in a HTTP response to ask the client to
// authenticate.
constexpr char kAuthenticateResponseHeader[] = "WWW-Authenticate";

// The response header that the client sends to transfer HTTP auth credentials.
constexpr char kAuthorizationRequestHeader[] = "Authorization";

constexpr char kRelayState[] = "RelayState";

constexpr char kIdpDomain[] = "example.com";
constexpr char kIdPHost[] = "login.corp.example.com";
constexpr char kSamlLoginPath[] = "SAML";
constexpr char kSamlLoginAuthPath[] = "SAMLAuth";
constexpr char kSamlLoginWithDeviceAttestationPath[] =
    "SAML-with-device-attestation";
constexpr char kSamlLoginCheckDeviceAnswerPath[] = "SAML-check-device-answer";

// Must be equal to SAML_VERIFIED_ACCESS_CHALLENGE_HEADER from saml_handler.js.
constexpr char kSamlVerifiedAccessChallengeHeader[] =
    "x-verified-access-challenge";
// Must be equal to SAML_VERIFIED_ACCESS_RESPONSE_HEADER from saml_handler.js.
constexpr char kSamlVerifiedAccessResponseHeader[] =
    "x-verified-access-challenge-response";

constexpr char kTpmChallenge[] = {0,   1,   2,      'c',    'h',
                                  'a', 'l', '\xFD', '\xFE', '\xFF'};

std::string GetTpmChallenge() {
  return std::string(kTpmChallenge, base::size(kTpmChallenge));
}

std::string GetTpmChallengeBase64() {
  return base::Base64Encode(
      base::as_bytes(base::span<const char>(kTpmChallenge)));
}

std::string GetTpmResponse() {
  return AttestationClient::Get()
      ->GetTestInterface()
      ->GetEnterpriseChallengeFakeSignature(GetTpmChallenge(),
                                            /*include_spkac=*/false);
}

std::string GetTpmResponseBase64() {
  const std::string response = GetTpmResponse();
  return base::Base64Encode(base::as_bytes(base::make_span(response)));
}

// Returns relay state from http get/post requests.
std::string GetRelayState(const HttpRequest& request) {
  std::string relay_state;

  if (request.method == net::test_server::HttpMethod::METHOD_GET) {
    EXPECT_TRUE(net::GetValueForKeyInQuery(request.GetURL(), kRelayState,
                                           &relay_state));
  } else if (request.method == net::test_server::HttpMethod::METHOD_POST) {
    GURL query_url("http://localhost?" + request.content);
    EXPECT_TRUE(
        net::GetValueForKeyInQuery(query_url, kRelayState, &relay_state));
  } else {
    EXPECT_TRUE(false);  // gtest friendly implementation of NOTREACHED().
  }

  return relay_state;
}

}  // namespace

FakeSamlIdpMixin::FakeSamlIdpMixin(InProcessBrowserTestMixinHost* host,
                                   FakeGaiaMixin* gaia_mixin)
    : InProcessBrowserTestMixin(host), gaia_mixin_(gaia_mixin) {
  saml_server_.RegisterRequestHandler(base::BindRepeating(
      &FakeSamlIdpMixin::HandleRequest, base::Unretained(this)));
  saml_http_server_.RegisterRequestHandler(base::BindRepeating(
      &FakeSamlIdpMixin::HandleRequest, base::Unretained(this)));
}

FakeSamlIdpMixin::~FakeSamlIdpMixin() = default;

void FakeSamlIdpMixin::SetUpCommandLine(base::CommandLine* command_line) {
  base::FilePath test_data_dir;
  ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_data_dir));
  // NOTE: Ideally testdata would all be in chromeos/login, to match the test.
  html_template_dir_ = test_data_dir.Append("login");
  saml_response_dir_ = test_data_dir.Append("chromeos").Append("login");

  ASSERT_TRUE(saml_server_.Start());
  ASSERT_TRUE(saml_http_server_.Start());
}

void FakeSamlIdpMixin::SetUpOnMainThread() {
  gaia_mixin_->fake_gaia()->RegisterSamlDomainRedirectUrl(GetIdpDomain(),
                                                          GetSamlPageUrl());
}

void FakeSamlIdpMixin::SetLoginHTMLTemplate(const std::string& template_file) {
  base::ScopedAllowBlockingForTesting allow_io;
  EXPECT_TRUE(base::ReadFileToString(html_template_dir_.Append(template_file),
                                     &login_html_template_));
}

void FakeSamlIdpMixin::SetLoginAuthHTMLTemplate(
    const std::string& template_file) {
  base::ScopedAllowBlockingForTesting allow_io;
  EXPECT_TRUE(base::ReadFileToString(html_template_dir_.Append(template_file),
                                     &login_auth_html_template_));
}

void FakeSamlIdpMixin::SetRefreshURL(const GURL& refresh_url) {
  refresh_url_ = refresh_url;
}

void FakeSamlIdpMixin::SetCookieValue(const std::string& cookie_value) {
  cookie_value_ = cookie_value;
}

void FakeSamlIdpMixin::SetRequireHttpBasicAuth(bool require_http_basic_auth) {
  require_http_basic_auth_ = require_http_basic_auth;
}

void FakeSamlIdpMixin::SetSamlResponseFile(const std::string& xml_file) {
  base::ScopedAllowBlockingForTesting allow_io;
  EXPECT_TRUE(base::ReadFileToString(saml_response_dir_.Append(xml_file),
                                     &saml_response_));
  base::Base64Encode(saml_response_, &saml_response_);
}

bool FakeSamlIdpMixin::IsLastChallengeResponseExists() const {
  return challenge_response_.has_value();
}

void FakeSamlIdpMixin::AssertChallengeResponseMatchesTpmResponse() const {
  ASSERT_EQ(challenge_response_.value(), GetTpmResponseBase64());
}

std::string FakeSamlIdpMixin::GetIdpHost() const {
  return kIdPHost;
}

std::string FakeSamlIdpMixin::GetIdpDomain() const {
  return kIdpDomain;
}

GURL FakeSamlIdpMixin::GetSamlPageUrl() const {
  return saml_server_.GetURL(kIdPHost, std::string("/") + kSamlLoginPath);
}

GURL FakeSamlIdpMixin::GetHttpSamlPageUrl() const {
  return saml_http_server_.GetURL(kIdPHost, std::string("/") + kSamlLoginPath);
}

GURL FakeSamlIdpMixin::GetSamlWithDeviceAttestationUrl() const {
  return saml_server_.GetURL(
      kIdPHost, std::string("/") + kSamlLoginWithDeviceAttestationPath);
}

GURL FakeSamlIdpMixin::GetSamlAuthPageUrl() const {
  return saml_server_.GetURL(kIdPHost, std::string("/") + kSamlLoginAuthPath);
}

GURL FakeSamlIdpMixin::GetSamlWithCheckDeviceAnswerUrl() const {
  return saml_server_.GetURL(
      kIdPHost, std::string("/") + kSamlLoginCheckDeviceAnswerPath);
}

std::unique_ptr<net::test_server::HttpResponse> FakeSamlIdpMixin::HandleRequest(
    const net::test_server::HttpRequest& request) {
  GURL request_url = request.GetURL();
  const RequestType request_type = ParseRequestTypeFromRequestPath(request_url);

  if (request_type == RequestType::kUnknown) {
    // Ignore this request.
    return nullptr;
  }

  // For HTTP Basic Auth, we don't care to check the credentials, just
  // if some credentials were provided. If not, respond with an authentication
  // request that should make the browser pop up a credentials entry UI.
  if (require_http_basic_auth_ &&
      !base::Contains(request.headers, kAuthorizationRequestHeader)) {
    auto http_response =
        std::make_unique<net::test_server::BasicHttpResponse>();
    http_response->set_code(net::HTTP_UNAUTHORIZED);
    http_response->AddCustomHeader(kAuthenticateResponseHeader,
                                   "Basic realm=\"test realm\"");
    return http_response;
  }

  switch (request_type) {
    case RequestType::kLogin:
      return BuildResponseForLogin(request, request_url);
    case RequestType::kLoginAuth:
      return BuildResponseForLoginAuth(request, request_url);
    case RequestType::kLoginWithDeviceAttestation:
      return BuildResponseForLoginWithDeviceAttestation(request, request_url);
    case RequestType::kLoginCheckDeviceAnswer:
      return BuildResponseForCheckDeviceAnswer(request, request_url);
    case RequestType::kUnknown:
      NOTREACHED();
      return nullptr;
  }
}

FakeSamlIdpMixin::RequestType FakeSamlIdpMixin::ParseRequestTypeFromRequestPath(
    const GURL& request_url) const {
  std::string request_path = request_url.path();

  if (request_path == GetSamlPageUrl().path())
    return RequestType::kLogin;
  if (request_path == GetSamlAuthPageUrl().path())
    return RequestType::kLoginAuth;
  if (request_path == GetSamlWithDeviceAttestationUrl().path())
    return RequestType::kLoginWithDeviceAttestation;
  if (request_path == GetSamlWithCheckDeviceAnswerUrl().path())
    return RequestType::kLoginCheckDeviceAnswer;

  return RequestType::kUnknown;
}

std::unique_ptr<HttpResponse> FakeSamlIdpMixin::BuildResponseForLogin(
    const HttpRequest& request,
    const GURL& request_url) const {
  const std::string relay_state = GetRelayState(request);
  return BuildHTMLResponse(login_html_template_, relay_state,
                           GetSamlAuthPageUrl().path());
}

std::unique_ptr<HttpResponse> FakeSamlIdpMixin::BuildResponseForLoginAuth(
    const HttpRequest& request,
    const GURL& request_url) {
  const std::string relay_state = GetRelayState(request);
  GURL redirect_url = gaia_mixin_->GetFakeGaiaURL("/SSO");

  if (!login_auth_html_template_.empty()) {
    return BuildHTMLResponse(login_auth_html_template_, relay_state,
                             redirect_url.spec());
  }

  redirect_url =
      net::AppendQueryParameter(redirect_url, "SAMLResponse", saml_response_);
  redirect_url =
      net::AppendQueryParameter(redirect_url, kRelayState, relay_state);

  auto http_response = std::make_unique<BasicHttpResponse>();
  http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
  http_response->AddCustomHeader("Location", redirect_url.spec());
  http_response->AddCustomHeader(
      "Set-cookie", base::StringPrintf("saml=%s", cookie_value_.c_str()));
  return http_response;
}

std::unique_ptr<HttpResponse>
FakeSamlIdpMixin::BuildResponseForLoginWithDeviceAttestation(
    const HttpRequest& request,
    const GURL& request_url) const {
  std::string relay_state = GetRelayState(request);

  GURL redirect_url = GetSamlWithCheckDeviceAnswerUrl();
  redirect_url =
      net::AppendQueryParameter(redirect_url, kRelayState, relay_state);

  auto http_response = std::make_unique<BasicHttpResponse>();
  http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
  http_response->AddCustomHeader("Location", redirect_url.spec());
  http_response->AddCustomHeader(kSamlVerifiedAccessChallengeHeader,
                                 GetTpmChallengeBase64());

  return http_response;
}

std::unique_ptr<HttpResponse>
FakeSamlIdpMixin::BuildResponseForCheckDeviceAnswer(const HttpRequest& request,
                                                    const GURL& request_url) {
  std::string relay_state = GetRelayState(request);

  auto iter = request.headers.find(kSamlVerifiedAccessResponseHeader);
  if (iter != request.headers.end()) {
    SaveChallengeResponse(/*challenge_response=*/iter->second);
  } else {
    ClearChallengeResponse();
  }

  GURL redirect_url =
      net::AppendQueryParameter(GetSamlPageUrl(), kRelayState, relay_state);

  auto http_response = std::make_unique<BasicHttpResponse>();
  http_response->set_code(net::HTTP_TEMPORARY_REDIRECT);
  http_response->AddCustomHeader("Location", redirect_url.spec());
  return http_response;
}

std::unique_ptr<net::test_server::HttpResponse>
FakeSamlIdpMixin::BuildHTMLResponse(const std::string& html_template,
                                    const std::string& relay_state,
                                    const std::string& next_path) const {
  std::string response_html = html_template;
  base::ReplaceSubstringsAfterOffset(&response_html, 0, "$RelayState",
                                     relay_state);
  base::ReplaceSubstringsAfterOffset(&response_html, 0, "$Post", next_path);
  base::ReplaceSubstringsAfterOffset(&response_html, 0, "$Refresh",
                                     refresh_url_.spec());

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content(response_html);
  http_response->set_content_type("text/html");

  return http_response;
}

void FakeSamlIdpMixin::SaveChallengeResponse(const std::string& response) {
  EXPECT_EQ(challenge_response_, absl::nullopt);
  challenge_response_ = response;
}

void FakeSamlIdpMixin::ClearChallengeResponse() {
  challenge_response_.reset();
}

}  // namespace ash
