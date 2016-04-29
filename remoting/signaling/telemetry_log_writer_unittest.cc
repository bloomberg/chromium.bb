// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/telemetry_log_writer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "net/http/http_status_code.h"
#include "remoting/base/url_request.h"
#include "remoting/signaling/chromoting_event.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

class FakeUrlRequest : public UrlRequest {
 public:
  FakeUrlRequest(const std::string& expected_post,
                 const UrlRequest::Result& returned_result)
      : expected_post_(expected_post), returned_result_(returned_result) {}

  void Respond() { on_result_callback_.Run(returned_result_); }

  // UrlRequest overrides.
  void SetPostData(const std::string& content_type,
                   const std::string& post_data) override {
    EXPECT_EQ(content_type, "application/json");
    EXPECT_EQ(post_data, expected_post_);
  }

  void AddHeader(const std::string& value) override {}

  void Start(const OnResultCallback& on_result_callback) override {
    on_result_callback_ = on_result_callback;
  }

 private:
  std::string expected_post_;
  UrlRequest::Result returned_result_;
  OnResultCallback on_result_callback_;

  DISALLOW_COPY_AND_ASSIGN(FakeUrlRequest);
};

class FakeUrlRequestFactory : public UrlRequestFactory {
 public:
  ~FakeUrlRequestFactory() override { EXPECT_TRUE(expected_requests_.empty()); }

  // Returns a respond closure. Run this closure to respond to the URL request.
  base::Closure AddExpectedRequest(const std::string& exp_post,
                                   const UrlRequest::Result& ret_result) {
    FakeUrlRequest* fakeRequest = new FakeUrlRequest(exp_post, ret_result);
    base::Closure closure =
        base::Bind(&FakeUrlRequest::Respond, base::Unretained(fakeRequest));
    expected_requests_.push_back(std::unique_ptr<UrlRequest>(fakeRequest));
    return closure;
  }

  // request_factory_ override.
  std::unique_ptr<UrlRequest> CreateUrlRequest(
      UrlRequest::Type type,
      const std::string& url) override {
    EXPECT_FALSE(expected_requests_.empty());
    if (expected_requests_.empty()) {
      return std::unique_ptr<UrlRequest>(nullptr);
    }
    EXPECT_EQ(type, UrlRequest::Type::POST);
    std::unique_ptr<UrlRequest> request(std::move(expected_requests_.front()));
    expected_requests_.pop_front();
    return request;
  }

 private:
  std::deque<std::unique_ptr<UrlRequest>> expected_requests_;
};

class TelemetryLogWriterTest : public testing::Test {
 public:
  TelemetryLogWriterTest()
      : request_factory_(new FakeUrlRequestFactory()),
        log_writer_("", base::WrapUnique(request_factory_)) {
    success_result_.success = true;
    success_result_.status = 200;
    success_result_.response_body = "{}";

    unauth_result_.success = false;
    unauth_result_.status = net::HTTP_UNAUTHORIZED;
    unauth_result_.response_body = "{}";
  }

 protected:
  void LogFakeEvent() {
    ChromotingEvent entry;
    entry.SetInteger("id", id_);
    id_++;
    log_writer_.Log(entry);
  }

  void SetAuthClosure() {
    log_writer_.SetAuthClosure(
        base::Bind(&TelemetryLogWriterTest::SetAuth, base::Unretained(this)));
  }

  UrlRequest::Result success_result_;
  UrlRequest::Result unauth_result_;

  FakeUrlRequestFactory* request_factory_;  // For peeking. No ownership.
  TelemetryLogWriter log_writer_;

  int set_auth_count_ = 0;

 private:
  void SetAuth() {
    set_auth_count_++;
    log_writer_.SetAuthToken("some token");
  }

  int id_ = 0;
};

// Test workflow: add request -> log event -> respond request.
// Test fails if req is incorrect or creates more/less reqs than expected
TEST_F(TelemetryLogWriterTest, PostOneLogImmediately) {
  auto respond = request_factory_->AddExpectedRequest(
      "{\"event\":[{\"id\":0}]}", success_result_);
  LogFakeEvent();
  respond.Run();
}

TEST_F(TelemetryLogWriterTest, PostOneLogAndHaveTwoPendingLogs) {
  auto respond1 = request_factory_->AddExpectedRequest(
      "{\"event\":[{\"id\":0}]}", success_result_);
  LogFakeEvent();

  auto respond2 = request_factory_->AddExpectedRequest(
      "{\"event\":[{\"id\":1},{\"id\":2}]}", success_result_);
  LogFakeEvent();
  LogFakeEvent();
  respond1.Run();
  respond2.Run();
}

TEST_F(TelemetryLogWriterTest, PostLogFailedAndRetry) {
  // kMaxTries = 5
  auto respond1 = request_factory_->AddExpectedRequest(
      "{\"event\":[{\"id\":0}]}", UrlRequest::Result::Failed());
  auto respond2 = request_factory_->AddExpectedRequest(
      "{\"event\":[{\"id\":0}]}", UrlRequest::Result::Failed());
  auto respond3 = request_factory_->AddExpectedRequest(
      "{\"event\":[{\"id\":0}]}", UrlRequest::Result::Failed());
  auto respond4 = request_factory_->AddExpectedRequest(
      "{\"event\":[{\"id\":0}]}", UrlRequest::Result::Failed());
  auto respond5 = request_factory_->AddExpectedRequest(
      "{\"event\":[{\"id\":0}]}", UrlRequest::Result::Failed());

  LogFakeEvent();

  respond1.Run();
  respond2.Run();
  respond3.Run();
  respond4.Run();
  respond5.Run();
}

TEST_F(TelemetryLogWriterTest, PostOneLogFailedResendWithTwoPendingLogs) {
  auto respond1 = request_factory_->AddExpectedRequest(
      "{\"event\":[{\"id\":0}]}", UrlRequest::Result::Failed());
  LogFakeEvent();

  auto respond2 = request_factory_->AddExpectedRequest(
      "{\"event\":[{\"id\":1},{\"id\":2},{\"id\":0}]}", success_result_);
  LogFakeEvent();
  LogFakeEvent();

  respond1.Run();
  respond2.Run();
}

TEST_F(TelemetryLogWriterTest, PostOneUnauthorizedCallClosureAndRetry) {
  SetAuthClosure();

  auto respond1 = request_factory_->AddExpectedRequest(
      "{\"event\":[{\"id\":0}]}", unauth_result_);
  LogFakeEvent();

  auto respond2 = request_factory_->AddExpectedRequest(
      "{\"event\":[{\"id\":0}]}", success_result_);
  respond1.Run();
  respond2.Run();

  EXPECT_EQ(1, set_auth_count_);
}

TEST_F(TelemetryLogWriterTest, PostOneUnauthorizedAndJustRetry) {
  auto respond1 = request_factory_->AddExpectedRequest(
      "{\"event\":[{\"id\":0}]}", unauth_result_);
  LogFakeEvent();

  auto respond2 = request_factory_->AddExpectedRequest(
      "{\"event\":[{\"id\":0}]}", success_result_);
  respond1.Run();
  respond2.Run();

  EXPECT_EQ(0, set_auth_count_);
}

}  // namespace remoting
