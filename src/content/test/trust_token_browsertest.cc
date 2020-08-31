// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/base64.h"
#include "base/strings/string_piece.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_monitor.h"
#include "content/shell/browser/shell.h"
#include "net/base/escape.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/trust_tokens/test/test_server_handler_registration.h"
#include "services/network/trust_tokens/test/trust_token_request_handler.h"
#include "services/network/trust_tokens/test/trust_token_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using network::test::TrustTokenRequestHandler;
using ::testing::Field;
using ::testing::HasSubstr;
using ::testing::IsFalse;
using ::testing::Optional;

// TrustTokenBrowsertest is a fixture containing boilerplate for initializing an
// HTTPS test server and passing requests through to an embedded instance of
// network::test::TrustTokenRequestHandler, which contains the guts of the
// "server-side" token issuance and redemption logic as well as some consistency
// checks for subsequent signed requests.
class TrustTokenBrowsertest : public ContentBrowserTest {
 public:
  TrustTokenBrowsertest() {
    auto& field_trial_param =
        network::features::kTrustTokenOperationsRequiringOriginTrial;
    features_.InitAndEnableFeatureWithParameters(
        network::features::kTrustTokens,
        {{field_trial_param.name,
          field_trial_param.GetName(
              network::features::TrustTokenOriginTrialSpec::
                  kOriginTrialNotRequired)}});
  }

  // Registers the following handlers:
  // - default //content/test/data files;
  // - a special "/issue" endpoint executing Trust Tokens issuance;
  // - a special "/redeem" endpoint executing redemption; and
  // - a special "/sign" endpoint that verifies that the received signed request
  // data is correctly structured and that the provided Sec-Signature header's
  // verification key was previously bound to a successful token redemption.
  void SetUpOnMainThread() override {
    server_.AddDefaultHandlers(
        base::FilePath(FILE_PATH_LITERAL("content/test/data")));

    host_resolver()->AddRule("*", "127.0.0.1");
    SetupCrossSiteRedirector(embedded_test_server());
    SetupCrossSiteRedirector(&server_);

    network::test::RegisterTrustTokenTestHandlers(&server_, &request_handler_);

    ASSERT_TRUE(server_.Start());
  }

 protected:
  base::test::ScopedFeatureList features_;

  // TODO(davidvc): Extend this to support more than one key set.
  TrustTokenRequestHandler request_handler_;

  net::EmbeddedTestServer server_{net::EmbeddedTestServer::TYPE_HTTPS};
};

}  // namespace

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, FetchEndToEnd) {
  base::RunLoop run_loop;
  GetNetworkService()->SetTrustTokenKeyCommitments(
      network::WrapKeyCommitmentForIssuer(
          url::Origin::Create(server_.base_url()),
          request_handler_.GetKeyCommitmentRecord()),
      run_loop.QuitClosure());
  run_loop.Run();

  GURL start_url(server_.GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  std::string cmd = R"(
  (async () => {
    await fetch("/issue", {trustToken: {type: 'token-request'}});
    await fetch("/redeem", {trustToken: {type: 'srr-token-redemption'}});
    await fetch("/sign", {trustToken: {type: 'send-srr',
                                  signRequestData: 'include',
                                  issuer: $1}}); })(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ(
      EvalJs(
          shell(),
          JsReplace(cmd, url::Origin::Create(server_.base_url()).Serialize()))
          .error,
      "");

  EXPECT_EQ(request_handler_.LastVerificationError(), base::nullopt);
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, XhrEndToEnd) {
  base::RunLoop run_loop;
  GetNetworkService()->SetTrustTokenKeyCommitments(
      network::WrapKeyCommitmentForIssuer(
          url::Origin::Create(server_.base_url()),
          request_handler_.GetKeyCommitmentRecord()),
      run_loop.QuitClosure());
  run_loop.Run();

  GURL start_url(server_.GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  // If this isn't idiomatic JS, I don't know what is.
  std::string cmd = R"(
  (async () => {
    let request = new XMLHttpRequest();
    request.open('GET', '/issue');
    request.setTrustToken({
      type: 'token-request'
    });
    let promise = new Promise((res, rej) => {
      request.onload = res; request.onerror = rej;
    });
    request.send();
    await promise;

    request = new XMLHttpRequest();
    request.open('GET', '/redeem');
    request.setTrustToken({
      type: 'srr-token-redemption'
    });
    promise = new Promise((res, rej) => {
      request.onload = res; request.onerror = rej;
    });
    request.send();
    await promise;

    request = new XMLHttpRequest();
    request.open('GET', '/sign');
    request.setTrustToken({
      type: 'send-srr',
      signRequestData: 'include',
      issuer: $1
    });
    promise = new Promise((res, rej) => {
      request.onload = res; request.onerror = rej;
    });
    request.send();
    await promise;
    })(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ(
      EvalJs(
          shell(),
          JsReplace(cmd, url::Origin::Create(server_.base_url()).Serialize()))
          .error,
      "");

  EXPECT_EQ(request_handler_.LastVerificationError(), base::nullopt);
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, IframeEndToEnd) {
  base::RunLoop run_loop;
  GetNetworkService()->SetTrustTokenKeyCommitments(
      network::WrapKeyCommitmentForIssuer(
          url::Origin::Create(server_.base_url()),
          request_handler_.GetKeyCommitmentRecord()),
      run_loop.QuitClosure());
  run_loop.Run();

  GURL start_url(server_.GetURL("/page_with_iframe.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  auto execute_op_via_iframe = [&](base::StringPiece path,
                                   base::StringPiece trust_token) {
    // It's important to set the trust token arguments before updating src, as
    // the latter triggers a load.
    EXPECT_TRUE(ExecJs(
        shell(), JsReplace(
                     R"( const myFrame = document.getElementById("test_iframe");
                         myFrame.trustToken = $1;
                         myFrame.src = $2;)",
                     trust_token, path)));
    TestNavigationObserver load_observer(shell()->web_contents());
    load_observer.WaitForNavigationFinished();
  };

  execute_op_via_iframe("/issue", R"({"type": "token-request"})");
  execute_op_via_iframe("/redeem", R"({"type": "srr-token-redemption"})");
  execute_op_via_iframe(
      "/sign",
      JsReplace(
          R"({"type": "send-srr", "signRequestData": "include", "issuer": $1})",
          url::Origin::Create(server_.base_url()).Serialize()));
  EXPECT_EQ(request_handler_.LastVerificationError(), base::nullopt);
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, HasTrustTokenAfterIssuance) {
  base::RunLoop run_loop;
  GetNetworkService()->SetTrustTokenKeyCommitments(
      network::WrapKeyCommitmentForIssuer(
          url::Origin::Create(server_.base_url()),
          request_handler_.GetKeyCommitmentRecord()),
      run_loop.QuitClosure());
  run_loop.Run();

  GURL start_url(server_.GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  std::string cmd =
      JsReplace(R"(
  (async () => {
    await fetch("/issue", {trustToken: {type: 'token-request'}});
    return await document.hasTrustToken($1);
  })();)",
                url::Origin::Create(server_.base_url()).Serialize());

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  //
  // Note: EvalJs's EXPECT_EQ type-conversion magic only supports the
  // "Yoda-style" EXPECT_EQ(expected, actual).
  EXPECT_EQ(true, EvalJs(shell(), cmd));
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest,
                       SigningWithNoRedemptionRecordDoesntCancelRequest) {
  TrustTokenRequestHandler::Options options;
  options.client_signing_outcome =
      TrustTokenRequestHandler::SigningOutcome::kFailure;
  request_handler_.UpdateOptions(std::move(options));

  GURL start_url(server_.GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  // This sign operation will fail, because we don't have a signed redemption
  // record in storage, a prerequisite. However, the failure shouldn't be fatal.
  std::string cmd =
      JsReplace(R"((async () => {
      await fetch("/sign", {trustToken: {type: 'send-srr',
                                         signRequestData: 'include',
                                         issuer: $1}}); }
                                         )(); )",
                url::Origin::Create(server_.base_url()).Serialize());

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ(EvalJs(shell(), cmd).error, "");
  EXPECT_EQ(request_handler_.LastVerificationError(), base::nullopt);
}

IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, RecordsTimers) {
  base::HistogramTester histograms;

  base::RunLoop run_loop;
  GetNetworkService()->SetTrustTokenKeyCommitments(
      network::WrapKeyCommitmentForIssuer(
          url::Origin::Create(server_.base_url()),
          request_handler_.GetKeyCommitmentRecord()),
      run_loop.QuitClosure());
  run_loop.Run();

  GURL start_url(server_.GetURL("/title1.html"));
  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  std::string cmd = R"(
  (async () => {
    await fetch("/issue", {trustToken: {type: 'token-request'}});
    await fetch("/redeem", {trustToken: {type: 'srr-token-redemption'}});
    await fetch("/sign", {trustToken: {type: 'send-srr',
                                  signRequestData: 'include',
                                  issuer: $1}}); })(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ(
      EvalJs(
          shell(),
          JsReplace(cmd, url::Origin::Create(server_.base_url()).Serialize()))
          .error,
      "");

  // Just check that the timers were populated: since we can't mock a clock in
  // this browser test, it's hard to check the recorded values for
  // reasonableness.
  content::FetchHistogramsFromChildProcesses();
  for (const std::string& op : {"Issuance", "Redemption", "Signing"}) {
    histograms.ExpectTotalCount(
        "Net.TrustTokens.OperationBeginTime.Success." + op, 1);
    histograms.ExpectTotalCount(
        "Net.TrustTokens.OperationTotalTime.Success." + op, 1);
    histograms.ExpectTotalCount(
        "Net.TrustTokens.OperationServerTime.Success." + op, 1);
    histograms.ExpectTotalCount(
        "Net.TrustTokens.OperationFinalizeTime.Success." + op, 1);
  }
}

// Trust Tokens should require that their executing contexts be secure.
IN_PROC_BROWSER_TEST_F(TrustTokenBrowsertest, OperationsRequireSecureContext) {
  ASSERT_TRUE(embedded_test_server()->Start());

  GURL start_url(
      embedded_test_server()->GetURL("a.com", "/page_with_iframe.html"));
  // Make sure that we are, in fact, using an insecure page.
  ASSERT_FALSE(network::IsUrlPotentiallyTrustworthy(start_url));

  EXPECT_TRUE(NavigateToURL(shell(), start_url));

  // 1. Confirm that the Fetch interface doesn't work:
  std::string cmd = R"(fetch($1, {trustToken: {type: 'token-request'}});)";
  EXPECT_THAT(EvalJs(shell(), JsReplace(cmd, server_.GetURL("/issue"))).error,
              HasSubstr("secure context"));

  // 2. Confirm that the XHR interface isn't present:
  EXPECT_EQ(false, EvalJs(shell(), "'setTrustToken' in (new XMLHttpRequest);"));

  // 3. Confirm that the iframe interface doesn't work by verifying that no
  // Trust Tokens operation gets executed.
  GURL issuance_url = server_.GetURL("/issue");
  URLLoaderMonitor monitor({issuance_url});
  // It's important to set the trust token arguments before updating src, as
  // the latter triggers a load.
  EXPECT_TRUE(ExecJs(
      shell(), JsReplace(
                   R"( const myFrame = document.getElementById("test_iframe");
                       myFrame.trustToken = $1;
                       myFrame.src = $2;)",
                   R"({"type": "token-request"})", issuance_url)));
  monitor.WaitForUrls();
  EXPECT_THAT(monitor.GetRequestInfo(issuance_url),
              Optional(Field(&network::ResourceRequest::trust_token_params,
                             IsFalse())));
}

}  // namespace content
