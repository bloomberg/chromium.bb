// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "sync/notifier/gcm_network_channel.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {
namespace {

class TestGCMNetworkChannelDelegate : public GCMNetworkChannelDelegate {
 public:
  virtual void RequestToken(RequestTokenCallback callback) OVERRIDE {
    request_token_callback = callback;
  }

  virtual void InvalidateToken(const std::string& token) OVERRIDE {
    invalidated_token = token;
  }

  virtual void Register(RegisterCallback callback) OVERRIDE {
    register_callback = callback;
  }

  RequestTokenCallback request_token_callback;
  std::string invalidated_token;
  RegisterCallback register_callback;
};

class GCMNetworkChannelTest
    : public ::testing::Test,
      public SyncNetworkChannel::Observer {
 protected:
  GCMNetworkChannelTest()
      : delegate_(NULL),
        url_fetchers_created_count_(0) {
  }

  virtual ~GCMNetworkChannelTest() {
  }

  virtual void SetUp() {
    request_context_getter_ = new net::TestURLRequestContextGetter(
        base::MessageLoopProxy::current());
    // Ownership of delegate goes to GCNMentworkChannel but test needs pointer
    // to it.
    delegate_ = new TestGCMNetworkChannelDelegate();
    scoped_ptr<GCMNetworkChannelDelegate> delegate(delegate_);
    gcm_network_channel_.reset(new GCMNetworkChannel(request_context_getter_,
                                                     delegate.Pass()));
    gcm_network_channel_->AddObserver(this);
    gcm_network_channel_->SetMessageReceiver(
        invalidation::NewPermanentCallback(
            this, &GCMNetworkChannelTest::OnIncomingMessage));
    url_fetcher_factory_.reset(new net::FakeURLFetcherFactory(NULL,
        base::Bind(&GCMNetworkChannelTest::CreateURLFetcher,
            base::Unretained(this))));
  }

  virtual void TearDown() {
    gcm_network_channel_->RemoveObserver(this);
  }

  virtual void OnNetworkChannelStateChanged(
      InvalidatorState invalidator_state) OVERRIDE {
  }

  void OnIncomingMessage(std::string incoming_message) {
  }

  GCMNetworkChannel* network_channel() {
    return gcm_network_channel_.get();
  }

  TestGCMNetworkChannelDelegate* delegate() {
    return delegate_;
  }

  int url_fetchers_created_count() {
    return url_fetchers_created_count_;
  }

  net::FakeURLFetcherFactory* url_fetcher_factory() {
    return url_fetcher_factory_.get();
  }

  scoped_ptr<net::FakeURLFetcher> CreateURLFetcher(
      const GURL& url,
      net::URLFetcherDelegate* delegate,
      const std::string& response_data,
      net::HttpStatusCode response_code,
      net::URLRequestStatus::Status status) {
    url_fetchers_created_count_++;
    return scoped_ptr<net::FakeURLFetcher>(new net::FakeURLFetcher(
        url, delegate, response_data, response_code, status));
  }

 private:
  base::MessageLoop message_loop_;
  TestGCMNetworkChannelDelegate* delegate_;
  scoped_ptr<GCMNetworkChannel> gcm_network_channel_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;
  scoped_ptr<net::FakeURLFetcherFactory> url_fetcher_factory_;
  int url_fetchers_created_count_;
};

TEST_F(GCMNetworkChannelTest, HappyCase) {
  GURL url("http://invalid.url.com");
  url_fetcher_factory()->SetFakeResponse(url, std::string(), net::HTTP_OK,
                                         net::URLRequestStatus::SUCCESS);

  // After construction GCMNetworkChannel should have called Register.
  EXPECT_FALSE(delegate()->register_callback.is_null());
  // Return valid registration id.
  delegate()->register_callback.Run("registration.id", gcm::GCMClient::SUCCESS);

  network_channel()->SendMessage("abra.cadabra");
  // SendMessage should have triggered RequestToken. No HTTP request should be
  // started yet.
  EXPECT_FALSE(delegate()->request_token_callback.is_null());
  EXPECT_EQ(url_fetchers_created_count(), 0);
  // Return valid access token. This should trigger HTTP request.
  delegate()->request_token_callback.Run(
      GoogleServiceAuthError::AuthErrorNone(), "access.token");
  {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  EXPECT_EQ(url_fetchers_created_count(), 1);

  // Return another access token. Message should be cleared by now and shouldn't
  // be sent.
  delegate()->request_token_callback.Run(
      GoogleServiceAuthError::AuthErrorNone(), "access.token2");
  {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  EXPECT_EQ(url_fetchers_created_count(), 1);
}

TEST_F(GCMNetworkChannelTest, FailedRegister) {
  // After construction GCMNetworkChannel should have called Register.
  EXPECT_FALSE(delegate()->register_callback.is_null());
  // Return error from Register call.
  delegate()->register_callback.Run("", gcm::GCMClient::SERVER_ERROR);

  network_channel()->SendMessage("abra.cadabra");
  // SendMessage shouldn't trigger RequestToken.
  EXPECT_TRUE(delegate()->request_token_callback.is_null());
  EXPECT_EQ(url_fetchers_created_count(), 0);
}

TEST_F(GCMNetworkChannelTest, RegisterFinishesAfterSendMessage) {
  GURL url("http://invalid.url.com");
  url_fetcher_factory()->SetFakeResponse(url, "", net::HTTP_OK,
      net::URLRequestStatus::SUCCESS);

  // After construction GCMNetworkChannel should have called Register.
  EXPECT_FALSE(delegate()->register_callback.is_null());

  network_channel()->SendMessage("abra.cadabra");
  // SendMessage shouldn't trigger RequestToken.
  EXPECT_TRUE(delegate()->request_token_callback.is_null());
  EXPECT_EQ(url_fetchers_created_count(), 0);

  // Return valid registration id.
  delegate()->register_callback.Run("registration.id", gcm::GCMClient::SUCCESS);

  EXPECT_FALSE(delegate()->request_token_callback.is_null());
  EXPECT_EQ(url_fetchers_created_count(), 0);
  // Return valid access token. This should trigger HTTP request.
  delegate()->request_token_callback.Run(
      GoogleServiceAuthError::AuthErrorNone(), "access.token");
  {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  EXPECT_EQ(url_fetchers_created_count(), 1);
}

TEST_F(GCMNetworkChannelTest, RequestTokenFailure) {
  // After construction GCMNetworkChannel should have called Register.
  EXPECT_FALSE(delegate()->register_callback.is_null());
  // Return valid registration id.
  delegate()->register_callback.Run("registration.id", gcm::GCMClient::SUCCESS);

  network_channel()->SendMessage("abra.cadabra");
  // SendMessage should have triggered RequestToken. No HTTP request should be
  // started yet.
  EXPECT_FALSE(delegate()->request_token_callback.is_null());
  EXPECT_EQ(url_fetchers_created_count(), 0);
  // RequestToken returns failure.
  delegate()->request_token_callback.Run(
      GoogleServiceAuthError::FromConnectionError(1), "");

  // Should be no HTTP requests.
  EXPECT_EQ(url_fetchers_created_count(), 0);
}

TEST_F(GCMNetworkChannelTest, AuthErrorFromServer) {
  // Setup fake response to return AUTH_ERROR.
  GURL url("http://invalid.url.com");
  url_fetcher_factory()->SetFakeResponse(url, "", net::HTTP_UNAUTHORIZED,
      net::URLRequestStatus::SUCCESS);

  // After construction GCMNetworkChannel should have called Register.
  EXPECT_FALSE(delegate()->register_callback.is_null());
  // Return valid registration id.
  delegate()->register_callback.Run("registration.id", gcm::GCMClient::SUCCESS);

  network_channel()->SendMessage("abra.cadabra");
  // SendMessage should have triggered RequestToken. No HTTP request should be
  // started yet.
  EXPECT_FALSE(delegate()->request_token_callback.is_null());
  EXPECT_EQ(url_fetchers_created_count(), 0);
  // Return valid access token. This should trigger HTTP request.
  delegate()->request_token_callback.Run(
      GoogleServiceAuthError::AuthErrorNone(), "access.token");
  {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }
  EXPECT_EQ(url_fetchers_created_count(), 1);
  EXPECT_EQ(delegate()->invalidated_token, "access.token");
}

// Following two tests are to check for memory leaks/crashes when Register and
// RequestToken don't complete by the teardown.
TEST_F(GCMNetworkChannelTest, RegisterNeverCompletes) {
  network_channel()->SendMessage("abra.cadabra");
  // Register should be called by now. Let's not complete and see what happens.
  EXPECT_FALSE(delegate()->register_callback.is_null());
}

TEST_F(GCMNetworkChannelTest, RequestTokenNeverCompletes) {
  network_channel()->SendMessage("abra.cadabra");
  // Return valid registration id.
  delegate()->register_callback.Run("registration.id", gcm::GCMClient::SUCCESS);
  // RequestToken should be called by now. Let's not complete and see what
  // happens.
  EXPECT_FALSE(delegate()->request_token_callback.is_null());
}

}  // namespace
}  // namespace syncer
