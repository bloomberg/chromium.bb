// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/certificate_report_sender.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "net/base/load_flags.h"
#include "net/base/network_delegate_impl.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/base/upload_data_stream.h"
#include "net/base/upload_element_reader.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/test/url_request/url_request_mock_data_job.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace {

const char kDummyReport[] = "foo.test";
const char kSecondDummyReport[] = "foo2.test";

void MarkURLRequestDestroyed(bool* url_request_destroyed) {
  *url_request_destroyed = true;
}

// Checks that data uploaded in the request matches the test report
// data. Erases the sent reports from |expect_reports|.
void CheckUploadData(const URLRequest& request,
                     std::set<std::string>* expect_reports) {
  const UploadDataStream* upload = request.get_upload();
  ASSERT_TRUE(upload);
  ASSERT_TRUE(upload->GetElementReaders());
  ASSERT_EQ(1u, upload->GetElementReaders()->size());

  const UploadBytesElementReader* reader =
      (*upload->GetElementReaders())[0]->AsBytesReader();
  ASSERT_TRUE(reader);
  std::string upload_data(reader->bytes(), reader->length());

  EXPECT_EQ(1u, expect_reports->erase(upload_data));
}

// A network delegate that lets tests check that a certificate report
// was sent. It counts the number of requests and lets tests register a
// callback to run when the request is destroyed. It also checks that
// the uploaded data is as expected.
class TestCertificateReportSenderNetworkDelegate : public NetworkDelegateImpl {
 public:
  TestCertificateReportSenderNetworkDelegate()
      : url_request_destroyed_callback_(base::Bind(&base::DoNothing)),
        all_url_requests_destroyed_callback_(base::Bind(&base::DoNothing)),
        num_requests_(0),
        expect_cookies_(false) {}

  void ExpectReport(const std::string& report) {
    expect_reports_.insert(report);
  }

  void set_all_url_requests_destroyed_callback(const base::Closure& callback) {
    all_url_requests_destroyed_callback_ = callback;
  }

  void set_url_request_destroyed_callback(const base::Closure& callback) {
    url_request_destroyed_callback_ = callback;
  }

  void set_expect_url(const GURL& expect_url) { expect_url_ = expect_url; }

  size_t num_requests() const { return num_requests_; }

  // Sets whether cookies are expected to be sent on requests.
  void set_expect_cookies(bool expect_cookies) {
    expect_cookies_ = expect_cookies;
  }

  // NetworkDelegateImpl implementation.
  int OnBeforeURLRequest(URLRequest* request,
                         const CompletionCallback& callback,
                         GURL* new_url) override {
    num_requests_++;
    EXPECT_EQ(expect_url_, request->url());
    EXPECT_STRCASEEQ("POST", request->method().data());

    if (expect_cookies_) {
      EXPECT_FALSE(request->load_flags() & LOAD_DO_NOT_SEND_COOKIES);
      EXPECT_FALSE(request->load_flags() & LOAD_DO_NOT_SAVE_COOKIES);
    } else {
      EXPECT_TRUE(request->load_flags() & LOAD_DO_NOT_SEND_COOKIES);
      EXPECT_TRUE(request->load_flags() & LOAD_DO_NOT_SAVE_COOKIES);
    }

    CheckUploadData(*request, &expect_reports_);

    // Unconditionally return OK, since the sender ignores the results
    // anyway.
    return OK;
  }

  void OnURLRequestDestroyed(URLRequest* request) override {
    url_request_destroyed_callback_.Run();
    if (expect_reports_.empty())
      all_url_requests_destroyed_callback_.Run();
  }

 private:
  base::Closure url_request_destroyed_callback_;
  base::Closure all_url_requests_destroyed_callback_;
  size_t num_requests_;
  GURL expect_url_;
  std::set<std::string> expect_reports_;
  bool expect_cookies_;

  DISALLOW_COPY_AND_ASSIGN(TestCertificateReportSenderNetworkDelegate);
};

class CertificateReportSenderTest : public ::testing::Test {
 public:
  CertificateReportSenderTest() : context_(true) {
    context_.set_network_delegate(&network_delegate_);
    context_.Init();
  }

  void SetUp() override {
    URLRequestFailedJob::AddUrlHandler();
    URLRequestMockDataJob::AddUrlHandler();
  }

  void TearDown() override { URLRequestFilter::GetInstance()->ClearHandlers(); }

  TestURLRequestContext* context() { return &context_; }

 protected:
  void SendReport(CertificateReportSender* reporter,
                  const std::string& report,
                  const GURL& url,
                  size_t request_sequence_number) {
    base::RunLoop run_loop;
    network_delegate_.set_url_request_destroyed_callback(
        run_loop.QuitClosure());

    network_delegate_.set_expect_url(url);
    network_delegate_.ExpectReport(report);

    EXPECT_EQ(request_sequence_number, network_delegate_.num_requests());

    reporter->Send(url, report);

    // The report is sent asynchronously, so wait for the report's
    // URLRequest to be destroyed before checking that the report was
    // sent.
    run_loop.Run();

    EXPECT_EQ(request_sequence_number + 1, network_delegate_.num_requests());
  }

  TestCertificateReportSenderNetworkDelegate network_delegate_;

 private:
  TestURLRequestContext context_;
};

// Test that CertificateReportSender::Send creates a URLRequest for the
// endpoint and sends the expected data.
TEST_F(CertificateReportSenderTest, SendsRequest) {
  GURL url = URLRequestMockDataJob::GetMockHttpsUrl("dummy data", 1);
  CertificateReportSender reporter(
      context(), CertificateReportSender::DO_NOT_SEND_COOKIES);
  SendReport(&reporter, kDummyReport, url, 0);
}

TEST_F(CertificateReportSenderTest, SendMultipleReportsSequentially) {
  GURL url = URLRequestMockDataJob::GetMockHttpsUrl("dummy data", 1);
  CertificateReportSender reporter(
      context(), CertificateReportSender::DO_NOT_SEND_COOKIES);
  SendReport(&reporter, kDummyReport, url, 0);
  SendReport(&reporter, kDummyReport, url, 1);
}

TEST_F(CertificateReportSenderTest, SendMultipleReportsSimultaneously) {
  base::RunLoop run_loop;
  network_delegate_.set_all_url_requests_destroyed_callback(
      run_loop.QuitClosure());

  GURL url = URLRequestMockDataJob::GetMockHttpsUrl("dummy data", 1);
  network_delegate_.set_expect_url(url);
  network_delegate_.ExpectReport(kDummyReport);
  network_delegate_.ExpectReport(kSecondDummyReport);

  CertificateReportSender reporter(
      context(), CertificateReportSender::DO_NOT_SEND_COOKIES);

  EXPECT_EQ(0u, network_delegate_.num_requests());

  reporter.Send(url, kDummyReport);
  reporter.Send(url, kSecondDummyReport);

  run_loop.Run();

  EXPECT_EQ(2u, network_delegate_.num_requests());
}

// Test that pending URLRequests get cleaned up when the report sender
// is deleted.
TEST_F(CertificateReportSenderTest, PendingRequestGetsDeleted) {
  bool url_request_destroyed = false;
  network_delegate_.set_url_request_destroyed_callback(base::Bind(
      &MarkURLRequestDestroyed, base::Unretained(&url_request_destroyed)));

  GURL url = URLRequestFailedJob::GetMockHttpUrlWithFailurePhase(
      URLRequestFailedJob::START, ERR_IO_PENDING);
  network_delegate_.set_expect_url(url);
  network_delegate_.ExpectReport(kDummyReport);

  EXPECT_EQ(0u, network_delegate_.num_requests());

  scoped_ptr<CertificateReportSender> reporter(new CertificateReportSender(
      context(), CertificateReportSender::DO_NOT_SEND_COOKIES));
  reporter->Send(url, kDummyReport);
  reporter.reset();

  EXPECT_EQ(1u, network_delegate_.num_requests());
  EXPECT_TRUE(url_request_destroyed);
}

// Test that a request that returns an error gets cleaned up.
TEST_F(CertificateReportSenderTest, ErroredRequestGetsDeleted) {
  GURL url = URLRequestFailedJob::GetMockHttpsUrl(ERR_FAILED);
  CertificateReportSender reporter(
      context(), CertificateReportSender::DO_NOT_SEND_COOKIES);
  // SendReport will block until the URLRequest is destroyed.
  SendReport(&reporter, kDummyReport, url, 0);
}

// Test that cookies are sent or not sent according to the error
// reporter's cookies preference.

TEST_F(CertificateReportSenderTest, SendCookiesPreference) {
  GURL url = URLRequestMockDataJob::GetMockHttpsUrl("dummy data", 1);
  CertificateReportSender reporter(context(),
                                   CertificateReportSender::SEND_COOKIES);

  network_delegate_.set_expect_cookies(true);
  SendReport(&reporter, kDummyReport, url, 0);
}

TEST_F(CertificateReportSenderTest, DoNotSendCookiesPreference) {
  GURL url = URLRequestMockDataJob::GetMockHttpsUrl("dummy data", 1);
  CertificateReportSender reporter(
      context(), CertificateReportSender::DO_NOT_SEND_COOKIES);

  network_delegate_.set_expect_cookies(false);
  SendReport(&reporter, kDummyReport, url, 0);
}

}  // namespace
}  // namespace net
