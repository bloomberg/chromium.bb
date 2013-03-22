// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_http_job.h"

#include <cstddef>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "googleurl/src/gurl.h"
#include "net/base/auth.h"
#include "net/http/http_transaction_factory.h"
#include "net/http/http_transaction_unittest.h"
#include "net/url_request/url_request_status.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

// Inherit from URLRequestHttpJob to expose the priority and some
// other hidden functions.
class TestURLRequestHttpJob : public URLRequestHttpJob {
 public:
  explicit TestURLRequestHttpJob(URLRequest* request)
      : URLRequestHttpJob(request, NULL,
                          request->context()->http_user_agent_settings()) {}

  using URLRequestHttpJob::SetPriority;
  using URLRequestHttpJob::Start;
  using URLRequestHttpJob::Kill;
  using URLRequestHttpJob::priority;

 protected:
  virtual ~TestURLRequestHttpJob() {}
};

class URLRequestHttpJobTest : public ::testing::Test {
 protected:
  URLRequestHttpJobTest()
      : req_(GURL("http://www.example.com"), &delegate_, &context_, NULL) {
    context_.set_http_transaction_factory(&network_layer_);
  }

  MockNetworkLayer network_layer_;
  TestURLRequestContext context_;
  TestDelegate delegate_;
  TestURLRequest req_;
};

// Make sure that SetPriority actually sets the URLRequestHttpJob's
// priority, both before and after start.
TEST_F(URLRequestHttpJobTest, SetPriorityBasic) {
  scoped_refptr<TestURLRequestHttpJob> job(new TestURLRequestHttpJob(&req_));
  EXPECT_EQ(DEFAULT_PRIORITY, job->priority());

  job->SetPriority(LOWEST);
  EXPECT_EQ(LOWEST, job->priority());

  job->SetPriority(LOW);
  EXPECT_EQ(LOW, job->priority());

  job->Start();
  EXPECT_EQ(LOW, job->priority());

  job->SetPriority(MEDIUM);
  EXPECT_EQ(MEDIUM, job->priority());
}

// Make sure that URLRequestHttpJob passes on its priority to its
// transaction on start.
TEST_F(URLRequestHttpJobTest, SetTransactionPriorityOnStart) {
  scoped_refptr<TestURLRequestHttpJob> job(new TestURLRequestHttpJob(&req_));
  job->SetPriority(LOW);

  EXPECT_FALSE(network_layer_.last_transaction());

  job->Start();

  ASSERT_TRUE(network_layer_.last_transaction());
  EXPECT_EQ(LOW, network_layer_.last_transaction()->priority());
}

// Make sure that URLRequestHttpJob passes on its priority updates to
// its transaction.
TEST_F(URLRequestHttpJobTest, SetTransactionPriority) {
  scoped_refptr<TestURLRequestHttpJob> job(new TestURLRequestHttpJob(&req_));
  job->SetPriority(LOW);
  job->Start();
  ASSERT_TRUE(network_layer_.last_transaction());
  EXPECT_EQ(LOW, network_layer_.last_transaction()->priority());

  job->SetPriority(HIGHEST);
  EXPECT_EQ(HIGHEST, network_layer_.last_transaction()->priority());
}

// Make sure that URLRequestHttpJob passes on its priority updates to
// newly-created transactions after the first one.
TEST_F(URLRequestHttpJobTest, SetSubsequentTransactionPriority) {
  scoped_refptr<TestURLRequestHttpJob> job(new TestURLRequestHttpJob(&req_));
  job->Start();

  job->SetPriority(LOW);
  ASSERT_TRUE(network_layer_.last_transaction());
  EXPECT_EQ(LOW, network_layer_.last_transaction()->priority());

  job->Kill();
  network_layer_.ClearLastTransaction();

  // Creates a second transaction.
  job->Start();
  ASSERT_TRUE(network_layer_.last_transaction());
  EXPECT_EQ(LOW, network_layer_.last_transaction()->priority());
}

}  // namespace

}  // namespace net
