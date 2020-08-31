// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/sms/sms_service.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "content/browser/sms/sms_fetcher_impl.h"
#include "content/browser/sms/test/mock_sms_provider.h"
#include "content/browser/sms/test/mock_sms_web_contents_delegate.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/sms_fetcher.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "services/service_manager/public/cpp/bind_source_info.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/sms/sms_receiver_destroyed_reason.h"
#include "third_party/blink/public/mojom/sms/sms_receiver.mojom.h"

using base::BindLambdaForTesting;
using base::Optional;
using blink::SmsReceiverDestroyedReason;
using blink::mojom::SmsReceiver;
using blink::mojom::SmsStatus;
using std::string;
using ::testing::_;
using ::testing::ByMove;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrictMock;
using url::Origin;

namespace content {

class RenderFrameHost;

namespace {

const char kTestUrl[] = "https://www.google.com";

// Service encapsulates a SmsService endpoint, with all of its dependencies
// mocked out (and the common plumbing needed to inject them), and a
// mojo::Remote<SmsReceiver> endpoint that tests can use to make requests.
// It exposes some common methods, like MakeRequest and NotifyReceive, but it
// also exposes the low level mocks that enables tests to set expectations and
// control the testing environment.
class Service {
 public:
  Service(WebContents* web_contents, const Origin& origin)
      : fetcher_(web_contents->GetBrowserContext(), &provider_) {
    WebContentsImpl* web_contents_impl =
        reinterpret_cast<WebContentsImpl*>(web_contents);
    web_contents_impl->SetDelegate(&delegate_);
    service_ = std::make_unique<SmsService>(
        &fetcher_, origin, web_contents->GetMainFrame(),
        service_remote_.BindNewPipeAndPassReceiver());
  }

  Service(WebContents* web_contents)
      : Service(web_contents,
                web_contents->GetMainFrame()->GetLastCommittedOrigin()) {}

  NiceMock<MockSmsProvider>* provider() { return &provider_; }
  SmsFetcher* fetcher() { return &fetcher_; }

  void CreateSmsPrompt(RenderFrameHost* rfh) {
    EXPECT_CALL(delegate_, CreateSmsPrompt(rfh, _, _, _, _))
        .WillOnce(Invoke([=](RenderFrameHost*, const Origin& origin,
                             const std::string&, base::OnceClosure on_confirm,
                             base::OnceClosure on_cancel) {
          confirm_callback_ = std::move(on_confirm);
          dismiss_callback_ = std::move(on_cancel);
        }));
  }

  void ConfirmPrompt() {
    if (!confirm_callback_.is_null()) {
      std::move(confirm_callback_).Run();
      dismiss_callback_.Reset();
      return;
    }
    FAIL() << "SmsInfobar not available";
  }

  void DismissPrompt() {
    if (dismiss_callback_.is_null()) {
      FAIL() << "SmsInfobar not available";
      return;
    }
    std::move(dismiss_callback_).Run();
    confirm_callback_.Reset();
  }

  bool IsPromptOpen() const {
    return !confirm_callback_.is_null() || !dismiss_callback_.is_null();
  }

  void MakeRequest(SmsReceiver::ReceiveCallback callback) {
    service_remote_->Receive(std::move(callback));
  }

  void AbortRequest() { service_remote_->Abort(); }

  void NotifyReceive(const GURL& url, const string& otp) {
    provider_.NotifyReceive(Origin::Create(url), otp);
  }

 private:
  NiceMock<MockSmsWebContentsDelegate> delegate_;
  NiceMock<MockSmsProvider> provider_;
  SmsFetcherImpl fetcher_;
  mojo::Remote<blink::mojom::SmsReceiver> service_remote_;
  std::unique_ptr<SmsService> service_;
  base::OnceClosure confirm_callback_;
  base::OnceClosure dismiss_callback_;
};

class SmsServiceTest : public RenderViewHostTestHarness {
 protected:
  SmsServiceTest() = default;
  ~SmsServiceTest() override = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        switches::kWebOtpBackend, switches::kWebOtpBackendSmsVerification);
  }

  void ExpectDestroyedReasonCount(SmsReceiverDestroyedReason bucket,
                                  int32_t count) {
    histogram_tester_.ExpectBucketCount("Blink.Sms.Receive.DestroyedReason",
                                        bucket, count);
  }

  const base::HistogramTester& histogram_tester() const {
    return histogram_tester_;
  }

 private:
  base::HistogramTester histogram_tester_;

  DISALLOW_COPY_AND_ASSIGN(SmsServiceTest);
};

}  // namespace

TEST_F(SmsServiceTest, Basic) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  base::RunLoop loop;

  service.CreateSmsPrompt(main_rfh());

  EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&service]() {
    service.NotifyReceive(GURL(kTestUrl), "hi");
    service.ConfirmPrompt();
  }));

  service.MakeRequest(BindLambdaForTesting(
      [&loop](SmsStatus status, const Optional<string>& otp) {
        EXPECT_EQ(SmsStatus::kSuccess, status);
        EXPECT_EQ("hi", otp.value());
        loop.Quit();
      }));

  loop.Run();

  ASSERT_FALSE(service.fetcher()->HasSubscribers());
}

TEST_F(SmsServiceTest, HandlesMultipleCalls) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  {
    base::RunLoop loop;

    service.CreateSmsPrompt(main_rfh());

    EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&service]() {
      service.NotifyReceive(GURL(kTestUrl), "first");
      service.ConfirmPrompt();
    }));

    service.MakeRequest(BindLambdaForTesting(
        [&loop](SmsStatus status, const Optional<string>& otp) {
          EXPECT_EQ("first", otp.value());
          EXPECT_EQ(SmsStatus::kSuccess, status);
          loop.Quit();
        }));

    loop.Run();
  }

  {
    base::RunLoop loop;

    service.CreateSmsPrompt(main_rfh());

    EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&service]() {
      service.NotifyReceive(GURL(kTestUrl), "second");
      service.ConfirmPrompt();
    }));

    service.MakeRequest(BindLambdaForTesting(
        [&loop](SmsStatus status, const Optional<string>& otp) {
          EXPECT_EQ("second", otp.value());
          EXPECT_EQ(SmsStatus::kSuccess, status);
          loop.Quit();
        }));

    loop.Run();
  }
}

TEST_F(SmsServiceTest, IgnoreFromOtherOrigins) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  SmsStatus sms_status;
  Optional<string> response;

  base::RunLoop sms_loop;

  service.CreateSmsPrompt(main_rfh());

  EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&service]() {
    // Delivers an SMS from an unrelated origin first and expect the
    // receiver to ignore it.
    service.NotifyReceive(GURL("http://b.com"), "wrong");
    service.NotifyReceive(GURL(kTestUrl), "right");
    service.ConfirmPrompt();
  }));

  service.MakeRequest(
      BindLambdaForTesting([&sms_status, &response, &sms_loop](
                               SmsStatus status, const Optional<string>& otp) {
        sms_status = status;
        response = otp;
        sms_loop.Quit();
      }));

  sms_loop.Run();

  EXPECT_EQ("right", response.value());
  EXPECT_EQ(SmsStatus::kSuccess, sms_status);
}

TEST_F(SmsServiceTest, ExpectOneReceiveTwo) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  SmsStatus sms_status;
  Optional<string> response;

  base::RunLoop sms_loop;

  service.CreateSmsPrompt(main_rfh());

  EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&service]() {
    // Delivers two SMSes for the same origin, even if only one was being
    // expected.
    ASSERT_TRUE(service.fetcher()->HasSubscribers());
    service.NotifyReceive(GURL(kTestUrl), "first");
    service.ConfirmPrompt();
    ASSERT_FALSE(service.fetcher()->HasSubscribers());
    service.NotifyReceive(GURL(kTestUrl), "second");
  }));

  service.MakeRequest(
      BindLambdaForTesting([&sms_status, &response, &sms_loop](
                               SmsStatus status, const Optional<string>& otp) {
        sms_status = status;
        response = otp;
        sms_loop.Quit();
      }));

  sms_loop.Run();

  EXPECT_EQ("first", response.value());
  EXPECT_EQ(SmsStatus::kSuccess, sms_status);
}

TEST_F(SmsServiceTest, AtMostOneSmsRequestPerOrigin) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  SmsStatus sms_status1;
  Optional<string> response1;
  SmsStatus sms_status2;
  Optional<string> response2;

  base::RunLoop sms1_loop, sms2_loop;

  // Expect SMS Prompt to be created once.
  service.CreateSmsPrompt(main_rfh());

  EXPECT_CALL(*service.provider(), Retrieve(_))
      .WillOnce(Return())
      .WillOnce(Invoke([&service]() {
        service.NotifyReceive(GURL(kTestUrl), "second");
        service.ConfirmPrompt();
      }));

  service.MakeRequest(
      BindLambdaForTesting([&sms_status1, &response1, &sms1_loop](
                               SmsStatus status, const Optional<string>& otp) {
        sms_status1 = status;
        response1 = otp;
        sms1_loop.Quit();
      }));

  // Make the 2nd SMS request which will cancel the 1st request because only
  // one request can be pending per origin per tab.
  service.MakeRequest(
      BindLambdaForTesting([&sms_status2, &response2, &sms2_loop](
                               SmsStatus status, const Optional<string>& otp) {
        sms_status2 = status;
        response2 = otp;
        sms2_loop.Quit();
      }));

  sms1_loop.Run();
  sms2_loop.Run();

  EXPECT_EQ(base::nullopt, response1);
  EXPECT_EQ(SmsStatus::kCancelled, sms_status1);

  EXPECT_EQ("second", response2.value());
  EXPECT_EQ(SmsStatus::kSuccess, sms_status2);
}

TEST_F(SmsServiceTest, SecondRequestDuringPrompt) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  SmsStatus sms_status1;
  Optional<string> response1;
  SmsStatus sms_status2;
  Optional<string> response2;

  base::RunLoop sms_loop;

  // Expect SMS Prompt to be created once.
  service.CreateSmsPrompt(main_rfh());

  EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&service]() {
    service.NotifyReceive(GURL(kTestUrl), "second");
  }));

  // First request.
  service.MakeRequest(
      BindLambdaForTesting([&sms_status1, &response1, &service](
                               SmsStatus status, const Optional<string>& otp) {
        sms_status1 = status;
        response1 = otp;
        service.ConfirmPrompt();
      }));

  // Make second request before confirming prompt.
  service.MakeRequest(
      BindLambdaForTesting([&sms_status2, &response2, &sms_loop](
                               SmsStatus status, const Optional<string>& otp) {
        sms_status2 = status;
        response2 = otp;
        sms_loop.Quit();
      }));

  sms_loop.Run();

  EXPECT_EQ(base::nullopt, response1);
  EXPECT_EQ(SmsStatus::kCancelled, sms_status1);

  EXPECT_EQ("second", response2.value());
  EXPECT_EQ(SmsStatus::kSuccess, sms_status2);
}

TEST_F(SmsServiceTest, CleansUp) {
  NavigateAndCommit(GURL(kTestUrl));

  NiceMock<MockSmsWebContentsDelegate> delegate;
  WebContentsImpl* web_contents_impl =
      reinterpret_cast<WebContentsImpl*>(web_contents());
  web_contents_impl->SetDelegate(&delegate);

  NiceMock<MockSmsProvider> provider;
  SmsFetcherImpl fetcher(web_contents()->GetBrowserContext(), &provider);
  mojo::Remote<blink::mojom::SmsReceiver> service;
  SmsService::Create(&fetcher, main_rfh(),
                     service.BindNewPipeAndPassReceiver());

  base::RunLoop navigate;

  EXPECT_CALL(provider, Retrieve(_)).WillOnce(Invoke([&navigate]() {
    navigate.Quit();
  }));

  base::RunLoop reload;

  service->Receive(base::BindLambdaForTesting(
      [&reload](SmsStatus status, const Optional<string>& otp) {
        EXPECT_EQ(SmsStatus::kTimeout, status);
        EXPECT_EQ(base::nullopt, otp);
        reload.Quit();
      }));

  navigate.Run();

  // Simulates the user reloading the page and navigating away, which
  // destructs the service.
  NavigateAndCommit(GURL(kTestUrl));

  reload.Run();

  ASSERT_FALSE(fetcher.HasSubscribers());
}

TEST_F(SmsServiceTest, PromptsDialog) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  base::RunLoop loop;

  service.CreateSmsPrompt(main_rfh());

  EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&service]() {
    service.NotifyReceive(GURL(kTestUrl), "hi");
    service.ConfirmPrompt();
  }));

  service.MakeRequest(BindLambdaForTesting(
      [&loop](SmsStatus status, const Optional<string>& otp) {
        EXPECT_EQ("hi", otp.value());
        EXPECT_EQ(SmsStatus::kSuccess, status);
        loop.Quit();
      }));

  loop.Run();

  ASSERT_FALSE(service.fetcher()->HasSubscribers());
}

TEST_F(SmsServiceTest, Cancel) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  base::RunLoop loop;

  service.CreateSmsPrompt(main_rfh());

  service.MakeRequest(BindLambdaForTesting(
      [&loop](SmsStatus status, const Optional<string>& otp) {
        EXPECT_EQ(SmsStatus::kCancelled, status);
        EXPECT_EQ(base::nullopt, otp);
        loop.Quit();
      }));

  EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&service]() {
    service.NotifyReceive(GURL(kTestUrl), "hi");
    service.DismissPrompt();
  }));

  loop.Run();

  ASSERT_FALSE(service.fetcher()->HasSubscribers());
}

TEST_F(SmsServiceTest, CancelForNoDelegate) {
  NavigateAndCommit(GURL(kTestUrl));

  NiceMock<MockSmsProvider> provider;
  SmsFetcherImpl fetcher(web_contents()->GetBrowserContext(), &provider);
  mojo::Remote<blink::mojom::SmsReceiver> service;
  SmsService::Create(&fetcher, main_rfh(),
                     service.BindNewPipeAndPassReceiver());

  base::RunLoop loop;

  service->Receive(base::BindLambdaForTesting(
      [&loop](SmsStatus status, const Optional<string>& otp) {
        EXPECT_EQ(SmsStatus::kCancelled, status);
        EXPECT_EQ(base::nullopt, otp);
        loop.Quit();
      }));

  loop.Run();

  ASSERT_FALSE(fetcher.HasSubscribers());
}

TEST_F(SmsServiceTest, Abort) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  base::RunLoop loop;

  service.MakeRequest(BindLambdaForTesting(
      [&loop](SmsStatus status, const Optional<string>& otp) {
        EXPECT_EQ(SmsStatus::kAborted, status);
        EXPECT_EQ(base::nullopt, otp);
        loop.Quit();
      }));

  service.AbortRequest();

  loop.Run();

  ASSERT_FALSE(service.fetcher()->HasSubscribers());
}

TEST_F(SmsServiceTest, AbortWhilePrompt) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  base::RunLoop loop;

  service.CreateSmsPrompt(main_rfh());

  service.MakeRequest(BindLambdaForTesting(
      [&loop](SmsStatus status, const Optional<string>& otp) {
        EXPECT_EQ(SmsStatus::kAborted, status);
        EXPECT_EQ(base::nullopt, otp);
        loop.Quit();
      }));

  EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&service]() {
    service.NotifyReceive(GURL(kTestUrl), "ABC");
    EXPECT_TRUE(service.IsPromptOpen());
    service.AbortRequest();
  }));

  loop.Run();

  ASSERT_FALSE(service.fetcher()->HasSubscribers());

  service.ConfirmPrompt();
}

TEST_F(SmsServiceTest, RequestAfterAbortWhilePrompt) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  {
    base::RunLoop loop;

    service.CreateSmsPrompt(main_rfh());

    service.MakeRequest(BindLambdaForTesting(
        [&loop](SmsStatus status, const Optional<string>& otp) {
          EXPECT_EQ(SmsStatus::kAborted, status);
          EXPECT_EQ(base::nullopt, otp);
          loop.Quit();
        }));

    EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&service]() {
      service.NotifyReceive(GURL(kTestUrl), "hi");
      EXPECT_TRUE(service.IsPromptOpen());
      service.AbortRequest();
    }));

    loop.Run();
  }

  ASSERT_FALSE(service.fetcher()->HasSubscribers());

  // Confirm to dismiss prompt for a request that has already aborted.
  service.ConfirmPrompt();

  {
    base::RunLoop loop;

    service.CreateSmsPrompt(main_rfh());

    service.MakeRequest(BindLambdaForTesting(
        [&loop](SmsStatus status, const Optional<string>& otp) {
          // Verify that the 2nd request completes successfully after prompt
          // confirmation.
          EXPECT_EQ(SmsStatus::kSuccess, status);
          EXPECT_EQ("hi2", otp.value());
          loop.Quit();
        }));

    EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&service]() {
      service.NotifyReceive(GURL(kTestUrl), "hi2");
      service.ConfirmPrompt();
    }));

    loop.Run();
  }
}

TEST_F(SmsServiceTest, SecondRequestWhilePrompt) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  base::RunLoop callback_loop1, callback_loop2, req_loop;

  service.CreateSmsPrompt(main_rfh());

  service.MakeRequest(BindLambdaForTesting(
      [&callback_loop1](SmsStatus status, const Optional<string>& otp) {
        EXPECT_EQ(SmsStatus::kAborted, status);
        EXPECT_EQ(base::nullopt, otp);
        callback_loop1.Quit();
      }));

  EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&service]() {
    service.NotifyReceive(GURL(kTestUrl), "hi");
    service.AbortRequest();
  }));

  callback_loop1.Run();

  base::ThreadTaskRunnerHandle::Get()->PostTaskAndReply(
      FROM_HERE, BindLambdaForTesting([&]() {
        service.MakeRequest(BindLambdaForTesting(
            [&callback_loop2](SmsStatus status, const Optional<string>& otp) {
              EXPECT_EQ(SmsStatus::kSuccess, status);
              EXPECT_EQ("hi", otp.value());
              callback_loop2.Quit();
            }));
      }),
      req_loop.QuitClosure());

  req_loop.Run();

  // Simulate pressing 'Verify' on Infobar.
  service.ConfirmPrompt();

  callback_loop2.Run();

  ASSERT_FALSE(service.fetcher()->HasSubscribers());
}

TEST_F(SmsServiceTest, RecordTimeMetricsForContinueOnSuccess) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  base::RunLoop loop;

  service.CreateSmsPrompt(main_rfh());

  EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&service]() {
    service.NotifyReceive(GURL(kTestUrl), "ABC");
    service.ConfirmPrompt();
  }));

  service.MakeRequest(BindLambdaForTesting(
      [&loop](SmsStatus status, const Optional<string>& otp) { loop.Quit(); }));

  loop.Run();

  histogram_tester().ExpectTotalCount("Blink.Sms.Receive.TimeContinueOnSuccess",
                                      1);
  histogram_tester().ExpectTotalCount("Blink.Sms.Receive.TimeSmsReceive", 1);
}

TEST_F(SmsServiceTest, RecordMetricsForCancelOnSuccess) {
  NavigateAndCommit(GURL(kTestUrl));

  Service service(web_contents());

  // Histogram will be recorded if the SMS has already arrived.
  base::RunLoop loop;

  service.CreateSmsPrompt(main_rfh());

  EXPECT_CALL(*service.provider(), Retrieve(_)).WillOnce(Invoke([&service]() {
    service.NotifyReceive(GURL(kTestUrl), "hi");
    service.DismissPrompt();
  }));

  service.MakeRequest(BindLambdaForTesting(
      [&loop](SmsStatus status, const Optional<string>& otp) { loop.Quit(); }));

  loop.Run();

  histogram_tester().ExpectTotalCount("Blink.Sms.Receive.TimeCancelOnSuccess",
                                      1);
  histogram_tester().ExpectTotalCount("Blink.Sms.Receive.TimeSmsReceive", 1);
}

TEST_F(SmsServiceTest, RecordMetricsForNewPage) {
  // This test depends on the page being destroyed on navigation.
  web_contents()->GetController().GetBackForwardCache().DisableForTesting(
      content::BackForwardCache::TEST_ASSUMES_NO_CACHING);
  NavigateAndCommit(GURL(kTestUrl));
  NiceMock<MockSmsWebContentsDelegate> delegate;
  WebContentsImpl* web_contents_impl =
      reinterpret_cast<WebContentsImpl*>(web_contents());
  web_contents_impl->SetDelegate(&delegate);

  NiceMock<MockSmsProvider> provider;
  SmsFetcherImpl fetcher(web_contents()->GetBrowserContext(), &provider);
  mojo::Remote<blink::mojom::SmsReceiver> service;
  SmsService::Create(&fetcher, main_rfh(),
                     service.BindNewPipeAndPassReceiver());

  base::RunLoop navigate;

  EXPECT_CALL(provider, Retrieve(_)).WillOnce(Invoke([&navigate]() {
    navigate.Quit();
  }));

  base::RunLoop reload;

  service->Receive(base::BindLambdaForTesting(
      [&reload](SmsStatus status, const Optional<string>& otp) {
        EXPECT_EQ(SmsStatus::kTimeout, status);
        EXPECT_EQ(base::nullopt, otp);
        reload.Quit();
      }));

  navigate.Run();

  // Simulates the user navigating to a new page.
  NavigateAndCommit(GURL("https://www.example.com"));

  reload.Run();

  ExpectDestroyedReasonCount(SmsReceiverDestroyedReason::kNavigateNewPage, 1);
}

TEST_F(SmsServiceTest, RecordMetricsForSamePage) {
  NavigateAndCommit(GURL(kTestUrl));
  NiceMock<MockSmsWebContentsDelegate> delegate;
  WebContentsImpl* web_contents_impl =
      reinterpret_cast<WebContentsImpl*>(web_contents());
  web_contents_impl->SetDelegate(&delegate);

  NiceMock<MockSmsProvider> provider;
  SmsFetcherImpl fetcher(web_contents()->GetBrowserContext(), &provider);
  mojo::Remote<blink::mojom::SmsReceiver> service;
  SmsService::Create(&fetcher, main_rfh(),
                     service.BindNewPipeAndPassReceiver());

  base::RunLoop navigate;

  EXPECT_CALL(provider, Retrieve(_)).WillOnce(Invoke([&navigate]() {
    navigate.Quit();
  }));

  base::RunLoop reload;

  service->Receive(base::BindLambdaForTesting(
      [&reload](SmsStatus status, const Optional<string>& otp) {
        EXPECT_EQ(SmsStatus::kTimeout, status);
        EXPECT_EQ(base::nullopt, otp);
        reload.Quit();
      }));

  navigate.Run();

  // Simulates the user re-navigating to the same page through the omni-box.
  NavigateAndCommit(GURL(kTestUrl));

  reload.Run();

  ExpectDestroyedReasonCount(SmsReceiverDestroyedReason::kNavigateSamePage, 1);
}

TEST_F(SmsServiceTest, RecordMetricsForExistingPage) {
  // This test depends on the page being destroyed on navigation.
  web_contents()->GetController().GetBackForwardCache().DisableForTesting(
      content::BackForwardCache::TEST_ASSUMES_NO_CACHING);
  NavigateAndCommit(GURL(kTestUrl));  // Add to history.
  NavigateAndCommit(GURL("https://example.com"));

  NiceMock<MockSmsWebContentsDelegate> delegate;
  WebContentsImpl* web_contents_impl =
      reinterpret_cast<WebContentsImpl*>(web_contents());
  web_contents_impl->SetDelegate(&delegate);

  NiceMock<MockSmsProvider> provider;
  SmsFetcherImpl fetcher(web_contents()->GetBrowserContext(), &provider);
  mojo::Remote<blink::mojom::SmsReceiver> service;
  SmsService::Create(&fetcher, main_rfh(),
                     service.BindNewPipeAndPassReceiver());

  base::RunLoop navigate;

  EXPECT_CALL(provider, Retrieve(_)).WillOnce(Invoke([&navigate]() {
    navigate.Quit();
  }));

  base::RunLoop reload;

  service->Receive(base::BindLambdaForTesting(
      [&reload](SmsStatus status, const Optional<string>& otp) {
        EXPECT_EQ(SmsStatus::kTimeout, status);
        EXPECT_EQ(base::nullopt, otp);
        reload.Quit();
      }));

  navigate.Run();

  // Simulates the user re-navigating to an existing history page.
  NavigationSimulator::GoBack(web_contents());

  reload.Run();

  ExpectDestroyedReasonCount(SmsReceiverDestroyedReason::kNavigateExistingPage,
                             1);
}

}  // namespace content
