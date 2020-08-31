// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/browser/browser_main_loop.h"
#include "content/browser/sms/sms_fetcher_impl.h"
#include "content/browser/sms/sms_service.h"
#include "content/browser/sms/test/mock_sms_provider.h"
#include "content/browser/sms/test/mock_sms_web_contents_delegate.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/shell/browser/shell.h"
#include "net/cert/mock_cert_verifier.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/common/sms/sms_receiver_outcome.h"

using blink::mojom::SmsStatus;
using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::Return;

namespace content {

namespace {

class SmsBrowserTest : public ContentBrowserTest {
 public:
  using Entry = ukm::builders::SMSReceiver;

  SmsBrowserTest() = default;
  ~SmsBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    ContentBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "SmsReceiver");
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    command_line->AppendSwitchASCII(switches::kWebOtpBackend,
                                    switches::kWebOtpBackendSmsVerification);
    cert_verifier_.SetUpCommandLine(command_line);
  }

  void ExpectOutcomeUKM(const GURL& url, blink::SMSReceiverOutcome outcome) {
    auto entries = ukm_recorder()->GetEntriesByName(Entry::kEntryName);

    if (entries.empty())
      FAIL() << "No SMSReceiverOutcome was recorded";

    for (const auto* const entry : entries) {
      const int64_t* metric = ukm_recorder()->GetEntryMetric(entry, "Outcome");
      if (*metric == static_cast<int>(outcome)) {
        SUCCEED();
        return;
      }
    }
    FAIL() << "Expected SMSReceiverOutcome was not recorded";
  }

  void ExpectNoOutcomeUKM() {
    EXPECT_TRUE(ukm_recorder()->GetEntriesByName(Entry::kEntryName).empty());
  }

  void ExpectSmsPrompt() {
    EXPECT_CALL(delegate_, CreateSmsPrompt(_, _, _, _, _))
        .WillOnce(Invoke([&](RenderFrameHost*, const url::Origin&,
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

 protected:
  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);
    ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  void SetUpInProcessBrowserTestFixture() override {
    cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

  SmsFetcher* GetSmsFetcher() {
    return SmsFetcher::Get(shell()->web_contents()->GetBrowserContext());
  }

  ukm::TestAutoSetUkmRecorder* ukm_recorder() { return ukm_recorder_.get(); }

  NiceMock<MockSmsWebContentsDelegate> delegate_;
  // Similar to net::MockCertVerifier, but also updates the CertVerifier
  // used by the NetworkService.
  ContentMockCertVerifier cert_verifier_;

  std::unique_ptr<ukm::TestAutoSetUkmRecorder> ukm_recorder_;

  base::OnceClosure confirm_callback_;
  base::OnceClosure dismiss_callback_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, Receive) {
  base::HistogramTester histogram_tester;
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  shell()->web_contents()->SetDelegate(&delegate_);

  ExpectSmsPrompt();

  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  // Test that SMS content can be retrieved after navigator.credentials.get().
  std::string script = R"(
    (async () => {
      let cred = await navigator.credentials.get({otp: {transport: ["sms"]}});
      return cred.code;
    }) ();
  )";

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_)).WillOnce(Invoke([&]() {
    mock_provider_ptr->NotifyReceive(url::Origin::Create(url), "hello");
    ConfirmPrompt();
  }));

  // Wait for UKM to be recorded to avoid race condition between outcome
  // capture and evaluation.
  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop.QuitClosure());

  EXPECT_EQ("hello", EvalJs(shell(), script));

  ukm_loop.Run();

  ASSERT_FALSE(GetSmsFetcher()->HasSubscribers());

  content::FetchHistogramsFromChildProcesses();
  ExpectOutcomeUKM(url, blink::SMSReceiverOutcome::kSuccess);
  histogram_tester.ExpectTotalCount("Blink.Sms.Receive.TimeSuccess", 1);
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, AtMostOneSmsRequestPerOrigin) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  shell()->web_contents()->SetDelegate(&delegate_);

  ExpectSmsPrompt();

  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  std::string script = R"(
    (async () => {
      let firstRequest = navigator.credentials.get({otp: {transport: ["sms"]}})
        .catch(({name}) => {
          return name;
        });
      let secondRequest = navigator.credentials.get({otp: {transport: ["sms"]}})
        .then(({code}) => {
          return code;
        });
      return Promise.all([firstRequest, secondRequest]);
    }) ();
  )";

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_))
      .WillOnce(Return())
      .WillOnce(Invoke([&]() {
        mock_provider_ptr->NotifyReceive(url::Origin::Create(url), "hello");
        ConfirmPrompt();
      }));

  // Wait for UKM to be recorded to avoid race condition between outcome
  // capture and evaluation.
  base::RunLoop ukm_loop1;
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop1.QuitClosure());

  EXPECT_EQ(ListValueOf("AbortError", "hello"), EvalJs(shell(), script));

  ukm_loop1.Run();

  ASSERT_FALSE(GetSmsFetcher()->HasSubscribers());

  ExpectOutcomeUKM(url, blink::SMSReceiverOutcome::kSuccess);
}

// Disabled test: https://crbug.com/1052385
IN_PROC_BROWSER_TEST_F(SmsBrowserTest,
                       DISABLED_AtMostOneSmsRequestPerOriginPerTab) {
  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  Shell* tab1 = CreateBrowser();
  Shell* tab2 = CreateBrowser();

  GURL url = GetTestUrl(nullptr, "simple_page.html");

  EXPECT_TRUE(NavigateToURL(tab1, url));
  EXPECT_TRUE(NavigateToURL(tab2, url));

  tab1->web_contents()->SetDelegate(&delegate_);
  tab2->web_contents()->SetDelegate(&delegate_);

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_)).Times(3);

  // Make 1 request on tab1 that is expected to be cancelled when the 2nd
  // request is made.
  EXPECT_TRUE(ExecJs(tab1, R"(
       var firstRequest = navigator.credentials.get({otp: {transport: ["sms"]}})
         .catch(({name}) => {
           return name;
         });
     )"));

  // Make 1 request on tab2 to verify requests on tab1 have no affect on tab2.
  EXPECT_TRUE(ExecJs(tab2, R"(
        var request = navigator.credentials.get({otp: {transport: ["sms"]}})
          .then(({code}) => {
            return code;
          });
     )"));

  // Make a 2nd request on tab1 to verify the 1st request gets cancelled when
  // the 2nd request is made.
  EXPECT_TRUE(ExecJs(tab1, R"(
        var secondRequest = navigator.credentials
          .get({otp: {transport: ["sms"]}})
          .then(({code}) => {
            return code;
          });
     )"));

  EXPECT_EQ("AbortError", EvalJs(tab1, "firstRequest"));

  ASSERT_TRUE(GetSmsFetcher()->HasSubscribers());

  {
    base::RunLoop ukm_loop;

    ExpectSmsPrompt();

    // Wait for UKM to be recorded to avoid race condition between outcome
    // capture and evaluation.
    ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                          ukm_loop.QuitClosure());

    mock_provider_ptr->NotifyReceive(url::Origin::Create(url), "hello1");
    ConfirmPrompt();

    ukm_loop.Run();
  }

  EXPECT_EQ("hello1", EvalJs(tab2, "request"));

  ASSERT_TRUE(GetSmsFetcher()->HasSubscribers());

  ExpectOutcomeUKM(url, blink::SMSReceiverOutcome::kSuccess);

  ukm_recorder()->Purge();

  {
    base::RunLoop ukm_loop;

    ExpectSmsPrompt();

    // Wait for UKM to be recorded to avoid race condition between outcome
    // capture and evaluation.
    ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                          ukm_loop.QuitClosure());

    mock_provider_ptr->NotifyReceive(url::Origin::Create(url), "hello2");
    ConfirmPrompt();

    ukm_loop.Run();
  }

  EXPECT_EQ("hello2", EvalJs(tab1, "secondRequest"));

  ASSERT_FALSE(GetSmsFetcher()->HasSubscribers());

  ExpectOutcomeUKM(url, blink::SMSReceiverOutcome::kSuccess);
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, Reload) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  std::string script = R"(
    // kicks off the sms receiver, adding the service
    // to the observer's list.
    navigator.credentials.get({otp: {transport: ["sms"]}});
  )";

  base::RunLoop loop;

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_)).WillOnce(Invoke([&loop]() {
    // Deliberately avoid calling NotifyReceive() to simulate
    // a request that has been received but not fulfilled.
    loop.Quit();
  }));

  EXPECT_TRUE(ExecJs(shell(), script));

  loop.Run();

  ASSERT_TRUE(GetSmsFetcher()->HasSubscribers());

  // Wait for UKM to be recorded to avoid race condition between outcome
  // capture and evaluation.
  base::RunLoop ukm_loop;
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop.QuitClosure());

  // Reload the page.
  EXPECT_TRUE(NavigateToURL(shell(), url));

  ukm_loop.Run();

  ASSERT_FALSE(GetSmsFetcher()->HasSubscribers());

  ExpectOutcomeUKM(url, blink::SMSReceiverOutcome::kTimeout);
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, Close) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  base::RunLoop loop;

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_)).WillOnce(Invoke([&loop]() {
    loop.Quit();
  }));

  EXPECT_TRUE(ExecJs(shell(), R"(
    navigator.credentials.get({otp: {transport: ["sms"]}});
  )"));

  loop.Run();

  ASSERT_TRUE(mock_provider_ptr->HasObservers());

  auto* fetcher = GetSmsFetcher();

  shell()->Close();

  ASSERT_FALSE(fetcher->HasSubscribers());

  ExpectNoOutcomeUKM();
}

// Disabled test: https://crbug.com/1052385
IN_PROC_BROWSER_TEST_F(SmsBrowserTest, DISABLED_TwoTabsSameOrigin) {
  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  Shell* tab1 = CreateBrowser();
  Shell* tab2 = CreateBrowser();

  GURL url = GetTestUrl(nullptr, "simple_page.html");

  EXPECT_TRUE(NavigateToURL(tab1, url));
  EXPECT_TRUE(NavigateToURL(tab2, url));

  tab1->web_contents()->SetDelegate(&delegate_);
  tab2->web_contents()->SetDelegate(&delegate_);

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_)).Times(2);

  std::string script = R"(
    var otp = navigator.credentials.get({otp: {transport: ["sms"]}})
      .then(({code}) => {
        return code;
      });
  )";

  // First tab registers an observer.
  EXPECT_TRUE(ExecJs(tab1, script));

  // Second tab registers an observer.
  EXPECT_TRUE(ExecJs(tab2, script));

  ASSERT_TRUE(mock_provider_ptr->HasObservers());

  {
    base::RunLoop ukm_loop;

    ExpectSmsPrompt();

    // Wait for UKM to be recorded to avoid race condition between outcome
    // capture and evaluation.
    ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                          ukm_loop.QuitClosure());

    mock_provider_ptr->NotifyReceive(url::Origin::Create(url), "hello1");
    ConfirmPrompt();

    ukm_loop.Run();
  }

  EXPECT_EQ("hello1", EvalJs(tab1, "otp"));

  ASSERT_TRUE(mock_provider_ptr->HasObservers());

  ExpectOutcomeUKM(url, blink::SMSReceiverOutcome::kSuccess);

  ukm_recorder()->Purge();

  {
    base::RunLoop ukm_loop;

    ExpectSmsPrompt();

    // Wait for UKM to be recorded to avoid race condition between outcome
    // capture and evaluation.
    ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                          ukm_loop.QuitClosure());

    mock_provider_ptr->NotifyReceive(url::Origin::Create(url), "hello2");
    ConfirmPrompt();

    ukm_loop.Run();
  }

  EXPECT_EQ("hello2", EvalJs(tab2, "otp"));

  ASSERT_FALSE(GetSmsFetcher()->HasSubscribers());

  ExpectOutcomeUKM(url, blink::SMSReceiverOutcome::kSuccess);
}

// Disabled test: https://crbug.com/1052385
IN_PROC_BROWSER_TEST_F(SmsBrowserTest, DISABLED_TwoTabsDifferentOrigin) {
  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  Shell* tab1 = CreateBrowser();
  Shell* tab2 = CreateBrowser();

  net::EmbeddedTestServer https_server(net::EmbeddedTestServer::TYPE_HTTPS);
  https_server.ServeFilesFromSourceDirectory(GetTestDataFilePath());
  ASSERT_TRUE(https_server.Start());

  GURL url1 = https_server.GetURL("a.com", "/simple_page.html");
  GURL url2 = https_server.GetURL("b.com", "/simple_page.html");

  EXPECT_TRUE(NavigateToURL(tab1, url1));
  EXPECT_TRUE(NavigateToURL(tab2, url2));

  std::string script = R"(
    var otp = navigator.credentials.get({otp: {transport: ["sms"]}})
      .then(({code}) => {
        return code;
      });
  )";

  base::RunLoop loop;

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_)).Times(2);

  tab1->web_contents()->SetDelegate(&delegate_);
  tab2->web_contents()->SetDelegate(&delegate_);

  EXPECT_TRUE(ExecJs(tab1, script));
  EXPECT_TRUE(ExecJs(tab2, script));

  ASSERT_TRUE(mock_provider_ptr->HasObservers());

  {
    base::RunLoop ukm_loop;
    ExpectSmsPrompt();
    // Wait for UKM to be recorded to avoid race condition between outcome
    // capture and evaluation.
    ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                          ukm_loop.QuitClosure());
    mock_provider_ptr->NotifyReceive(url::Origin::Create(url1), "hello1");
    ConfirmPrompt();
    ukm_loop.Run();
  }

  EXPECT_EQ("hello1", EvalJs(tab1, "otp"));

  ASSERT_TRUE(mock_provider_ptr->HasObservers());

  {
    base::RunLoop ukm_loop;
    ExpectSmsPrompt();
    // Wait for UKM to be recorded to avoid race condition between outcome
    // capture and evaluation.
    ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                          ukm_loop.QuitClosure());
    mock_provider_ptr->NotifyReceive(url::Origin::Create(url2), "hello2");
    ConfirmPrompt();
    ukm_loop.Run();
  }

  EXPECT_EQ("hello2", EvalJs(tab2, "otp"));

  ASSERT_FALSE(GetSmsFetcher()->HasSubscribers());

  ExpectOutcomeUKM(url1, blink::SMSReceiverOutcome::kSuccess);
  ExpectOutcomeUKM(url2, blink::SMSReceiverOutcome::kSuccess);
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, SmsReceivedAfterTabIsClosed) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  base::RunLoop loop;

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_)).WillOnce(Invoke([&loop]() {
    loop.Quit();
  }));

  EXPECT_TRUE(ExecJs(shell(), R"(
      // kicks off an sms receiver call, but deliberately leaves it hanging.
      navigator.credentials.get({otp: {transport: ["sms"]}});
    )"));

  loop.Run();

  shell()->Close();

  mock_provider_ptr->NotifyReceive(url::Origin::Create(url), "hello");

  ExpectNoOutcomeUKM();
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, Cancels) {
  base::HistogramTester histogram_tester;
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  shell()->web_contents()->SetDelegate(&delegate_);

  base::RunLoop ukm_loop;

  ExpectSmsPrompt();

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_)).WillOnce(Invoke([&]() {
    mock_provider_ptr->NotifyReceive(url::Origin::Create(url), "hello");
    DismissPrompt();
  }));

  // Wait for UKM to be recorded to avoid race condition between outcome
  // capture and evaluation.
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop.QuitClosure());

  EXPECT_TRUE(ExecJs(shell(), R"(
     var error = navigator.credentials.get({otp: {transport: ["sms"]}})
       .catch(({name}) => {
         return name;
       });
    )"));

  ukm_loop.Run();

  EXPECT_EQ("AbortError", EvalJs(shell(), "error"));

  content::FetchHistogramsFromChildProcesses();
  ExpectOutcomeUKM(url, blink::SMSReceiverOutcome::kCancelled);
  histogram_tester.ExpectTotalCount("Blink.Sms.Receive.TimeCancel", 1);
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, AbortAfterSmsRetrieval) {
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  auto provider = std::make_unique<MockSmsProvider>();
  MockSmsProvider* mock_provider_ptr = provider.get();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(std::move(provider));

  shell()->web_contents()->SetDelegate(&delegate_);

  ExpectSmsPrompt();

  EXPECT_CALL(*mock_provider_ptr, Retrieve(_))
      .WillOnce(Invoke([&mock_provider_ptr, &url]() {
        mock_provider_ptr->NotifyReceive(url::Origin::Create(url), "hello");
      }));

  EXPECT_TRUE(ExecJs(shell(), R"(
       var controller = new AbortController();
       var signal = controller.signal;
       var request = navigator.credentials
         .get({otp: {transport: ["sms"]}, signal: signal})
         .catch(({name}) => {
           return name;
         });
     )"));

  base::RunLoop ukm_loop;

  // Wait for UKM to be recorded to avoid race condition between outcome
  // capture and evaluation.
  ukm_recorder()->SetOnAddEntryCallback(Entry::kEntryName,
                                        ukm_loop.QuitClosure());

  EXPECT_TRUE(ExecJs(shell(), R"( controller.abort(); )"));

  ukm_loop.Run();

  EXPECT_EQ("AbortError", EvalJs(shell(), "request"));

  ExpectOutcomeUKM(url, blink::SMSReceiverOutcome::kAborted);
}

IN_PROC_BROWSER_TEST_F(SmsBrowserTest, SmsFetcherUAF) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kWebOtpBackend, switches::kWebOtpBackendUserConsent);
  GURL url = GetTestUrl(nullptr, "simple_page.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));

  auto* provider = new NiceMock<MockSmsProvider>();
  BrowserMainLoop::GetInstance()->SetSmsProviderForTesting(
      base::WrapUnique(provider));

  shell()->web_contents()->SetDelegate(&delegate_);

  auto* fetcher = SmsFetcher::Get(shell()->web_contents()->GetBrowserContext());
  auto* fetcher2 =
      SmsFetcher::Get(shell()->web_contents()->GetBrowserContext());
  mojo::Remote<blink::mojom::SmsReceiver> service;
  mojo::Remote<blink::mojom::SmsReceiver> service2;

  RenderFrameHost* render_frame_host = shell()->web_contents()->GetMainFrame();
  SmsService::Create(fetcher, render_frame_host,
                     service.BindNewPipeAndPassReceiver());
  SmsService::Create(fetcher2, render_frame_host,
                     service2.BindNewPipeAndPassReceiver());

  base::RunLoop navigate;

  EXPECT_CALL(*provider, Retrieve(_))
      .WillOnce(Invoke([&]() {
        static_cast<SmsFetcherImpl*>(fetcher)->OnReceive(
            url::Origin::Create(url), "ABC234");
      }))
      .WillOnce(Invoke([&]() {
        static_cast<SmsFetcherImpl*>(fetcher2)->OnReceive(
            url::Origin::Create(url), "DEF567");
      }));

  service->Receive(base::BindLambdaForTesting(
      [](SmsStatus status, const base::Optional<std::string>& otp) {
        EXPECT_EQ(SmsStatus::kSuccess, status);
        EXPECT_EQ("ABC234", otp);
      }));

  service2->Receive(base::BindLambdaForTesting(
      [&navigate](SmsStatus status, const base::Optional<std::string>& otp) {
        EXPECT_EQ(SmsStatus::kSuccess, status);
        EXPECT_EQ("DEF567", otp);
        navigate.Quit();
      }));

  navigate.Run();
}

}  // namespace content
