// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/seller_worklet.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom.h"
#include "content/services/auction_worklet/worklet_devtools_debug_test_util.h"
#include "content/services/auction_worklet/worklet_test_util.h"
#include "content/services/auction_worklet/worklet_v8_debug_test_util.h"
#include "mojo/public/cpp/bindings/unique_receiver_set.h"
#include "net/http/http_status_code.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/interest_group/interest_group_types.mojom.h"
#include "url/gurl.h"

using testing::HasSubstr;
using testing::StartsWith;

namespace auction_worklet {
namespace {

// Very short time used by some tests that want to wait until just before a
// timer triggers.
constexpr base::TimeDelta kTinyTime = base::Microseconds(1);

// Common trusted scoring signals response.
const char kTrustedScoringSignalsResponse[] = R"(
  {
    "renderUrls": {"https://render.url.test/": 4},
    "adComponentRenderUrls": {
      "https://component1.test/": 1,
      "https://component2.test/": 2
    }
  }
)";

// Creates seller a script with scoreAd() returning the specified expression.
// Allows using scoreAd() arguments, arbitrary values, incorrect types, etc.
std::string CreateScoreAdScript(const std::string& raw_return_value) {
  constexpr char kSellAdScript[] = R"(
    function scoreAd(adMetadata, bid, auctionConfig, trustedScoringSignals,
        browserSignals) {
      return %s;
    }
  )";
  return base::StringPrintf(kSellAdScript, raw_return_value.c_str());
}

// Returns a working script, primarily for testing failure cases where it
// should not be run.
std::string CreateBasicSellAdScript() {
  return CreateScoreAdScript("1");
}

// Creates a seller script with report_result() returning the specified
// expression. If |extra_code| is non-empty, it will be added as an additional
// line above the return value. Intended for sendReportTo() calls. In practice,
// these scripts will always include a scoreAd() method, but few tests check
// both methods.
std::string CreateReportToScript(const std::string& raw_return_value,
                                 const std::string& extra_code) {
  constexpr char kBasicSellerScript[] = R"(
    function reportResult(auctionConfig, browserSignals) {
      %s;
      return %s;
    }
  )";
  return CreateBasicSellAdScript() +
         base::StringPrintf(kBasicSellerScript, extra_code.c_str(),
                            raw_return_value.c_str());
}

class SellerWorkletTest : public testing::Test {
 public:
  SellerWorkletTest() {
    SetDefaultParameters();
    v8_helper_ = AuctionV8Helper::Create(AuctionV8Helper::CreateTaskRunner());
  }

  ~SellerWorkletTest() override {
    // Release the V8 helper and process all pending tasks. This is to make sure
    // there aren't any pending tasks between the V8 thread and the main thread
    // that will result in UAFs. These lines are not necessary for any test to
    // pass.
    v8_helper_.reset();
    task_environment_.RunUntilIdle();

    // In all tests where the SellerWorklet receiver is closed before the
    // remote, the disconnect reason should be consumed and validated.
    EXPECT_FALSE(disconnect_reason_);
  }

  // Sets default values for scoreAd() and report_result() arguments. No test
  // actually depends on these being anything but valid, but this does allow
  // tests to reset them to a consistent state.
  void SetDefaultParameters() {
    ad_metadata_ = "[1]";
    bid_ = 1;
    decision_logic_url_ = GURL("https://url.test/");
    trusted_scoring_signals_url_.reset();
    auction_ad_config_non_shared_params_ =
        blink::mojom::AuctionAdConfigNonSharedParams::New();

    top_window_origin_ = url::Origin::Create(GURL("https://window.test/"));
    browser_signal_interest_group_owner_ =
        url::Origin::Create(GURL("https://interest.group.owner.test/"));
    browser_signal_render_url_ = GURL("https://render.url.test/");
    browser_signal_ad_components_.clear();
    browser_signal_bidding_duration_msecs_ = 0;
    browser_signal_desireability_ = 1;
  }

  // Configures `url_loader_factory_` to return a script with the specified
  // return line, expecting the provided result.
  void RunScoreAdWithReturnValueExpectingResult(
      const std::string& raw_return_value,
      double expected_score,
      const std::vector<std::string>& expected_errors =
          std::vector<std::string>()) {
    RunScoreAdWithJavascriptExpectingResult(
        CreateScoreAdScript(raw_return_value), expected_score, expected_errors);
  }

  // Behaves just like RunScoreAdWithReturnValueExpectingResult(), but
  // additionally expects the auction to take exactly `expected_duration`, using
  // FastForwardBy() to advance time. Can't just use RunLoop and Time::Now()
  // time, because that can get confused by superfluous events and wait 30
  // seconds too long (perhaps confused by the 30 second download timer, even
  // though the download should complete immediately, and the URLLoader with the
  // timer on it deleted?)
  void RunScoreAdWithReturnValueExpectingResultInExactTime(
      const std::string& raw_return_value,
      double expected_score,
      base::TimeDelta expected_duration,
      const std::vector<std::string>& expected_errors = {}) {
    AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                          CreateScoreAdScript(raw_return_value));
    auto seller_worklet = CreateWorklet();

    base::RunLoop run_loop;
    RunScoreAdOnWorkletAsync(seller_worklet.get(), expected_score,
                             expected_errors, run_loop.QuitClosure());
    task_environment_.FastForwardBy(expected_duration - kTinyTime);
    EXPECT_FALSE(run_loop.AnyQuitCalled());
    task_environment_.FastForwardBy(kTinyTime);
    EXPECT_TRUE(run_loop.AnyQuitCalled());
  }

  // Configures `url_loader_factory_` to return the provided script, and then
  // runs its generate_bid() function. Then runs the script, expecting the
  // provided result.
  void RunScoreAdWithJavascriptExpectingResult(
      const std::string& javascript,
      double expected_score,
      const std::vector<std::string>& expected_errors =
          std::vector<std::string>()) {
    SCOPED_TRACE(javascript);
    AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                          javascript);
    RunScoreAdExpectingResult(expected_score, expected_errors);
  }

  // Runs score_ad() script, checking result and invoking provided closure
  // when done. Something else must spin the event loop.
  void RunScoreAdOnWorkletAsync(mojom::SellerWorklet* seller_worklet,
                                double expected_score,
                                const std::vector<std::string>& expected_errors,
                                base::OnceClosure done_closure) {
    seller_worklet->ScoreAd(
        ad_metadata_, bid_, auction_ad_config_non_shared_params_.Clone(),
        browser_signal_interest_group_owner_, browser_signal_render_url_,
        browser_signal_ad_components_, browser_signal_bidding_duration_msecs_,
        base::BindOnce(
            [](double expected_score, std::vector<std::string> expected_errors,
               base::OnceClosure done_closure, double score,
               const std::vector<std::string>& errors) {
              EXPECT_EQ(expected_score, score);
              EXPECT_EQ(expected_errors, errors);
              std::move(done_closure).Run();
            },
            expected_score, expected_errors, std::move(done_closure)));
  }

  void RunScoreAdOnWorkletExpectingCallbackNeverInvoked(
      mojom::SellerWorklet* seller_worklet) {
    seller_worklet->ScoreAd(
        ad_metadata_, bid_, auction_ad_config_non_shared_params_.Clone(),
        browser_signal_interest_group_owner_, browser_signal_render_url_,
        browser_signal_ad_components_, browser_signal_bidding_duration_msecs_,
        base::BindOnce(
            [](double score, const std::vector<std::string>& errors) {
              ADD_FAILURE() << "This should not be invoked";
            }));
  }

  // Loads and runs a scode_ad() script, expecting the supplied result.
  void RunScoreAdExpectingResultOnWorklet(
      mojom::SellerWorklet* seller_worklet,
      double expected_score,
      const std::vector<std::string>& expected_errors =
          std::vector<std::string>()) {
    base::RunLoop run_loop;
    RunScoreAdOnWorkletAsync(seller_worklet, expected_score, expected_errors,
                             run_loop.QuitClosure());
    run_loop.Run();
  }

  // Loads and runs a scode_ad() script, expecting the supplied result.
  void RunScoreAdExpectingResult(
      double expected_score,
      const std::vector<std::string>& expected_errors =
          std::vector<std::string>()) {
    auto seller_worklet = CreateWorklet();
    ASSERT_TRUE(seller_worklet);
    RunScoreAdExpectingResultOnWorklet(seller_worklet.get(), expected_score,
                                       expected_errors);
  }

  // Configures `url_loader_factory_` to return a report_result() script created
  // with CreateReportToScript(). Then runs the script, expecting the provided
  // result.
  void RunReportResultCreatedScriptExpectingResult(
      const std::string& raw_return_value,
      const std::string& extra_code,
      const absl::optional<std::string>& expected_signals_for_winner,
      const absl::optional<GURL>& expected_report_url,
      const std::vector<std::string>& expected_errors =
          std::vector<std::string>()) {
    RunReportResultWithJavascriptExpectingResult(
        CreateReportToScript(raw_return_value, extra_code),
        expected_signals_for_winner, expected_report_url, expected_errors);
  }

  // Configures `url_loader_factory_` to return the provided script, and then
  // runs its report_result() function. Then runs the script, expecting the
  // provided result.
  void RunReportResultWithJavascriptExpectingResult(
      const std::string& javascript,
      const absl::optional<std::string>& expected_signals_for_winner,
      const absl::optional<GURL>& expected_report_url,
      const std::vector<std::string>& expected_errors =
          std::vector<std::string>()) {
    SCOPED_TRACE(javascript);
    AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                          javascript);
    RunReportResultExpectingResult(expected_signals_for_winner,
                                   expected_report_url, expected_errors);
  }

  // Loads and runs a report_result() script, expecting the supplied result.
  // Caller is responsible for spinning the event loop at least until
  // `done_closure`.
  void RunReportResultExpectingResultAsync(
      mojom::SellerWorklet* seller_worklet,
      const absl::optional<std::string>& expected_signals_for_winner,
      const absl::optional<GURL>& expected_report_url,
      const std::vector<std::string>& expected_errors,
      base::OnceClosure done_closure) {
    seller_worklet->ReportResult(
        auction_ad_config_non_shared_params_.Clone(),
        browser_signal_interest_group_owner_, browser_signal_render_url_, bid_,
        browser_signal_desireability_,
        base::BindOnce(
            [](const absl::optional<std::string>& expected_signals_for_winner,
               const absl::optional<GURL>& expected_report_url,
               const std::vector<std::string>& expected_errors,
               base::OnceClosure done_closure,
               const absl::optional<std::string>& signals_for_winner,
               const absl::optional<GURL>& report_url,
               const std::vector<std::string>& errors) {
              EXPECT_EQ(expected_signals_for_winner, signals_for_winner);
              EXPECT_EQ(expected_report_url, report_url);
              EXPECT_EQ(expected_errors, errors);
              std::move(done_closure).Run();
            },
            expected_signals_for_winner, expected_report_url, expected_errors,
            std::move(done_closure)));
  }

  // Loads and runs a report_result() script, expecting the supplied result.
  // Runs ScoreAd() first, expecting a score of 1, since that's required before
  // calling ReportResult.
  void RunReportResultExpectingResult(
      const absl::optional<std::string>& expected_signals_for_winner,
      const absl::optional<GURL>& expected_report_url,
      const std::vector<std::string>& expected_errors =
          std::vector<std::string>()) {
    auto seller_worklet = CreateWorklet();
    RunScoreAdExpectingResultOnWorklet(seller_worklet.get(), 1);
    ASSERT_TRUE(seller_worklet);

    base::RunLoop run_loop;
    RunReportResultExpectingResultAsync(
        seller_worklet.get(), expected_signals_for_winner, expected_report_url,
        expected_errors, run_loop.QuitClosure());
    run_loop.Run();
  }

  // Create a seller worklet. If out_seller_worklet_impl is non-null, will also
  // the stash an actual implementation pointer there.
  mojo::Remote<mojom::SellerWorklet> CreateWorklet(
      bool pause_for_debugger_on_start = false,
      SellerWorklet** out_seller_worklet_impl = nullptr) {
    mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory;
    url_loader_factory_.Clone(
        url_loader_factory.InitWithNewPipeAndPassReceiver());

    mojo::Remote<mojom::SellerWorklet> seller_worklet;
    auto seller_worklet_impl = std::make_unique<SellerWorklet>(
        v8_helper_, pause_for_debugger_on_start, std::move(url_loader_factory),
        decision_logic_url_, trusted_scoring_signals_url_, top_window_origin_);
    auto* seller_worklet_ptr = seller_worklet_impl.get();
    mojo::ReceiverId receiver_id =
        seller_worklets_.Add(std::move(seller_worklet_impl),
                             seller_worklet.BindNewPipeAndPassReceiver());
    seller_worklet_ptr->set_close_pipe_callback(
        base::BindOnce(&SellerWorkletTest::ClosePipeCallback,
                       base::Unretained(this), receiver_id));
    seller_worklet.set_disconnect_with_reason_handler(base::BindRepeating(
        &SellerWorkletTest::OnDisconnectWithReason, base::Unretained(this)));

    if (out_seller_worklet_impl)
      *out_seller_worklet_impl = seller_worklet_ptr;
    return seller_worklet;
  }

  // Waits for OnDisconnectWithReason() to be invoked, if it hasn't been
  // already, and returns the error string it was invoked with.
  std::string WaitForDisconnect() {
    DCHECK(!disconnect_run_loop_);

    if (!disconnect_reason_) {
      disconnect_run_loop_ = std::make_unique<base::RunLoop>();
      disconnect_run_loop_->Run();
      disconnect_run_loop_.reset();
    }

    DCHECK(disconnect_reason_);
    std::string disconnect_reason = std::move(disconnect_reason_).value();
    disconnect_reason_.reset();
    return disconnect_reason;
  }

 protected:
  void ClosePipeCallback(mojo::ReceiverId receiver_id,
                         const std::string& description) {
    seller_worklets_.RemoveWithReason(receiver_id, /*custom_reason_code=*/0,
                                      description);
  }

  void OnDisconnectWithReason(uint32_t custom_reason,
                              const std::string& description) {
    DCHECK(!disconnect_reason_);

    disconnect_reason_ = description;
    if (disconnect_run_loop_)
      disconnect_run_loop_->Quit();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // Arguments passed to score_bid() and report_result(). Arguments common to
  // both of them use the same field.
  std::string ad_metadata_;
  // This is a browser signal for report_result(), but a direct parameter for
  // score_bid().
  double bid_;
  GURL decision_logic_url_;
  absl::optional<GURL> trusted_scoring_signals_url_;
  blink::mojom::AuctionAdConfigNonSharedParamsPtr
      auction_ad_config_non_shared_params_;
  url::Origin top_window_origin_;
  url::Origin browser_signal_interest_group_owner_;
  GURL browser_signal_render_url_;
  std::vector<GURL> browser_signal_ad_components_;
  uint32_t browser_signal_bidding_duration_msecs_;
  double browser_signal_desireability_;

  // Reuseable run loop for disconnection errors.
  std::unique_ptr<base::RunLoop> disconnect_run_loop_;
  absl::optional<std::string> disconnect_reason_;

  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<AuctionV8Helper> v8_helper_;

  // Owns all created seller worklets - having a ReceiverSet allows them to have
  // a ClosePipeCallback which behaves just like the one in
  // AuctionWorkletServiceImpl, to better match production behavior.
  mojo::UniqueReceiverSet<mojom::SellerWorklet> seller_worklets_;
};

// Test the case the SellerWorklet pipe is closed before any of its methods are
// invoked. Nothing should happen.
TEST_F(SellerWorkletTest, PipeClosed) {
  auto sellet_worklet = CreateWorklet();
  sellet_worklet.reset();
  base::RunLoop().RunUntilIdle();
}

TEST_F(SellerWorkletTest, NetworkError) {
  url_loader_factory_.AddResponse(decision_logic_url_.spec(),
                                  CreateBasicSellAdScript(),
                                  net::HTTP_NOT_FOUND);
  auto sellet_worklet = CreateWorklet();
  EXPECT_EQ("Failed to load https://url.test/ HTTP status = 404 Not Found.",
            WaitForDisconnect());
}

TEST_F(SellerWorkletTest, CompileError) {
  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        "Invalid Javascript");
  auto sellet_worklet = CreateWorklet();
  std::string disconnect_error = WaitForDisconnect();
  EXPECT_THAT(disconnect_error, StartsWith("https://url.test/:1 "));
  EXPECT_THAT(disconnect_error, HasSubstr("SyntaxError"));
}

// Test parsing of return values.
TEST_F(SellerWorkletTest, ScoreAd) {
  // Base case. Also serves to make sure the script returned by
  // CreateBasicSellAdScript() does indeed work.
  RunScoreAdWithJavascriptExpectingResult(CreateBasicSellAdScript(), 1);

  RunScoreAdWithReturnValueExpectingResult("3", 3);
  RunScoreAdWithReturnValueExpectingResult("0.5", 0.5);
  RunScoreAdWithReturnValueExpectingResult("0", 0);
  RunScoreAdWithReturnValueExpectingResult("-10", 0);

  // No return value.
  RunScoreAdWithReturnValueExpectingResult(
      "", 0, {"https://url.test/ scoreAd() did not return a valid number."});

  // Wrong return type / invalid values.
  RunScoreAdWithReturnValueExpectingResult(
      "[15]", 0,
      {"https://url.test/ scoreAd() did not return a valid number."});
  RunScoreAdWithReturnValueExpectingResult(
      "1/0", 0, {"https://url.test/ scoreAd() did not return a valid number."});
  RunScoreAdWithReturnValueExpectingResult(
      "0/0", 0, {"https://url.test/ scoreAd() did not return a valid number."});
  RunScoreAdWithReturnValueExpectingResult(
      "-1/0", 0,
      {"https://url.test/ scoreAd() did not return a valid number."});
  RunScoreAdWithReturnValueExpectingResult(
      "true", 0,
      {"https://url.test/ scoreAd() did not return a valid number."});

  // Throw exception.
  RunScoreAdWithReturnValueExpectingResult(
      "shrimp", 0,
      {"https://url.test/:4 Uncaught ReferenceError: shrimp is not defined."});
}

TEST_F(SellerWorkletTest, ScoreAdDateNotAvailable) {
  RunScoreAdWithReturnValueExpectingResult(
      "Date.parse(Date().toString())", 0,
      {"https://url.test/:4 Uncaught ReferenceError: Date is not defined."});
}

TEST_F(SellerWorkletTest, ScoreAdLogAndError) {
  const char kScript[] = R"(
    function scoreAd() {
      console.log("Logging");
      return "hello";
    }
  )";

  RunScoreAdWithJavascriptExpectingResult(
      kScript, 0,
      {"https://url.test/ [Log]: Logging",
       "https://url.test/ scoreAd() did not return a valid number."});
}

TEST_F(SellerWorkletTest, ScoreAdMedata) {
  ad_metadata_ = R"("foo")";
  RunScoreAdWithReturnValueExpectingResult(R"(adMetadata === "foo" ? 4 : 0)",
                                           4);

  ad_metadata_ = "[1]";
  RunScoreAdWithReturnValueExpectingResult(R"(adMetadata[0] === 1 ? 4 : 0)", 4);
}

TEST_F(SellerWorkletTest, ScoreAdTopWindowOrigin) {
  top_window_origin_ = url::Origin::Create(GURL("https://foo.test/"));
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.topWindowHostname == "foo.test" ? 2 : 0)", 2);

  top_window_origin_ = url::Origin::Create(GURL("https://[::1]:40000/"));
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.topWindowHostname == "[::1]" ? 3 : 0)", 3);
}

TEST_F(SellerWorkletTest, ScoreAdInterestGroupOwner) {
  browser_signal_interest_group_owner_ =
      url::Origin::Create(GURL("https://foo.test/"));
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.interestGroupOwner == "https://foo.test" ? 2 : 0)", 2);

  browser_signal_interest_group_owner_ =
      url::Origin::Create(GURL("https://[::1]:40000/"));
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.interestGroupOwner == "https://[::1]:40000" ? 3 : 0)",
      3);
}

TEST_F(SellerWorkletTest, ScoreAdRenderUrl) {
  browser_signal_render_url_ = GURL("https://bar.test/path");
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.renderUrl === "https://bar.test/path" ? 3 : 0)", 3);
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.renderUrl === "https://bar.test/" ? 3 : 0)", 0);
}

TEST_F(SellerWorkletTest, ScoreAdAdComponents) {
  browser_signal_ad_components_.clear();
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.adComponents === undefined ? 3 : 0)", 3);

  browser_signal_ad_components_ = {GURL("https://bar.test/path")};
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.adComponents.length)", 1);
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.adComponents[0] === "https://bar.test/path" ? 3 : 0)",
      3);

  // These are not in lexical order to make sure ordering is preserved.
  browser_signal_ad_components_ = {GURL("https://2.test/"),
                                   GURL("https://1.test/"),
                                   GURL("https://3.test/")};
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.adComponents.length)", 3);
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.adComponents[0] === "https://2.test/" ? 3 : 0)", 3);
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.adComponents[1] === "https://1.test/" ? 3 : 0)", 3);
  RunScoreAdWithReturnValueExpectingResult(
      R"(browserSignals.adComponents[2] === "https://3.test/" ? 3 : 0)", 3);
}

TEST_F(SellerWorkletTest, ScoreAdBid) {
  bid_ = 5;
  RunScoreAdWithReturnValueExpectingResult(base::StringPrintf("bid"), 5);
  bid_ = 0.5;
  RunScoreAdWithReturnValueExpectingResult(base::StringPrintf("bid"), 0.5);
  bid_ = -1;
  RunScoreAdWithReturnValueExpectingResult(base::StringPrintf("bid"), 0);
}

TEST_F(SellerWorkletTest, ScoreAdBiddingDuration) {
  // Test browserSignals.bidding_duration_msec.
  browser_signal_bidding_duration_msecs_ = 0;
  RunScoreAdWithReturnValueExpectingResult(
      base::StringPrintf("browserSignals.biddingDurationMsec"), 0);
  browser_signal_bidding_duration_msecs_ = 100;
  RunScoreAdWithReturnValueExpectingResult(
      base::StringPrintf("browserSignals.biddingDurationMsec"), 100);
}

// Test that auction config gets into scoreAd. More detailed handling of
// (shared) construction of actual object is in ReportResultAuctionConfigParam,
// as that worklet is easier to get things out of.
TEST_F(SellerWorkletTest, ScoreAdAuctionConfigParam) {
  decision_logic_url_ = GURL("https://url.test/");
  RunScoreAdWithReturnValueExpectingResult(
      "auctionConfig.decisionLogicUrl.length",
      decision_logic_url_.spec().length());

  decision_logic_url_ = GURL("https://url.test/longer/url");
  RunScoreAdWithReturnValueExpectingResult(
      "auctionConfig.decisionLogicUrl.length",
      decision_logic_url_.spec().length());
}

// Tests that trusted scoring signals are correctly passed to scoreAd(). Each
// request is sent individually, without calling SendPendingSignalsRequests() -
// instead, the test advances the mock clock by
// TrustedSignalsRequestManager::kAutoSendDelay, triggering each request to
// automatically be sent.
TEST_F(SellerWorkletTest, ScoreAdTrustedScoringSignals) {
  // With no trusted scoring signals URL, `trustedScoringSignals` should be
  // null.
  trusted_scoring_signals_url_ = absl::nullopt;
  RunScoreAdWithReturnValueExpectingResult(
      "trustedScoringSignals === null ? 1 : 0", 1);

  trusted_scoring_signals_url_ =
      GURL("https://url.test/trusted_scoring_signals");
  // Trusted scoring signals URL without any component ads.
  const GURL kNoComponentSignalsUrl = GURL(
      "https://url.test/trusted_scoring_signals?hostname=window.test"
      "&renderUrls=https%3A%2F%2Frender.url.test%2F");

  // Successful download case.

  AddJsonResponse(&url_loader_factory_, kNoComponentSignalsUrl,
                  kTrustedScoringSignalsResponse);

  // Each call should cause the clock to advance exactly `kAutoSendDelay`
  // milliseconds before the request is send over the wire, waiting for other
  // requests.
  RunScoreAdWithReturnValueExpectingResultInExactTime(
      "trustedScoringSignals.renderUrl['https://render.url.test/']",
      4 /* Magic value in trustedScoringSignals */,
      TrustedSignalsRequestManager::kAutoSendDelay);
  RunScoreAdWithReturnValueExpectingResultInExactTime(
      "trustedScoringSignals.adComponentRenderUrls === undefined ? 1 : 0", 1,
      TrustedSignalsRequestManager::kAutoSendDelay);

  // A network error when fetching the scoring signals results in null
  // `trustedScoringSignals`. This case is just before the component ad test
  // case so that its error response for `kNoComponentSignalsUrl` makes a
  // failure if that URL is incorrectly requested in the component ad test case.
  url_loader_factory_.AddResponse(kNoComponentSignalsUrl.spec(),
                                  /*content=*/std::string(),
                                  net::HTTP_NOT_FOUND);
  RunScoreAdWithReturnValueExpectingResultInExactTime(
      "trustedScoringSignals === null ? 1 : 0", 1,
      TrustedSignalsRequestManager::kAutoSendDelay,
      /*expected_errors=*/
      {base::StringPrintf("Failed to load %s HTTP status = 404 Not Found.",
                          kNoComponentSignalsUrl.spec().c_str())});

  browser_signal_ad_components_ = {GURL("https://component1.test/"),
                                   GURL("https://component2.test/")};
  AddJsonResponse(
      &url_loader_factory_,
      GURL("https://url.test/trusted_scoring_signals?hostname=window.test"
           "&renderUrls=https%3A%2F%2Frender.url.test%2F"
           "&adComponentRenderUrls=https%3A%2F%2Fcomponent1.test%2F,"
           "https%3A%2F%2Fcomponent2.test%2F"),
      kTrustedScoringSignalsResponse);

  RunScoreAdWithReturnValueExpectingResultInExactTime(
      "trustedScoringSignals.renderUrl['https://render.url.test/']",
      4 /* Magic value in trustedScoringSignals */,
      TrustedSignalsRequestManager::kAutoSendDelay);
  RunScoreAdWithReturnValueExpectingResultInExactTime(
      "trustedScoringSignals.adComponentRenderUrls['https://component1.test/']",
      1 /* Magic value in trustedScoringSignals */,
      TrustedSignalsRequestManager::kAutoSendDelay);
  RunScoreAdWithReturnValueExpectingResultInExactTime(
      "trustedScoringSignals.adComponentRenderUrls['https://component2.test/']",
      2 /* Magic value in trustedScoringSignals */,
      TrustedSignalsRequestManager::kAutoSendDelay);
}

// Test the case of a bunch of ScoreAd() calls in parallel, all started before
// the worklet script has loaded.
TEST_F(SellerWorkletTest, ScoreAdParallelBeforeLoadComplete) {
  mojo::Remote<mojom::SellerWorklet> seller_worklet =
      CreateWorklet(/*pause_for_debugger_on_start=*/false);

  const size_t kNumWorklets = 10;
  size_t num_completed_worklets = 0;
  base::RunLoop run_loop;
  for (size_t i = 0; i < kNumWorklets; ++i) {
    browser_signal_render_url_ = GURL(base::StringPrintf("https://foo/%zu", i));
    RunScoreAdOnWorkletAsync(seller_worklet.get(), /*expected_score=*/i,
                             /*expected_errors=*/std::vector<std::string>(),
                             base::BindLambdaForTesting([&]() {
                               ++num_completed_worklets;
                               if (num_completed_worklets == kNumWorklets)
                                 run_loop.Quit();
                             }));
  }

  // No calls should complete, since the script hasn't loaded yet.
  task_environment_.RunUntilIdle();
  EXPECT_EQ(0u, num_completed_worklets);

  // Load a seller script that uses the last character of `renderUrl` as the
  // score. The worklet should report a successful load.
  AddJavascriptResponse(
      &url_loader_factory_, decision_logic_url_,
      CreateScoreAdScript("parseInt(browserSignals.renderUrl.slice(-1))"));

  // All scripts should complete successfully.
  run_loop.Run();
}

// Test the case of a bunch of ScoreAd() calls in parallel, all started after
// the worklet script has loaded.
TEST_F(SellerWorkletTest, ScoreAdParallelAfterLoadComplete) {
  // Seller script that uses the last character of `renderUrl` as the score.
  AddJavascriptResponse(
      &url_loader_factory_, decision_logic_url_,
      CreateScoreAdScript("parseInt(browserSignals.renderUrl.slice(-1))"));
  auto seller_worklet = CreateWorklet();

  const size_t kNumWorklets = 10;
  size_t num_completed_worklets = 0;
  base::RunLoop run_loop;
  for (size_t i = 0; i < kNumWorklets; ++i) {
    browser_signal_render_url_ = GURL(base::StringPrintf("https://foo/%zu", i));
    RunScoreAdOnWorkletAsync(seller_worklet.get(), /*expected_score=*/i,
                             /*expected_errors=*/std::vector<std::string>(),
                             base::BindLambdaForTesting([&]() {
                               ++num_completed_worklets;
                               if (num_completed_worklets == kNumWorklets)
                                 run_loop.Quit();
                             }));
  }
  run_loop.Run();
}

// Test the case of a bunch of ScoreAd() calls in parallel, all started before
// the worklet script fails to load.
TEST_F(SellerWorkletTest, ScoreAdParallelLoadFails) {
  mojo::Remote<mojom::SellerWorklet> seller_worklet = CreateWorklet();

  for (size_t i = 0; i < 10; ++i) {
    browser_signal_render_url_ = GURL(base::StringPrintf("https://foo/%zu", i));
    RunScoreAdOnWorkletExpectingCallbackNeverInvoked(seller_worklet.get());
  }

  // No calls should complete, since the script hasn't loaded yet.
  task_environment_.RunUntilIdle();

  // Script fails to load.
  url_loader_factory_.AddResponse(decision_logic_url_.spec(),
                                  /*content=*/std::string(),
                                  net::HTTP_NOT_FOUND);

  // The worklet should fail to load.
  EXPECT_EQ("Failed to load https://url.test/ HTTP status = 404 Not Found.",
            WaitForDisconnect());
  // The worklet script callbacks should not be invoked.
  task_environment_.RunUntilIdle();
}

// Test the case of a bunch of ScoreAd() calls in parallel, in the case trusted
// scoring signals is non-null. In this case, call AllBidsGenerated() between
// scoring each bid, which should result in requests being sent individually.
TEST_F(SellerWorkletTest, ScoreAdParallelTrustedScoringSignalsNotBatched) {
  base::Time start_time = base::Time::Now();

  // Seller script that gets the score from the `trustedScoringSignals` value of
  // the passed in `renderUrl`.
  AddJavascriptResponse(
      &url_loader_factory_, decision_logic_url_,
      CreateScoreAdScript(
          "trustedScoringSignals.renderUrl[browserSignals.renderUrl]"));
  trusted_scoring_signals_url_ =
      GURL("https://url.test/trusted_scoring_signals");
  auto seller_worklet = CreateWorklet();

  // Start scoring a bunch of worklets. Don't provide JSON responses, to make
  // sure they all reside in the worklet's task list at once.
  const size_t kNumWorklets = 10;
  size_t num_completed_worklets = 0;
  base::RunLoop run_loop;
  for (size_t i = 0; i < kNumWorklets; ++i) {
    browser_signal_render_url_ = GURL(base::StringPrintf("https://foo/%zu", i));
    RunScoreAdOnWorkletAsync(seller_worklet.get(), /*expected_score=*/2 * i,
                             /*expected_errors=*/std::vector<std::string>(),
                             base::BindLambdaForTesting([&]() {
                               ++num_completed_worklets;
                               if (num_completed_worklets == kNumWorklets)
                                 run_loop.Quit();
                             }));
    seller_worklet->SendPendingSignalsRequests();
  }

  // Spin run loop so all requests reach the scoring worklet.
  run_loop.RunUntilIdle();
  EXPECT_EQ(0u, num_completed_worklets);

  // Provide all JSON responses.
  for (size_t i = 0; i < kNumWorklets; ++i) {
    GURL trusted_scoring_signals = GURL(base::StringPrintf(
        "%s?hostname=%s&renderUrls=https%%3A%%2F%%2Ffoo%%2F%zu",
        trusted_scoring_signals_url_->spec().c_str(),
        top_window_origin_.host().c_str(), i));
    std::string response_body = base::StringPrintf(
        R"({"renderUrls": {"https://foo/%zu": %zu}})", i, 2 * i);
    AddJsonResponse(&url_loader_factory_, trusted_scoring_signals,
                    response_body);
  }
  run_loop.Run();

  // No time should have passed during this test, since the
  // SendPendingSignalsRequests() calls ensure requests are send immediately,
  // without waiting on a timer. Using a mock time ensures that the passage of
  // wall clock time doesn't impact the current time, only delayed tasks and
  // timers do.
  EXPECT_EQ(base::Time::Now(), start_time);
}

// Test the case of a bunch of ScoreAd() calls in parallel, in the case trusted
// scoring signals is non-null. In this case, don't call AllBidsGenerated()
// between scoring each bid, which should result in all requests being sent as a
// single request.
//
// In this test, the ordering is:
// 1) The worklet script load completes.
// 2) ScoreAd() calls are made.
// 3) The trusted bidding signals are loaded.
TEST_F(SellerWorkletTest, ScoreAdParallelTrustedScoringSignalsBatched1) {
  // Seller script that gets the score from the `trustedScoringSignals` value of
  // the passed in `renderUrl`.
  AddJavascriptResponse(
      &url_loader_factory_, decision_logic_url_,
      CreateScoreAdScript(
          "trustedScoringSignals.renderUrl[browserSignals.renderUrl]"));
  trusted_scoring_signals_url_ =
      GURL("https://url.test/trusted_scoring_signals");
  auto seller_worklet = CreateWorklet();

  // Start scoring a bunch of worklets. Don't provide JSON responses, to make
  // sure they all reside in the worklet's task list at once.
  const size_t kNumWorklets = 10;
  size_t num_completed_worklets = 0;
  base::RunLoop run_loop;
  for (size_t i = 0; i < kNumWorklets; ++i) {
    browser_signal_render_url_ = GURL(base::StringPrintf("https://foo/%zu", i));
    RunScoreAdOnWorkletAsync(seller_worklet.get(), /*expected_score=*/2 * i,
                             /*expected_errors=*/std::vector<std::string>(),
                             base::BindLambdaForTesting([&]() {
                               ++num_completed_worklets;
                               if (num_completed_worklets == kNumWorklets)
                                 run_loop.Quit();
                             }));
  }

  // Spin run loop so all requests reach the scoring worklet.
  run_loop.RunUntilIdle();
  EXPECT_EQ(0u, num_completed_worklets);

  // Provide a single response for the merged URL request.
  std::string request_url =
      base::StringPrintf("%s?hostname=%s&renderUrls=",
                         trusted_scoring_signals_url_->spec().c_str(),
                         top_window_origin_.host().c_str());
  std::string response_body;
  for (size_t i = 0; i < kNumWorklets; ++i) {
    if (i > 0) {
      request_url += ",";
      response_body += ",";
    }
    request_url += base::StringPrintf("https%%3A%%2F%%2Ffoo%%2F%zu", i);
    response_body += base::StringPrintf(R"("https://foo/%zu": %zu)", i, 2 * i);
  }
  response_body =
      base::StringPrintf(R"({"renderUrls": {%s}})", response_body.c_str());
  AddJsonResponse(&url_loader_factory_, GURL(request_url), response_body);

  // All ScoreAd() calls should succeed with the expected scores.
  run_loop.Run();
}

// Same as above, but with different ordering.
//
// In this test, the ordering is:
// 1) ScoreAd() calls are made.
// 2) The worklet script load completes.
// 3) The trusted bidding signals are loaded.
TEST_F(SellerWorkletTest, ScoreAdParallelTrustedScoringSignalsBatched2) {
  trusted_scoring_signals_url_ =
      GURL("https://url.test/trusted_scoring_signals");
  mojo::Remote<mojom::SellerWorklet> seller_worklet = CreateWorklet();

  // Start scoring a bunch of worklets. Don't provide JSON responses, to make
  // sure they all reside in the worklet's task list at once.
  const size_t kNumWorklets = 10;
  size_t num_completed_worklets = 0;
  base::RunLoop run_loop;
  for (size_t i = 0; i < kNumWorklets; ++i) {
    browser_signal_render_url_ = GURL(base::StringPrintf("https://foo/%zu", i));
    RunScoreAdOnWorkletAsync(seller_worklet.get(), /*expected_score=*/2 * i,
                             /*expected_errors=*/std::vector<std::string>(),
                             base::BindLambdaForTesting([&]() {
                               ++num_completed_worklets;
                               if (num_completed_worklets == kNumWorklets)
                                 run_loop.Quit();
                             }));
  }

  // Spin run loop so all requests reach the scoring worklet.
  run_loop.RunUntilIdle();
  EXPECT_EQ(0u, num_completed_worklets);

  // Return seller script that gets the score from the `trustedScoringSignals`
  // value of the passed in `renderUrl`, and wait for it to finish loading.
  AddJavascriptResponse(
      &url_loader_factory_, decision_logic_url_,
      CreateScoreAdScript(
          "trustedScoringSignals.renderUrl[browserSignals.renderUrl]"));
  task_environment_.RunUntilIdle();

  // Provide a single response for the merged URL request.
  std::string request_url =
      base::StringPrintf("%s?hostname=%s&renderUrls=",
                         trusted_scoring_signals_url_->spec().c_str(),
                         top_window_origin_.host().c_str());
  std::string response_body;
  for (size_t i = 0; i < kNumWorklets; ++i) {
    if (i > 0) {
      request_url += ",";
      response_body += ",";
    }
    request_url += base::StringPrintf("https%%3A%%2F%%2Ffoo%%2F%zu", i);
    response_body += base::StringPrintf(R"("https://foo/%zu": %zu)", i, 2 * i);
  }
  response_body =
      base::StringPrintf(R"({"renderUrls": {%s}})", response_body.c_str());
  AddJsonResponse(&url_loader_factory_, GURL(request_url), response_body);

  // All ScoreAd() calls should succeed with the expected scores.
  run_loop.Run();
}

// Same as above, but with different ordering.
//
// In this test, the ordering is:
// 1) ScoreAd() calls are made.
// 2) The trusted bidding signals are loaded.
// 3) The worklet script load completes.
TEST_F(SellerWorkletTest, ScoreAdParallelTrustedScoringSignalsBatched3) {
  trusted_scoring_signals_url_ =
      GURL("https://url.test/trusted_scoring_signals");
  mojo::Remote<mojom::SellerWorklet> seller_worklet = CreateWorklet();

  // Start scoring a bunch of worklets. Don't provide JSON responses, to make
  // sure they all reside in the worklet's task list at once.
  const size_t kNumWorklets = 10;
  size_t num_completed_worklets = 0;
  base::RunLoop run_loop;
  for (size_t i = 0; i < kNumWorklets; ++i) {
    browser_signal_render_url_ = GURL(base::StringPrintf("https://foo/%zu", i));
    RunScoreAdOnWorkletAsync(seller_worklet.get(), /*expected_score=*/2 * i,
                             /*expected_errors=*/std::vector<std::string>(),
                             base::BindLambdaForTesting([&]() {
                               ++num_completed_worklets;
                               if (num_completed_worklets == kNumWorklets)
                                 run_loop.Quit();
                             }));
  }

  // Spin run loop so all requests reach the scoring worklet.
  run_loop.RunUntilIdle();
  EXPECT_EQ(0u, num_completed_worklets);

  // Provide a single response for the merged URL request.
  std::string request_url =
      base::StringPrintf("%s?hostname=%s&renderUrls=",
                         trusted_scoring_signals_url_->spec().c_str(),
                         top_window_origin_.host().c_str());
  std::string response_body;
  for (size_t i = 0; i < kNumWorklets; ++i) {
    if (i > 0) {
      request_url += ",";
      response_body += ",";
    }
    request_url += base::StringPrintf("https%%3A%%2F%%2Ffoo%%2F%zu", i);
    response_body += base::StringPrintf(R"("https://foo/%zu": %zu)", i, 2 * i);
  }
  response_body =
      base::StringPrintf(R"({"renderUrls": {%s}})", response_body.c_str());
  AddJsonResponse(&url_loader_factory_, GURL(request_url), response_body);

  // Spin run loop so the response is handled. No ScoreAdCalls should complete
  // yet.
  run_loop.RunUntilIdle();
  EXPECT_EQ(0u, num_completed_worklets);

  // Return seller script that gets the score from the `trustedScoringSignals`
  // value of the passed in `renderUrl`, and wait for it to finish loading.
  AddJavascriptResponse(
      &url_loader_factory_, decision_logic_url_,
      CreateScoreAdScript(
          "trustedScoringSignals.renderUrl[browserSignals.renderUrl]"));

  // All ScoreAd() calls should succeed with the expected scores.
  run_loop.Run();
}

// Tests parsing of return values.
TEST_F(SellerWorkletTest, ReportResult) {
  RunReportResultCreatedScriptExpectingResult(
      "1", std::string() /* extra_code */,
      "1" /* expected_signals_for_winner */,
      absl::nullopt /* expected_report_url */);
  RunReportResultCreatedScriptExpectingResult(
      R"("  1   ")", std::string() /* extra_code */,
      R"("  1   ")" /* expected_signals_for_winner */,
      absl::nullopt /* expected_report_url */);
  RunReportResultCreatedScriptExpectingResult(
      "[ null ]", std::string() /* extra_code */, "[null]",
      absl::nullopt /* expected_report_url */);

  // No return value.
  RunReportResultCreatedScriptExpectingResult(
      "", std::string() /* extra_code */, "null",
      absl::nullopt /* expected_report_url */);

  // Throw exception.
  RunReportResultCreatedScriptExpectingResult(
      "shrimp", std::string() /* extra_code */,
      absl::nullopt /* expected_signals_for_winner */,
      absl::nullopt /* expected_render_url */,
      {"https://url.test/:9 Uncaught ReferenceError: "
       "shrimp is not defined."});
}

// Tests reporting URLs.
TEST_F(SellerWorkletTest, ReportResultSendReportTo) {
  RunReportResultCreatedScriptExpectingResult(
      "1", R"(sendReportTo("https://foo.test"))",
      "1" /* expected_signals_for_winner */, GURL("https://foo.test/"));
  RunReportResultCreatedScriptExpectingResult(
      "1", R"(sendReportTo("https://foo.test/bar"))",
      "1" /* expected_signals_for_winner */, GURL("https://foo.test/bar"));

  // Disallowed schemes.
  RunReportResultCreatedScriptExpectingResult(
      "1", R"(sendReportTo("http://foo.test/"))",
      absl::nullopt /* expected_signals_for_winner */,
      absl::nullopt /* expected_render_url */,
      {"https://url.test/:8 Uncaught TypeError: "
       "sendReportTo must be passed a valid HTTPS url."});
  RunReportResultCreatedScriptExpectingResult(
      "1", R"(sendReportTo("file:///foo/"))",
      absl::nullopt /* expected_signals_for_winner */,
      absl::nullopt /* expected_render_url */,
      {"https://url.test/:8 Uncaught TypeError: "
       "sendReportTo must be passed a valid HTTPS url."});

  // Multiple calls.
  RunReportResultCreatedScriptExpectingResult(
      "1",
      R"(sendReportTo("https://foo.test/"); sendReportTo("https://foo.test/"))",
      absl::nullopt /* expected_signals_for_winner */,
      absl::nullopt /* expected_render_url */,
      {"https://url.test/:8 Uncaught TypeError: "
       "sendReportTo may be called at most once."});

  // No message if caught, but still no URL.
  RunReportResultCreatedScriptExpectingResult(
      "1",
      R"(try {
        sendReportTo("https://foo.test/");
        sendReportTo("https://foo.test/")} catch(e) {})",
      "1" /* expected_render_url */, absl::nullopt /* expected_report_url */);

  // Not a URL.
  RunReportResultCreatedScriptExpectingResult(
      "1", R"(sendReportTo("France"))",
      absl::nullopt /* expected_signals_for_winner */,
      absl::nullopt /* expected_render_url */,
      {"https://url.test/:8 Uncaught TypeError: "
       "sendReportTo must be passed a valid HTTPS url."});
  RunReportResultCreatedScriptExpectingResult(
      "1", R"(sendReportTo(null))",
      absl::nullopt /* expected_signals_for_winner */,
      absl::nullopt /* expected_render_url */,
      {"https://url.test/:8 Uncaught TypeError: "
       "sendReportTo requires 1 string parameter."});
  RunReportResultCreatedScriptExpectingResult(
      "1", R"(sendReportTo([5]))",
      absl::nullopt /* expected_signals_for_winner */,
      absl::nullopt /* expected_render_url */,
      {"https://url.test/:8 Uncaught TypeError: "
       "sendReportTo requires 1 string parameter."});
}

TEST_F(SellerWorkletTest, ReportResultDateNotAvailable) {
  RunReportResultCreatedScriptExpectingResult(
      "1", R"(sendReportTo("https://foo.test/" + Date().toString()))",
      absl::nullopt /* expected_signals_for_winner */,
      absl::nullopt /* expected_render_url */,
      {"https://url.test/:8 Uncaught ReferenceError: Date is not defined."});
}

TEST_F(SellerWorkletTest, ReportResultTopWindowOrigin) {
  top_window_origin_ = url::Origin::Create(GURL("https://foo.test/"));
  RunReportResultCreatedScriptExpectingResult(
      R"(browserSignals.topWindowHostname == "foo.test" ? 2 : 1)",
      std::string() /* extra_code */, "2",
      absl::nullopt /* expected_report_url */);

  top_window_origin_ = url::Origin::Create(GURL("https://[::1]:40000/"));
  RunReportResultCreatedScriptExpectingResult(
      R"(browserSignals.topWindowHostname == "[::1]" ? 3 : 1)",
      std::string() /* extra_code */, "3",
      absl::nullopt /* expected_report_url */);
}

TEST_F(SellerWorkletTest, ReportResultInterestGroupOwner) {
  browser_signal_interest_group_owner_ =
      url::Origin::Create(GURL("https://foo.test/"));
  RunReportResultCreatedScriptExpectingResult(
      R"(browserSignals.interestGroupOwner == "https://foo.test" ? 2 : 1)",
      std::string() /* extra_code */, "2",
      absl::nullopt /* expected_report_url */);

  browser_signal_interest_group_owner_ =
      url::Origin::Create(GURL("https://[::1]:40000/"));
  RunReportResultCreatedScriptExpectingResult(
      R"(browserSignals.interestGroupOwner == "https://[::1]:40000" ? 3 : 1)",
      std::string() /* extra_code */, "3",
      absl::nullopt /* expected_report_url */);
}

TEST_F(SellerWorkletTest, ReportResultRenderUrl) {
  browser_signal_render_url_ = GURL("https://foo/");
  RunReportResultCreatedScriptExpectingResult(
      "browserSignals.renderUrl", "sendReportTo(browserSignals.renderUrl)",
      R"("https://foo/")", browser_signal_render_url_);
}

TEST_F(SellerWorkletTest, ReportResultBid) {
  bid_ = 5;
  RunReportResultCreatedScriptExpectingResult(
      "browserSignals.bid + typeof browserSignals.bid",
      std::string() /* extra_code */, R"("5number")",
      absl::nullopt /* expected_report_url */);
}

TEST_F(SellerWorkletTest, ReportResultDesireability) {
  browser_signal_desireability_ = 10;
  RunReportResultCreatedScriptExpectingResult(
      "browserSignals.desirability + typeof browserSignals.desirability",
      std::string() /* extra_code */, R"("10number")",
      absl::nullopt /* expected_report_url */);
}

TEST_F(SellerWorkletTest, ReportResultAuctionConfigParam) {
  // Empty AuctionAdConfig, with nothing filled in, except the seller and
  // decision logic URL.
  decision_logic_url_ = GURL("https://example.com/auction.js");
  RunReportResultCreatedScriptExpectingResult(
      "auctionConfig", std::string() /* extra_code */,
      R"({"seller":"https://example.com",)"
      R"("decisionLogicUrl":"https://example.com/auction.js"})",
      absl::nullopt /* expected_report_url */);

  // Everything filled in.
  decision_logic_url_ = GURL("https://example.com/auction.js");
  auction_ad_config_non_shared_params_->interest_group_buyers =
      blink::mojom::InterestGroupBuyers::NewAllBuyers(
          blink::mojom::AllBuyers::New());
  auction_ad_config_non_shared_params_->auction_signals =
      R"({"is_auction_signals": true})";
  auction_ad_config_non_shared_params_->seller_signals =
      R"({"is_seller_signals": true})";
  base::flat_map<url::Origin, std::string> per_buyer_signals;
  per_buyer_signals[url::Origin::Create(GURL("https://a.com"))] =
      R"({"signals_a": "A"})";
  per_buyer_signals[url::Origin::Create(GURL("https://b.com"))] =
      R"({"signals_b": "B"})";
  auction_ad_config_non_shared_params_->per_buyer_signals =
      std::move(per_buyer_signals);

  const char kExpectedJson[] =
      R"({"seller":"https://example.com",)"
      R"("decisionLogicUrl":"https://example.com/auction.js",)"
      R"("interestGroupBuyers":"*",)"
      R"("auctionSignals":{"is_auction_signals":true},)"
      R"("sellerSignals":{"is_seller_signals":true},)"
      R"("perBuyerSignals":{"https://a.com":{"signals_a":"A"},)"
      R"("https://b.com":{"signals_b":"B"}}})";
  RunReportResultCreatedScriptExpectingResult(
      "auctionConfig", std::string() /* extra_code */, kExpectedJson,
      absl::nullopt /* expected_report_url */);

  // Array option for interest_group_buyers. Everything else optional
  // unpopulated.
  std::vector<url::Origin> buyers;
  buyers.push_back(url::Origin::Create(GURL("https://buyer1.com")));
  buyers.push_back(url::Origin::Create(GURL("https://another-buyer.com")));
  auction_ad_config_non_shared_params_ =
      blink::mojom::AuctionAdConfigNonSharedParams::New();
  decision_logic_url_ = GURL("https://example.com/auction.js");
  auction_ad_config_non_shared_params_->interest_group_buyers =
      blink::mojom::InterestGroupBuyers::NewBuyers(std::move(buyers));
  const char kExpectedJson2[] =
      R"({"seller":"https://example.com",)"
      R"("decisionLogicUrl":"https://example.com/auction.js",)"
      R"("interestGroupBuyers":["https://buyer1.com","https://another-buyer.com"]})";
  RunReportResultCreatedScriptExpectingResult(
      "auctionConfig", std::string() /* extra_code */, kExpectedJson2,
      absl::nullopt /* expected_report_url */);
}

// Subsequent runs of the same script should not affect each other. Same is true
// for different scripts, but it follows from the single script case.
TEST_F(SellerWorkletTest, ScriptIsolation) {
  // Use arrays so that all values are references, to catch both the case where
  // variables are persisted, and the case where what they refer to is
  // persisted, but variables are overwritten between runs.
  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        R"(
        // Globally scoped variable.
        if (!globalThis.var1)
          globalThis.var1 = [1];
        scoreAd = function() {
          // Value only visible within this closure.
          var var2 = [2];
          return function() {
            if (2 == ++globalThis.var1[0] && 3 == ++var2[0])
              return 2;
            return 1;
          }
        }();

        reportResult = scoreAd;
      )");
  auto seller_worklet = CreateWorklet();
  ASSERT_TRUE(seller_worklet);

  for (int i = 0; i < 3; ++i) {
    // Run each script twice in a row, to cover both cases where the same
    // function is run sequentially, and when one function is run after the
    // other.
    for (int j = 0; j < 2; ++j) {
      base::RunLoop run_loop;
      seller_worklet->ScoreAd(
          ad_metadata_, bid_, auction_ad_config_non_shared_params_.Clone(),
          browser_signal_interest_group_owner_, browser_signal_render_url_,
          browser_signal_ad_components_, browser_signal_bidding_duration_msecs_,
          base::BindLambdaForTesting(
              [&run_loop](double score,
                          const std::vector<std::string>& errors) {
                EXPECT_EQ(2, score);
                EXPECT_TRUE(errors.empty());
                run_loop.Quit();
              }));
      run_loop.Run();
    }

    for (int j = 0; j < 2; ++j) {
      base::RunLoop run_loop;
      seller_worklet->ReportResult(
          auction_ad_config_non_shared_params_.Clone(),
          browser_signal_interest_group_owner_, browser_signal_render_url_,
          bid_, browser_signal_desireability_,
          base::BindLambdaForTesting(
              [&run_loop](const absl::optional<std::string>& signals_for_winner,
                          const absl::optional<GURL>& report_url,
                          const std::vector<std::string>& errors) {
                EXPECT_EQ("2", signals_for_winner);
                EXPECT_TRUE(errors.empty());
                run_loop.Quit();
              }));
      run_loop.Run();
    }
  }
}

TEST_F(SellerWorkletTest, DeleteBeforeScoreAdCallback) {
  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        CreateBasicSellAdScript());
  auto seller_worklet = CreateWorklet();
  ASSERT_TRUE(seller_worklet);

  base::WaitableEvent* event_handle = WedgeV8Thread(v8_helper_.get());
  seller_worklet->ScoreAd(
      ad_metadata_, bid_, auction_ad_config_non_shared_params_.Clone(),
      browser_signal_interest_group_owner_, browser_signal_render_url_,
      browser_signal_ad_components_, browser_signal_bidding_duration_msecs_,
      base::BindOnce([](double score, const std::vector<std::string>& errors) {
        ADD_FAILURE() << "Callback should not be invoked since worklet deleted";
      }));
  base::RunLoop().RunUntilIdle();
  seller_worklet.reset();
  event_handle->Signal();
}

TEST_F(SellerWorkletTest, DeleteBeforeReportResultCallback) {
  AddJavascriptResponse(
      &url_loader_factory_, decision_logic_url_,
      CreateReportToScript("1", R"(sendReportTo("https://foo.test"))"));
  auto seller_worklet = CreateWorklet();
  ASSERT_TRUE(seller_worklet);
  // Need to call ScoreAd() calling ReportResult().
  RunScoreAdExpectingResultOnWorklet(seller_worklet.get(), 1);

  base::WaitableEvent* event_handle = WedgeV8Thread(v8_helper_.get());
  seller_worklet->ReportResult(
      auction_ad_config_non_shared_params_.Clone(),
      browser_signal_interest_group_owner_, browser_signal_render_url_, bid_,
      browser_signal_desireability_,
      base::BindOnce([](const absl::optional<std::string>& signals_for_winner,
                        const absl::optional<GURL>& report_url,
                        const std::vector<std::string>& errors) {
        ADD_FAILURE() << "Callback should not be invoked since worklet deleted";
      }));
  base::RunLoop().RunUntilIdle();
  seller_worklet.reset();
  event_handle->Signal();
}

TEST_F(SellerWorkletTest, PauseOnStart) {
  // If pause isn't working, this will be used and not the right script.
  url_loader_factory_.AddResponse(decision_logic_url_.spec(), "",
                                  net::HTTP_NOT_FOUND);

  SellerWorklet* worklet_impl = nullptr;
  auto worklet =
      CreateWorklet(/*pause_for_debugger_on_start=*/true, &worklet_impl);
  // Grab the context ID to be able to resume.
  int id = worklet_impl->context_group_id_for_testing();

  // Queue a ScoreAd() call, which should not happen immediately since loading
  // is paused.
  base::RunLoop run_loop;
  RunScoreAdOnWorkletAsync(worklet.get(), /*expected_score=*/10,
                           /*expected_errors=*/{}, run_loop.QuitClosure());

  // Give it a chance to fetch.
  task_environment_.RunUntilIdle();
  EXPECT_FALSE(run_loop.AnyQuitCalled());

  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        CreateScoreAdScript("10"));

  task_environment_.RunUntilIdle();
  EXPECT_FALSE(run_loop.AnyQuitCalled());

  // Let the ScoreAd() call run.
  v8_helper_->v8_runner()->PostTask(
      FROM_HERE, base::BindOnce([](scoped_refptr<AuctionV8Helper> v8_helper,
                                   int id) { v8_helper->Resume(id); },
                                v8_helper_, id));

  run_loop.RunUntilIdle();
}

TEST_F(SellerWorkletTest, PauseOnStartDelete) {
  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        CreateScoreAdScript("10"));

  SellerWorklet* worklet_impl = nullptr;
  auto worklet =
      CreateWorklet(/*pause_for_debugger_on_start=*/true, &worklet_impl);

  // Queue a ScoreAd() call, which should start paused and will never be run.
  base::RunLoop run_loop;
  RunScoreAdOnWorkletExpectingCallbackNeverInvoked(worklet.get());

  // Give it a chance to fetch.
  task_environment_.RunUntilIdle();

  // Grab the context ID.
  int id = worklet_impl->context_group_id_for_testing();

  // Delete the worklet.
  worklet.reset();
  task_environment_.RunUntilIdle();

  // Try to resume post-delete. Should not crash
  v8_helper_->v8_runner()->PostTask(
      FROM_HERE, base::BindOnce([](scoped_refptr<AuctionV8Helper> v8_helper,
                                   int id) { v8_helper->Resume(id); },
                                v8_helper_, id));

  task_environment_.RunUntilIdle();
}

TEST_F(SellerWorkletTest, BasicV8Debug) {
  ScopedInspectorSupport inspector_support(v8_helper_.get());

  // Helper for looking for scriptParsed events.
  auto is_script_parsed = [](const TestChannel::Event& event) -> bool {
    if (event.type != TestChannel::Event::Type::Notification)
      return false;

    const std::string* candidate_method = event.value.FindStringKey("method");
    return (candidate_method && *candidate_method == "Debugger.scriptParsed");
  };

  const GURL kUrl1 = GURL("http://example.com/first.js");
  const GURL kUrl2 = GURL("http://example.org/second.js");

  AddJavascriptResponse(&url_loader_factory_, kUrl1, CreateScoreAdScript("1"));
  AddJavascriptResponse(&url_loader_factory_, kUrl2, CreateScoreAdScript("2"));

  SellerWorklet* worklet_impl1 = nullptr;
  decision_logic_url_ = kUrl1;
  auto worklet1 = CreateWorklet(
      /*pause_for_debugger_on_start=*/true, &worklet_impl1);
  base::RunLoop run_loop1;
  RunScoreAdOnWorkletAsync(worklet1.get(), /*expected_score=*/1,
                           /*expected_errors=*/{}, run_loop1.QuitClosure());

  decision_logic_url_ = kUrl2;
  SellerWorklet* worklet_impl2 = nullptr;
  auto worklet2 = CreateWorklet(
      /*pause_for_debugger_on_start=*/true, &worklet_impl2);
  base::RunLoop run_loop2;
  RunScoreAdOnWorkletAsync(worklet2.get(), /*expected_score=*/2,
                           /*expected_errors=*/{}, run_loop2.QuitClosure());

  int id1 = worklet_impl1->context_group_id_for_testing();
  int id2 = worklet_impl2->context_group_id_for_testing();

  TestChannel* channel1 = inspector_support.ConnectDebuggerSession(id1);
  TestChannel* channel2 = inspector_support.ConnectDebuggerSession(id2);

  channel1->RunCommandAndWaitForResult(
      1, "Runtime.enable", R"({"id":1,"method":"Runtime.enable","params":{}})");
  channel1->RunCommandAndWaitForResult(
      2, "Debugger.enable",
      R"({"id":2,"method":"Debugger.enable","params":{}})");

  channel2->RunCommandAndWaitForResult(
      1, "Runtime.enable", R"({"id":1,"method":"Runtime.enable","params":{}})");
  channel2->RunCommandAndWaitForResult(
      2, "Debugger.enable",
      R"({"id":2,"method":"Debugger.enable","params":{}})");

  // Should not see scriptParsed before resume.
  std::list<TestChannel::Event> events1 = channel1->TakeAllEvents();
  EXPECT_TRUE(std::none_of(events1.begin(), events1.end(), is_script_parsed));

  // Unpause execution for #1.
  EXPECT_FALSE(run_loop1.AnyQuitCalled());
  channel1->RunCommandAndWaitForResult(
      3, "Runtime.runIfWaitingForDebugger",
      R"({"id":3,"method":"Runtime.runIfWaitingForDebugger","params":{}})");
  run_loop1.Run();

  // channel1 should have had a parsed notification for kUrl1.
  TestChannel::Event script_parsed1 =
      channel1->WaitForMethodNotification("Debugger.scriptParsed");
  const std::string* url1 = script_parsed1.value.FindStringPath("params.url");
  ASSERT_TRUE(url1);
  EXPECT_EQ(kUrl1.spec(), *url1);

  // There shouldn't be a parsed notification on channel 2, however.
  std::list<TestChannel::Event> events2 = channel2->TakeAllEvents();
  EXPECT_TRUE(std::none_of(events2.begin(), events2.end(), is_script_parsed));

  // Unpause execution for #2.
  EXPECT_FALSE(run_loop2.AnyQuitCalled());
  channel2->RunCommandAndWaitForResult(
      3, "Runtime.runIfWaitingForDebugger",
      R"({"id":3,"method":"Runtime.runIfWaitingForDebugger","params":{}})");
  run_loop2.Run();

  // channel2 should have had a parsed notification for kUrl2.
  TestChannel::Event script_parsed2 =
      channel2->WaitForMethodNotification("Debugger.scriptParsed");
  const std::string* url2 = script_parsed2.value.FindStringPath("params.url");
  ASSERT_TRUE(url2);
  EXPECT_EQ(kUrl2, *url2);

  worklet1.reset();
  worklet2.reset();
  task_environment_.RunUntilIdle();

  // No other scriptParsed events should be on either channel.
  events1 = channel1->TakeAllEvents();
  events2 = channel2->TakeAllEvents();
  EXPECT_TRUE(std::none_of(events1.begin(), events1.end(), is_script_parsed));
  EXPECT_TRUE(std::none_of(events2.begin(), events2.end(), is_script_parsed));
}

TEST_F(SellerWorkletTest, ParseErrorV8Debug) {
  ScopedInspectorSupport inspector_support(v8_helper_.get());
  AddJavascriptResponse(&url_loader_factory_, decision_logic_url_,
                        "Invalid Javascript");
  SellerWorklet* worklet_impl = nullptr;
  auto worklet =
      CreateWorklet(/*pause_for_debugger_on_start=*/true, &worklet_impl);
  int id = worklet_impl->context_group_id_for_testing();
  TestChannel* channel = inspector_support.ConnectDebuggerSession(id);

  channel->RunCommandAndWaitForResult(
      1, "Runtime.enable", R"({"id":1,"method":"Runtime.enable","params":{}})");
  channel->RunCommandAndWaitForResult(
      2, "Debugger.enable",
      R"({"id":2,"method":"Debugger.enable","params":{}})");

  // Unpause execution and wait for the pipe to be closed with an error.
  channel->RunCommandAndWaitForResult(
      3, "Runtime.runIfWaitingForDebugger",
      R"({"id":3,"method":"Runtime.runIfWaitingForDebugger","params":{}})");
  EXPECT_FALSE(WaitForDisconnect().empty());

  // Should have gotten a parse error notification.
  TestChannel::Event parse_error =
      channel->WaitForMethodNotification("Debugger.scriptFailedToParse");
  const std::string* error_url = parse_error.value.FindStringPath("params.url");
  ASSERT_TRUE(error_url);
  EXPECT_EQ(decision_logic_url_.spec(), *error_url);
}

TEST_F(SellerWorkletTest, BasicDevToolsDebug) {
  const char kScriptResult[] = "this.global_score ? this.global_score : 10";

  const char kUrl1[] = "http://example.com/first.js";
  const char kUrl2[] = "http://example.org/second.js";

  AddJavascriptResponse(&url_loader_factory_, GURL(kUrl1),
                        CreateScoreAdScript(kScriptResult));
  AddJavascriptResponse(&url_loader_factory_, GURL(kUrl2),
                        CreateScoreAdScript(kScriptResult));

  decision_logic_url_ = GURL(kUrl1);
  auto worklet1 = CreateWorklet(true /* pause_for_debugger_on_start */);
  base::RunLoop run_loop1;
  RunScoreAdOnWorkletAsync(worklet1.get(), 100.5, {}, run_loop1.QuitClosure());

  decision_logic_url_ = GURL(kUrl2);
  auto worklet2 = CreateWorklet(true /* pause_for_debugger_on_start */);
  base::RunLoop run_loop2;
  RunScoreAdOnWorkletAsync(
      worklet2.get(), 0,
      {"http://example.org/second.js scoreAd() did not return a valid number."},
      run_loop2.QuitClosure());

  mojo::Remote<blink::mojom::DevToolsAgent> agent1, agent2;
  worklet1->ConnectDevToolsAgent(agent1.BindNewPipeAndPassReceiver());
  worklet2->ConnectDevToolsAgent(agent2.BindNewPipeAndPassReceiver());

  TestDevToolsAgentClient debug1(std::move(agent1), "123",
                                 true /* use_binary_protocol */);
  TestDevToolsAgentClient debug2(std::move(agent2), "456",
                                 true /* use_binary_protocol */);

  debug1.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 1, "Runtime.enable",
      R"({"id":1,"method":"Runtime.enable","params":{}})");
  debug1.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 2, "Debugger.enable",
      R"({"id":2,"method":"Debugger.enable","params":{}})");

  debug2.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 1, "Runtime.enable",
      R"({"id":1,"method":"Runtime.enable","params":{}})");
  debug2.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 2, "Debugger.enable",
      R"({"id":2,"method":"Debugger.enable","params":{}})");

  const char kBreakpointTemplate[] = R"({
        "id":3,
        "method":"Debugger.setBreakpointByUrl",
        "params": {
          "lineNumber": 2,
          "url": "%s",
          "columnNumber": 0,
          "condition": ""
        }})";

  debug1.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 3, "Debugger.setBreakpointByUrl",
      base::StringPrintf(kBreakpointTemplate, kUrl1));
  debug2.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 3, "Debugger.setBreakpointByUrl",
      base::StringPrintf(kBreakpointTemplate, kUrl2));

  // Now start #1. We should see a scriptParsed event.
  debug1.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 4,
      "Runtime.runIfWaitingForDebugger",
      R"({"id":4,"method":"Runtime.runIfWaitingForDebugger","params":{}})");

  TestDevToolsAgentClient::Event script_parsed1 =
      debug1.WaitForMethodNotification("Debugger.scriptParsed");
  const std::string* url1 = script_parsed1.value.FindStringPath("params.url");
  ASSERT_TRUE(url1);
  EXPECT_EQ(*url1, kUrl1);
  absl::optional<int> context_id1 =
      script_parsed1.value.FindIntPath("params.executionContextId");
  ASSERT_TRUE(context_id1.has_value());

  // Next there is the breakpoint.
  TestDevToolsAgentClient::Event breakpoint_hit1 =
      debug1.WaitForMethodNotification("Debugger.paused");

  base::Value* hit_breakpoints1 =
      breakpoint_hit1.value.FindListPath("params.hitBreakpoints");
  ASSERT_TRUE(hit_breakpoints1);
  base::Value::ConstListView hit_breakpoints_list1 =
      hit_breakpoints1->GetList();
  ASSERT_EQ(1u, hit_breakpoints_list1.size());
  ASSERT_TRUE(hit_breakpoints_list1[0].is_string());
  EXPECT_EQ("1:2:0:http://example.com/first.js",
            hit_breakpoints_list1[0].GetString());

  // Override the score value.
  const char kCommandTemplate[] = R"({
    "id": 5,
    "method": "Runtime.evaluate",
    "params": {
      "expression": "global_score = %s",
      "contextId": %d
    }
  })";

  debug1.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 5, "Runtime.evaluate",
      base::StringPrintf(kCommandTemplate, "100.5", context_id1.value()));

  // Let worklet 1 finish. The callback set by RunScoreAdOnWorkletAsync() will
  // verify the result.
  EXPECT_FALSE(run_loop1.AnyQuitCalled());
  debug1.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 6, "Debugger.resume",
      R"({"id":6,"method":"Debugger.resume","params":{}})");
  run_loop1.Run();

  // Start #2, see that it parses the script.
  debug2.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 4,
      "Runtime.runIfWaitingForDebugger",
      R"({"id":4,"method":"Runtime.runIfWaitingForDebugger","params":{}})");

  TestDevToolsAgentClient::Event script_parsed2 =
      debug2.WaitForMethodNotification("Debugger.scriptParsed");
  const std::string* url2 = script_parsed2.value.FindStringPath("params.url");
  ASSERT_TRUE(url2);
  EXPECT_EQ(*url2, kUrl2);
  absl::optional<int> context_id2 =
      script_parsed2.value.FindIntPath("params.executionContextId");
  ASSERT_TRUE(context_id2.has_value());

  // Wait for breakpoint, and then change the result to be trouble.
  TestDevToolsAgentClient::Event breakpoint_hit2 =
      debug2.WaitForMethodNotification("Debugger.paused");
  debug2.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 5, "Runtime.evaluate",
      base::StringPrintf(kCommandTemplate, R"(\"not a score\")",
                         context_id2.value()));

  // Let worklet 2 finish. The callback set by RunScoreAdOnWorkletAsync() will
  // verify the result.
  debug2.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 6, "Debugger.resume",
      R"({"id":6,"method":"Debugger.resume","params":{}})");
  run_loop2.Run();
}

TEST_F(SellerWorkletTest, InstrumentationBreakpoints) {
  const char kUrl[] = "http://example.com/script.js";

  std::string script_body =
      CreateBasicSellAdScript() +
      CreateReportToScript("1", R"(sendReportTo("https://foo.test"))");
  AddJavascriptResponse(&url_loader_factory_, GURL(kUrl), script_body);

  decision_logic_url_ = GURL(kUrl);
  auto worklet = CreateWorklet(true /* pause_for_debugger_on_start */);
  base::RunLoop run_loop;
  RunScoreAdOnWorkletAsync(worklet.get(), 1.0, {}, run_loop.QuitClosure());

  mojo::Remote<blink::mojom::DevToolsAgent> agent;
  worklet->ConnectDevToolsAgent(agent.BindNewPipeAndPassReceiver());

  TestDevToolsAgentClient debug(std::move(agent), "123",
                                true /* use_binary_protocol */);

  debug.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 1, "Runtime.enable",
      R"({"id":1,"method":"Runtime.enable","params":{}})");
  debug.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 2, "Debugger.enable",
      R"({"id":2,"method":"Debugger.enable","params":{}})");

  // Set the instrumentation breakpoints.
  debug.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 3,
      "EventBreakpoints.setInstrumentationBreakpoint",
      MakeInstrumentationBreakpointCommand(3, "set",
                                           "beforeSellerWorkletScoringStart"));
  debug.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 4,
      "EventBreakpoints.setInstrumentationBreakpoint",
      MakeInstrumentationBreakpointCommand(
          4, "set", "beforeSellerWorkletReportingStart"));

  // Resume creation, ScoreAd() call should hit a breakpoint.
  debug.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 5,
      "Runtime.runIfWaitingForDebugger",
      R"({"id":5,"method":"Runtime.runIfWaitingForDebugger","params":{}})");

  TestDevToolsAgentClient::Event breakpoint_hit1 =
      debug.WaitForMethodNotification("Debugger.paused");

  const std::string* breakpoint1 =
      breakpoint_hit1.value.FindStringPath("params.data.eventName");
  ASSERT_TRUE(breakpoint1);
  EXPECT_EQ("instrumentation:beforeSellerWorkletScoringStart", *breakpoint1);

  // Let scoring finish.
  EXPECT_FALSE(run_loop.AnyQuitCalled());
  debug.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 6, "Debugger.resume",
      R"({"id":6,"method":"Debugger.resume","params":{}})");
  run_loop.Run();

  // Now try reporting, should hit the other breakpoint.
  base::RunLoop run_loop2;
  RunReportResultExpectingResultAsync(worklet.get(), "1",
                                      GURL("https://foo.test/"), {},
                                      run_loop2.QuitClosure());
  TestDevToolsAgentClient::Event breakpoint_hit2 =
      debug.WaitForMethodNotification("Debugger.paused");
  const std::string* breakpoint2 =
      breakpoint_hit2.value.FindStringPath("params.data.eventName");
  ASSERT_TRUE(breakpoint2);
  EXPECT_EQ("instrumentation:beforeSellerWorkletReportingStart", *breakpoint2);

  // Let reporting finish.
  debug.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 7, "Debugger.resume",
      R"({"id":7,"method":"Debugger.resume","params":{}})");
  run_loop2.Run();

  // Running another scoreAd will trigger the breakpoint again, since we didn't
  // remove it.
  base::RunLoop run_loop3;
  RunScoreAdOnWorkletAsync(worklet.get(), 1.0, {}, run_loop3.QuitClosure());

  TestDevToolsAgentClient::Event breakpoint_hit3 =
      debug.WaitForMethodNotification("Debugger.paused");

  const std::string* breakpoint3 =
      breakpoint_hit1.value.FindStringPath("params.data.eventName");
  ASSERT_TRUE(breakpoint3);
  EXPECT_EQ("instrumentation:beforeSellerWorkletScoringStart", *breakpoint3);

  // Let this round of scoring finish, too.
  debug.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kIO, 8, "Debugger.resume",
      R"({"id":8,"method":"Debugger.resume","params":{}})");
  run_loop3.Run();
}

TEST_F(SellerWorkletTest, UnloadWhilePaused) {
  // Make sure things are cleaned up properly if the worklet is destroyed while
  // paused on a breakpoint.
  const char kUrl[] = "http://example.com/script.js";

  std::string script_body =
      CreateBasicSellAdScript() +
      CreateReportToScript("1", R"(sendReportTo("https://foo.test"))");
  AddJavascriptResponse(&url_loader_factory_, GURL(kUrl), script_body);

  decision_logic_url_ = GURL(kUrl);
  auto worklet = CreateWorklet(/*pause_for_debugger_on_start=*/true);
  RunScoreAdOnWorkletExpectingCallbackNeverInvoked(worklet.get());

  mojo::Remote<blink::mojom::DevToolsAgent> agent;
  worklet->ConnectDevToolsAgent(agent.BindNewPipeAndPassReceiver());

  TestDevToolsAgentClient debug(std::move(agent), "123",
                                /*use_binary_protocol=*/true);

  debug.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 1, "Runtime.enable",
      R"({"id":1,"method":"Runtime.enable","params":{}})");
  debug.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 2, "Debugger.enable",
      R"({"id":2,"method":"Debugger.enable","params":{}})");

  // Set the instrumentation breakpoint.
  debug.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 3,
      "EventBreakpoints.setInstrumentationBreakpoint",
      MakeInstrumentationBreakpointCommand(3, "set",
                                           "beforeSellerWorkletScoringStart"));
  // Resume execution of create. Should hit corresponding breakpoint.
  debug.RunCommandAndWaitForResult(
      TestDevToolsAgentClient::Channel::kMain, 4,
      "Runtime.runIfWaitingForDebugger",
      R"({"id":4,"method":"Runtime.runIfWaitingForDebugger","params":{}})");

  RunScoreAdOnWorkletAsync(worklet.get(), 1.0, {}, base::BindOnce([]() {
                             ADD_FAILURE()
                                 << "scoreAd shouldn't actually get to finish.";
                           }));

  debug.WaitForMethodNotification("Debugger.paused");

  // Destroy the worklet
  worklet.reset();

  // This won't terminate if the V8 thread is still blocked in debugger.
  task_environment_.RunUntilIdle();
}

}  // namespace
}  // namespace auction_worklet
