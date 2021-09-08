// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "content/browser/devtools/protocol/devtools_protocol_test_support.h"
#include "content/browser/devtools/protocol/network.h"
#include "content/browser/net/trust_token_browsertest.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/shell/browser/shell.h"
#include "services/network/public/cpp/features.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace content {

class DevToolsTrustTokenBrowsertest : public DevToolsProtocolTest,
                                      public TrustTokenBrowsertest {
 public:
  void SetUpOnMainThread() override {
    TrustTokenBrowsertest::SetUpOnMainThread();
    DevToolsProtocolTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    TrustTokenBrowsertest::TearDownOnMainThread();
    DevToolsProtocolTest::TearDownOnMainThread();
  }

  // The returned view is only valid until the next |SendCommand| call.
  base::Value::ListView GetTrustTokensViaProtocol() {
    SendCommand("Storage.getTrustTokens", nullptr);
    base::Value* tokens = result_->FindPath("tokens");
    EXPECT_TRUE(tokens);
    return tokens->GetList();
  }

  // Asserts that CDP reports |count| number of tokens for |issuerOrigin|.
  void AssertTrustTokensViaProtocol(const std::string& issuerOrigin,
                                    int expectedCount) {
    auto tokens = GetTrustTokensViaProtocol();
    EXPECT_GT(tokens.size(), 0ul);

    for (const auto& token : tokens) {
      const std::string* issuer = token.FindStringKey("issuerOrigin");
      if (*issuer == issuerOrigin) {
        const absl::optional<int> actualCount = token.FindIntPath("count");
        EXPECT_THAT(actualCount, ::testing::Optional(expectedCount));
        return;
      }
    }
    FAIL() << "No trust tokens for issuer " << issuerOrigin;
  }
};

// After a successful issuance and redemption, a subsequent redemption against
// the same issuer should hit the redemption record (RR) cache.
IN_PROC_BROWSER_TEST_F(DevToolsTrustTokenBrowsertest,
                       RedemptionRecordCacheHitIsReportedAsLoadingFinished) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  // 1. Navigate to a test site, request and redeem a trust token.
  ASSERT_TRUE(NavigateToURL(shell(), server_.GetURL("a.test", "/title1.html")));

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { trustToken: { type: 'token-request' } })
        .then(()=>'Success'); )",
                                      server_.GetURL("a.test", "/issue"))));

  EXPECT_EQ("Success",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { trustToken: { type: 'token-redemption' } })
        .then(()=>'Success'); )",
                                      server_.GetURL("a.test", "/redeem"))));

  // 2) Open DevTools and enable Network domain.
  Attach();
  SendCommand("Network.enable", std::make_unique<base::DictionaryValue>());

  // Make sure there are no existing DevTools events in the queue.
  EXPECT_EQ(notifications_.size(), 0ul);

  // 3) Issue another redemption, and verify its served from cache.
  EXPECT_EQ("NoModificationAllowedError",
            EvalJs(shell(), JsReplace(R"(fetch($1,
        { trustToken: { type: 'token-redemption' } })
        .catch(err => err.name); )",
                                      server_.GetURL("a.test", "/redeem"))));

  // 4) Verify the request is marked as successful and not as failed.
  WaitForNotification("Network.requestServedFromCache", true);
  WaitForNotification("Network.loadingFinished", true);
  WaitForNotification("Network.trustTokenOperationDone", true);
}

class DevToolsTrustTokenBrowsertestWithPlatformIssuance
    : public DevToolsTrustTokenBrowsertest {
 public:
  DevToolsTrustTokenBrowsertestWithPlatformIssuance() {
    // This assertion helps guard against the brittleness of deserializing
    // "true", in case we refactor the parameter's type.
    static_assert(
        std::is_same<decltype(
                         network::features::kPlatformProvidedTrustTokenIssuance
                             .default_value),
                     const bool>::value,
        "Need to update this initialization logic if the type of the param "
        "changes.");
    features_.InitAndEnableFeatureWithParameters(
        network::features::kTrustTokens,
        {{network::features::kPlatformProvidedTrustTokenIssuance.name,
          "true"}});
  }

 private:
  base::test::ScopedFeatureList features_;
};

#if defined(OS_ANDROID)
// After a successful platform-provided issuance operation (which involves an
// IPC to a system-local provider, not an HTTP request to a server), the
// request's outcome should show as a cache hit in the network panel.
IN_PROC_BROWSER_TEST_F(
    DevToolsTrustTokenBrowsertestWithPlatformIssuance,
    SuccessfulPlatformProvidedIssuanceIsReportedAsLoadingFinished) {
  TrustTokenRequestHandler::Options options;
  options.specify_platform_issuance_on = {
      network::mojom::TrustTokenKeyCommitmentResult::Os::kAndroid};
  request_handler_.UpdateOptions(std::move(options));

  HandlerWrappingLocalTrustTokenFulfiller fulfiller(request_handler_);

  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  // Open DevTools and enable Network domain.
  Attach();
  SendCommand("Network.enable", std::make_unique<base::DictionaryValue>());

  // Make sure there are no existing DevTools events in the queue.
  EXPECT_EQ(notifications_.size(), 0ul);

  // Issuance operations successfully answered locally result in
  // NoModificationAllowedError.
  std::string command = R"(
  (async () => {
    try {
      await fetch("/issue", {trustToken: {type: 'token-request'}});
      return "Unexpected success";
    } catch (e) {
      if (e.name !== "NoModificationAllowedError") {
        return "Unexpected exception";
      }
      const hasToken = await document.hasTrustToken($1);
      if (!hasToken)
        return "Unexpectedly absent token";
      return "Success";
    }})(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ(
      "Success",
      EvalJs(shell(), JsReplace(command, IssuanceOriginFromHost("a.test"))));

  // Verify the request is marked as successful and not as failed.
  WaitForNotification("Network.requestServedFromCache", true);
  WaitForNotification("Network.loadingFinished", true);
  WaitForNotification("Network.trustTokenOperationDone", true);
}
#endif  // defined(OS_ANDROID)

namespace {

bool MatchStatus(const std::string& expected_status,
                 base::DictionaryValue* params) {
  std::string actual_status;
  EXPECT_TRUE(params->GetString("status", &actual_status));
  return expected_status == actual_status;
}

base::RepeatingCallback<bool(base::DictionaryValue*)> okStatusMatcher =
    base::BindRepeating(
        &MatchStatus,
        protocol::Network::TrustTokenOperationDone::StatusEnum::Ok);

}  // namespace

IN_PROC_BROWSER_TEST_F(DevToolsTrustTokenBrowsertest, FetchEndToEnd) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  // 1) Navigate to a test site.
  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  // 2) Open DevTools and enable Network domain.
  Attach();
  SendCommand("Network.enable", std::make_unique<base::DictionaryValue>());

  // 3) Request and redeem a token, then use the redeemed token in a Signing
  // request.
  std::string command = R"(
  (async () => {
    await fetch('/issue', {trustToken: {type: 'token-request'}});
    await fetch('/redeem', {trustToken: {type: 'token-redemption'}});
    await fetch('/sign', {trustToken: {type: 'send-redemption-record',
                                  signRequestData: 'include',
                                  issuers: [$1]}});
    return 'Success'; })(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ(
      "Success",
      EvalJs(shell(), JsReplace(command, IssuanceOriginFromHost("a.test"))));

  // 4) Verify that we received three successful events.
  WaitForMatchingNotification("Network.trustTokenOperationDone",
                              okStatusMatcher);
  WaitForMatchingNotification("Network.trustTokenOperationDone",
                              okStatusMatcher);
  WaitForMatchingNotification("Network.trustTokenOperationDone",
                              okStatusMatcher);
}

IN_PROC_BROWSER_TEST_F(DevToolsTrustTokenBrowsertest, IframeEndToEnd) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  // 1) Navigate to a test site.
  GURL start_url = server_.GetURL("a.test", "/page_with_iframe.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  // 2) Open DevTools and enable Network domain.
  Attach();
  SendCommand("Network.enable", std::make_unique<base::DictionaryValue>());

  // 3) Request and redeem a token, then use the redeemed token in a Signing
  // request.
  auto execute_op_via_iframe = [&](base::StringPiece path,
                                   base::StringPiece trust_token) {
    // It's important to set the trust token arguments before updating src, as
    // the latter triggers a load.
    EXPECT_TRUE(ExecJs(
        shell(), JsReplace(
                     R"( const myFrame = document.getElementById('test_iframe');
                         myFrame.trustToken = $1;
                         myFrame.src = $2;)",
                     trust_token, path)));
    TestNavigationObserver load_observer(shell()->web_contents());
    load_observer.WaitForNavigationFinished();
  };

  execute_op_via_iframe("/issue", R"({"type": "token-request"})");
  execute_op_via_iframe("/redeem", R"({"type": "token-redemption"})");
  execute_op_via_iframe("/sign", JsReplace(
                                     R"({"type": "send-redemption-record",
              "signRequestData": "include", "issuers": [$1]})",
                                     IssuanceOriginFromHost("a.test")));

  // 4) Verify that we received three successful events.
  WaitForMatchingNotification("Network.trustTokenOperationDone",
                              okStatusMatcher);
  WaitForMatchingNotification("Network.trustTokenOperationDone",
                              okStatusMatcher);
  WaitForMatchingNotification("Network.trustTokenOperationDone",
                              okStatusMatcher);
}

// When the server rejects issuance, DevTools gets a failed notification.
IN_PROC_BROWSER_TEST_F(DevToolsTrustTokenBrowsertest,
                       FailedIssuanceFiresFailedOperationEvent) {
  TrustTokenRequestHandler::Options options;
  options.issuance_outcome =
      TrustTokenRequestHandler::ServerOperationOutcome::kUnconditionalFailure;
  request_handler_.UpdateOptions(std::move(options));

  // 1) Navigate to a test site.
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  // 2) Open DevTools and enable Network domain.
  Attach();
  SendCommand("Network.enable", std::make_unique<base::DictionaryValue>());

  // 3) Request some Trust Tokens.
  EXPECT_EQ("OperationError", EvalJs(shell(), R"(fetch('/issue',
        { trustToken: { type: 'token-request' } })
        .then(()=>'Success').catch(err => err.name); )"));

  // 4) Verify that we received an Trust Token operation failed event.
  WaitForMatchingNotification(
      "Network.trustTokenOperationDone",
      base::BindRepeating(
          &MatchStatus,
          protocol::Network::TrustTokenOperationDone::StatusEnum::BadResponse));
}

IN_PROC_BROWSER_TEST_F(DevToolsTrustTokenBrowsertest, GetTrustTokens) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  // 1) Navigate to a test site.
  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  // 2) Open DevTools.
  Attach();

  // 3) Call Storage.getTrustTokens and expect none to be there.
  EXPECT_EQ(GetTrustTokensViaProtocol().size(), 0ul);

  // 4) Request some Trust Tokens.
  std::string command = R"(
  (async () => {
    await fetch('/issue', {trustToken: {type: 'token-request'}});
    return 'Success'; })(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ("Success", EvalJs(shell(), command));

  // 5) Call Storage.getTrustTokens and expect a Trust Token to be there.
  EXPECT_EQ(GetTrustTokensViaProtocol().size(), 1ul);
}

IN_PROC_BROWSER_TEST_F(DevToolsTrustTokenBrowsertest, ClearTrustTokens) {
  ProvideRequestHandlerKeyCommitmentsToNetworkService({"a.test"});

  // 1) Navigate to a test site.
  GURL start_url = server_.GetURL("a.test", "/title1.html");
  ASSERT_TRUE(NavigateToURL(shell(), start_url));

  // 2) Open DevTools.
  Attach();

  // 3) Request some Trust Tokens.
  std::string command = R"(
  (async () => {
    await fetch('/issue', {trustToken: {type: 'token-request'}});
    return 'Success'; })(); )";

  // We use EvalJs here, not ExecJs, because EvalJs waits for promises to
  // resolve.
  EXPECT_EQ("Success", EvalJs(shell(), command));

  // 4) Call Storage.getTrustTokens and expect a Trust Token to be there.
  AssertTrustTokensViaProtocol(IssuanceOriginFromHost("a.test"), 10);

  // 5) Call Storage.clearTrustTokens
  auto params = std::make_unique<base::DictionaryValue>();
  params->SetStringPath("issuerOrigin", IssuanceOriginFromHost("a.test"));
  auto* result = SendCommand("Storage.clearTrustTokens", std::move(params));

  EXPECT_THAT(result->FindBoolPath("didDeleteTokens"),
              ::testing::Optional(true));

  // 6) Call Storage.getTrustTokens and expect no Trust Tokens to be there.
  //    Note that we still get an entry for our 'issuerOrigin', but the actual
  //    Token count must be 0.
  AssertTrustTokensViaProtocol(IssuanceOriginFromHost("a.test"), 0);
}

}  // namespace content
