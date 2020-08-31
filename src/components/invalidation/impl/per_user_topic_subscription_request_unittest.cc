// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/per_user_topic_subscription_request.h"

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

using testing::_;
using testing::SaveArg;

MATCHER_P(EqualsJSON, json, "equals JSON") {
  std::unique_ptr<base::Value> expected =
      base::JSONReader::ReadDeprecated(json);
  if (!expected) {
    *result_listener << "INTERNAL ERROR: couldn't parse expected JSON";
    return false;
  }

  std::string err_msg;
  int err_line, err_col;
  std::unique_ptr<base::Value> actual =
      base::JSONReader::ReadAndReturnErrorDeprecated(
          arg, base::JSON_PARSE_RFC, nullptr, &err_msg, &err_line, &err_col);
  if (!actual) {
    *result_listener << "input:" << err_line << ":" << err_col << ": "
                     << "parse error: " << err_msg;
    return false;
  }
  return *expected == *actual;
}

network::mojom::URLResponseHeadPtr CreateHeadersForTest(int responce_code) {
  auto head = network::mojom::URLResponseHead::New();
  head->headers = new net::HttpResponseHeaders(base::StringPrintf(
      "HTTP/1.1 %d OK\nContent-type: text/html\n\n", responce_code));
  head->mime_type = "text/html";
  return head;
}

}  // namespace

class PerUserTopicSubscriptionRequestTest : public testing::Test {
 public:
  PerUserTopicSubscriptionRequestTest() {}

  GURL url(PerUserTopicSubscriptionRequest* request) {
    return request->GetUrlForTesting();
  }

  network::TestURLLoaderFactory* url_loader_factory() {
    return &url_loader_factory_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
  network::TestURLLoaderFactory url_loader_factory_;

  DISALLOW_COPY_AND_ASSIGN(PerUserTopicSubscriptionRequestTest);
};

TEST_F(PerUserTopicSubscriptionRequestTest,
       ShouldNotInvokeCallbackWhenCancelled) {
  std::string token = "1234567890";
  std::string url = "http://valid-url.test";
  std::string topic = "test";
  std::string project_id = "smarty-pants-12345";
  PerUserTopicSubscriptionRequest::RequestType type =
      PerUserTopicSubscriptionRequest::SUBSCRIBE;

  base::MockCallback<PerUserTopicSubscriptionRequest::CompletedCallback>
      callback;
  EXPECT_CALL(callback, Run(_, _)).Times(0);

  PerUserTopicSubscriptionRequest::Builder builder;
  std::unique_ptr<PerUserTopicSubscriptionRequest> request =
      builder.SetInstanceIdToken(token)
          .SetScope(url)
          .SetPublicTopicName(topic)
          .SetProjectId(project_id)
          .SetType(type)
          .Build();
  request->Start(callback.Get(), url_loader_factory());
  base::RunLoop().RunUntilIdle();

  // Destroy the request before getting any response.
  request.reset();
}

TEST_F(PerUserTopicSubscriptionRequestTest, ShouldSubscribeWithoutErrors) {
  std::string token = "1234567890";
  std::string base_url = "http://valid-url.test";
  std::string topic = "test";
  std::string project_id = "smarty-pants-12345";
  PerUserTopicSubscriptionRequest::RequestType type =
      PerUserTopicSubscriptionRequest::SUBSCRIBE;

  base::MockCallback<PerUserTopicSubscriptionRequest::CompletedCallback>
      callback;
  Status status(StatusCode::FAILED, "initial");
  std::string private_topic;
  EXPECT_CALL(callback, Run(_, _))
      .WillOnce(DoAll(SaveArg<0>(&status), SaveArg<1>(&private_topic)));

  PerUserTopicSubscriptionRequest::Builder builder;
  std::unique_ptr<PerUserTopicSubscriptionRequest> request =
      builder.SetInstanceIdToken(token)
          .SetScope(base_url)
          .SetPublicTopicName(topic)
          .SetProjectId(project_id)
          .SetType(type)
          .Build();
  std::string response_body = R"(
    {
      "privateTopicName": "test-pr"
    }
  )";

  network::URLLoaderCompletionStatus response_status(net::OK);
  response_status.decoded_body_length = response_body.size();

  url_loader_factory()->AddResponse(url(request.get()),
                                    CreateHeadersForTest(net::HTTP_OK),
                                    response_body, response_status);
  request->Start(callback.Get(), url_loader_factory());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(status.code, StatusCode::SUCCESS);
  EXPECT_EQ(private_topic, "test-pr");
}

TEST_F(PerUserTopicSubscriptionRequestTest,
       ShouleNotSubscribeWhenNetworkProblem) {
  std::string token = "1234567890";
  std::string base_url = "http://valid-url.test";
  std::string topic = "test";
  std::string project_id = "smarty-pants-12345";
  PerUserTopicSubscriptionRequest::RequestType type =
      PerUserTopicSubscriptionRequest::SUBSCRIBE;

  base::MockCallback<PerUserTopicSubscriptionRequest::CompletedCallback>
      callback;
  Status status(StatusCode::FAILED, "initial");
  std::string private_topic;
  EXPECT_CALL(callback, Run(_, _))
      .WillOnce(DoAll(SaveArg<0>(&status), SaveArg<1>(&private_topic)));

  PerUserTopicSubscriptionRequest::Builder builder;
  std::unique_ptr<PerUserTopicSubscriptionRequest> request =
      builder.SetInstanceIdToken(token)
          .SetScope(base_url)
          .SetPublicTopicName(topic)
          .SetProjectId(project_id)
          .SetType(type)
          .Build();
  std::string response_body = R"(
    {
      "privateTopicName": "test-pr"
    }
  )";

  network::URLLoaderCompletionStatus response_status(net::ERR_TIMED_OUT);
  response_status.decoded_body_length = response_body.size();

  url_loader_factory()->AddResponse(url(request.get()),
                                    CreateHeadersForTest(net::HTTP_OK),
                                    response_body, response_status);
  request->Start(callback.Get(), url_loader_factory());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(status.code, StatusCode::FAILED);
}

TEST_F(PerUserTopicSubscriptionRequestTest,
       ShouldNotSubscribeWhenWrongResponse) {
  std::string token = "1234567890";
  std::string base_url = "http://valid-url.test";
  std::string topic = "test";
  std::string project_id = "smarty-pants-12345";
  PerUserTopicSubscriptionRequest::RequestType type =
      PerUserTopicSubscriptionRequest::SUBSCRIBE;

  base::MockCallback<PerUserTopicSubscriptionRequest::CompletedCallback>
      callback;
  Status status(StatusCode::SUCCESS, "initial");
  std::string private_topic;

  EXPECT_CALL(callback, Run(_, _))
      .WillOnce(DoAll(SaveArg<0>(&status), SaveArg<1>(&private_topic)));

  PerUserTopicSubscriptionRequest::Builder builder;
  std::unique_ptr<PerUserTopicSubscriptionRequest> request =
      builder.SetInstanceIdToken(token)
          .SetScope(base_url)
          .SetPublicTopicName(topic)
          .SetProjectId(project_id)
          .SetType(type)
          .Build();
  std::string response_body = R"(
    {}
  )";

  network::URLLoaderCompletionStatus response_status(net::OK);
  response_status.decoded_body_length = response_body.size();

  url_loader_factory()->AddResponse(url(request.get()),
                                    CreateHeadersForTest(net::HTTP_OK),
                                    response_body, response_status);
  request->Start(callback.Get(), url_loader_factory());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(status.code, StatusCode::FAILED);
  EXPECT_EQ(status.message, "Missing topic name");
}

TEST_F(PerUserTopicSubscriptionRequestTest, ShouldUnsubscribe) {
  std::string token = "1234567890";
  std::string base_url = "http://valid-url.test";
  std::string topic = "test";
  std::string project_id = "smarty-pants-12345";
  PerUserTopicSubscriptionRequest::RequestType type =
      PerUserTopicSubscriptionRequest::UNSUBSCRIBE;

  base::MockCallback<PerUserTopicSubscriptionRequest::CompletedCallback>
      callback;
  Status status(StatusCode::FAILED, "initial");
  std::string private_topic;

  EXPECT_CALL(callback, Run(_, _))
      .WillOnce(DoAll(SaveArg<0>(&status), SaveArg<1>(&private_topic)));

  PerUserTopicSubscriptionRequest::Builder builder;
  std::unique_ptr<PerUserTopicSubscriptionRequest> request =
      builder.SetInstanceIdToken(token)
          .SetScope(base_url)
          .SetPublicTopicName(topic)
          .SetProjectId(project_id)
          .SetType(type)
          .Build();
  std::string response_body = R"(
    {}
  )";

  network::URLLoaderCompletionStatus response_status(net::OK);
  response_status.decoded_body_length = response_body.size();

  url_loader_factory()->AddResponse(url(request.get()),
                                    CreateHeadersForTest(net::HTTP_OK),
                                    response_body, response_status);
  request->Start(callback.Get(), url_loader_factory());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(status.code, StatusCode::SUCCESS);
  EXPECT_EQ(status.message, std::string());
}

// Regression test for crbug.com/1054590, |completed_callback| destroys
// |request|.
TEST_F(PerUserTopicSubscriptionRequestTest, ShouldDestroyOnFailure) {
  std::string token = "1234567890";
  std::string base_url = "http://valid-url.test";
  std::string topic = "test";
  std::string project_id = "smarty-pants-12345";
  PerUserTopicSubscriptionRequest::RequestType type =
      PerUserTopicSubscriptionRequest::SUBSCRIBE;

  std::unique_ptr<PerUserTopicSubscriptionRequest> request;
  bool callback_called = false;
  auto completed_callback = base::BindLambdaForTesting(
      [&](const Status& status, const std::string& topic_name) {
        request.reset();
        callback_called = true;
      });

  PerUserTopicSubscriptionRequest::Builder builder;
  request = builder.SetInstanceIdToken(token)
                .SetScope(base_url)
                .SetPublicTopicName(topic)
                .SetProjectId(project_id)
                .SetType(type)
                .Build();
  std::string response_body = R"(
    {
      "privateTopicName": "test-pr"
    }
  )";

  network::URLLoaderCompletionStatus response_status(net::ERR_TIMED_OUT);
  response_status.decoded_body_length = response_body.size();

  url_loader_factory()->AddResponse(url(request.get()),
                                    CreateHeadersForTest(net::HTTP_OK),
                                    response_body, response_status);
  request->Start(std::move(completed_callback), url_loader_factory());
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(callback_called);
  // The main expectation is that there is no crash.
}

class PerUserTopicSubscriptionRequestParamTest
    : public PerUserTopicSubscriptionRequestTest,
      public testing::WithParamInterface<net::HttpStatusCode> {
 public:
  PerUserTopicSubscriptionRequestParamTest() = default;
  ~PerUserTopicSubscriptionRequestParamTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(PerUserTopicSubscriptionRequestParamTest);
};

TEST_P(PerUserTopicSubscriptionRequestParamTest,
       ShouldNotSubscribeWhenNonRepeatableError) {
  std::string token = "1234567890";
  std::string base_url = "http://valid-url.test";
  std::string topic = "test";
  std::string project_id = "smarty-pants-12345";
  PerUserTopicSubscriptionRequest::RequestType type =
      PerUserTopicSubscriptionRequest::SUBSCRIBE;

  base::MockCallback<PerUserTopicSubscriptionRequest::CompletedCallback>
      callback;
  Status status(StatusCode::FAILED, "initial");
  std::string private_topic;
  EXPECT_CALL(callback, Run(_, _))
      .WillOnce(DoAll(SaveArg<0>(&status), SaveArg<1>(&private_topic)));

  PerUserTopicSubscriptionRequest::Builder builder;
  std::unique_ptr<PerUserTopicSubscriptionRequest> request =
      builder.SetInstanceIdToken(token)
          .SetScope(base_url)
          .SetPublicTopicName(topic)
          .SetProjectId(project_id)
          .SetType(type)
          .Build();
  network::URLLoaderCompletionStatus response_status(net::OK);

  url_loader_factory()->AddResponse(
      url(request.get()), CreateHeadersForTest(GetParam()),
      /* response_body */ std::string(), response_status);
  request->Start(callback.Get(), url_loader_factory());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(status.code, StatusCode::FAILED_NON_RETRIABLE);
}

INSTANTIATE_TEST_SUITE_P(All,
                         PerUserTopicSubscriptionRequestParamTest,
                         testing::Values(net::HTTP_BAD_REQUEST,
                                         net::HTTP_FORBIDDEN,
                                         net::HTTP_NOT_FOUND));

}  // namespace syncer
