// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "sync/notifier/gcm_network_channel.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class TestGCMNetworkChannelDelegate : public GCMNetworkChannelDelegate {
 public:
  TestGCMNetworkChannelDelegate()
      : register_call_count_(0) {}

  virtual void Initialize() OVERRIDE {}

  virtual void RequestToken(RequestTokenCallback callback) OVERRIDE {
    request_token_callback = callback;
  }

  virtual void InvalidateToken(const std::string& token) OVERRIDE {
    invalidated_token = token;
  }

  virtual void Register(RegisterCallback callback) OVERRIDE {
    ++register_call_count_;
    register_callback = callback;
  }

  RequestTokenCallback request_token_callback;
  std::string invalidated_token;
  RegisterCallback register_callback;
  int register_call_count_;
};

// Backoff policy for test. Run first 5 retries without delay.
const net::BackoffEntry::Policy kTestBackoffPolicy = {
  // Number of initial errors (in sequence) to ignore before applying
  // exponential back-off rules.
  5,

  // Initial delay for exponential back-off in ms.
  2000, // 2 seconds.

  // Factor by which the waiting time will be multiplied.
  2,

  // Fuzzing percentage. ex: 10% will spread requests randomly
  // between 90%-100% of the calculated time.
  0.2, // 20%.

  // Maximum amount of time we are willing to delay our request in ms.
  1000 * 3600 * 4, // 4 hours.

  // Time to keep an entry from being discarded even when it
  // has no significant state, -1 to never discard.
  -1,

  // Don't use initial delay unless the last request was an error.
  false,
};

class TestGCMNetworkChannel : public GCMNetworkChannel {
 public:
  TestGCMNetworkChannel(
      scoped_refptr<net::URLRequestContextGetter> request_context_getter,
      scoped_ptr<GCMNetworkChannelDelegate> delegate)
      :  GCMNetworkChannel(request_context_getter, delegate.Pass()) {
    ResetRegisterBackoffEntryForTest(&kTestBackoffPolicy);
  }

 protected:
  // On Android GCMNetworkChannel::BuildUrl hits NOTREACHED(). I still want
  // tests to run.
  virtual GURL BuildUrl(const std::string& registration_id) OVERRIDE {
    return GURL("http://test.url.com");
  }
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
    gcm_network_channel_.reset(new TestGCMNetworkChannel(
        request_context_getter_,
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

  // Helper functions to call private methods from test
  GURL BuildUrl(const std::string& registration_id) {
    return gcm_network_channel_->GCMNetworkChannel::BuildUrl(registration_id);
  }

  static void Base64EncodeURLSafe(const std::string& input,
                                  std::string* output) {
    GCMNetworkChannel::Base64EncodeURLSafe(input, output);
  }

  static bool Base64DecodeURLSafe(const std::string& input,
                                  std::string* output) {
    return GCMNetworkChannel::Base64DecodeURLSafe(input, output);
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
    ++url_fetchers_created_count_;
    return scoped_ptr<net::FakeURLFetcher>(new net::FakeURLFetcher(
        url, delegate, response_data, response_code, status));
  }

  void RunLoopUntilIdle() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
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
  url_fetcher_factory()->SetFakeResponse(GURL("http://test.url.com"),
                                         std::string(),
                                         net::HTTP_OK,
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
  RunLoopUntilIdle();
  EXPECT_EQ(url_fetchers_created_count(), 1);

  // Return another access token. Message should be cleared by now and shouldn't
  // be sent.
  delegate()->request_token_callback.Run(
      GoogleServiceAuthError::AuthErrorNone(), "access.token2");
  RunLoopUntilIdle();
  EXPECT_EQ(url_fetchers_created_count(), 1);
}

TEST_F(GCMNetworkChannelTest, FailedRegister) {
  // After construction GCMNetworkChannel should have called Register.
  EXPECT_FALSE(delegate()->register_callback.is_null());
  EXPECT_EQ(1, delegate()->register_call_count_);
  // Return transient error from Register call.
  delegate()->register_callback.Run("", gcm::GCMClient::NETWORK_ERROR);
  RunLoopUntilIdle();
  // GcmNetworkChannel should have scheduled Register retry.
  EXPECT_EQ(2, delegate()->register_call_count_);
  // Return persistent error from Register call.
  delegate()->register_callback.Run("", gcm::GCMClient::NOT_SIGNED_IN);
  RunLoopUntilIdle();
  // GcmNetworkChannel should give up trying.
  EXPECT_EQ(2, delegate()->register_call_count_);

  network_channel()->SendMessage("abra.cadabra");
  // SendMessage shouldn't trigger RequestToken.
  EXPECT_TRUE(delegate()->request_token_callback.is_null());
  EXPECT_EQ(0, url_fetchers_created_count());
}

TEST_F(GCMNetworkChannelTest, RegisterFinishesAfterSendMessage) {
  url_fetcher_factory()->SetFakeResponse(GURL("http://test.url.com"),
                                         "",
                                         net::HTTP_OK,
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
  RunLoopUntilIdle();
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
  url_fetcher_factory()->SetFakeResponse(GURL("http://test.url.com"),
                                         "",
                                         net::HTTP_UNAUTHORIZED,
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
  RunLoopUntilIdle();
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

#if !defined(ANDROID)
TEST_F(GCMNetworkChannelTest, BuildUrl) {
  GURL url = BuildUrl("registration.id");
  EXPECT_TRUE(url.SchemeIsHTTPOrHTTPS());
  EXPECT_FALSE(url.host().empty());
  EXPECT_FALSE(url.path().empty());
  std::vector<std::string> parts;
  Tokenize(url.path(), "/", &parts);
  std::string buffer;
  EXPECT_TRUE(Base64DecodeURLSafe(parts[parts.size() - 1], &buffer));
}
#endif

TEST_F(GCMNetworkChannelTest, Base64EncodeDecode) {
  std::string input;
  std::string plain;
  std::string base64;
  // Empty string.
  Base64EncodeURLSafe(input, &base64);
  EXPECT_TRUE(base64.empty());
  EXPECT_TRUE(Base64DecodeURLSafe(base64, &plain));
  EXPECT_EQ(input, plain);
  // String length: 1..7.
  for (int length = 1; length < 8; length++) {
    input = "abra.cadabra";
    input.resize(length);
    Base64EncodeURLSafe(input, &base64);
    // Ensure no padding at the end.
    EXPECT_NE(base64[base64.size() - 1], '=');
    EXPECT_TRUE(Base64DecodeURLSafe(base64, &plain));
    EXPECT_EQ(input, plain);
  }
  // Presence of '-', '_'.
  input = "\xfb\xff";
  Base64EncodeURLSafe(input, &base64);
  EXPECT_EQ("-_8", base64);
  EXPECT_TRUE(Base64DecodeURLSafe(base64, &plain));
  EXPECT_EQ(input, plain);
}

}  // namespace syncer
