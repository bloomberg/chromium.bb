// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/oauth_token_getter_proxy.h"

#include "base/callback.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

class FakeOAuthTokenGetter : public OAuthTokenGetter {
 public:
  FakeOAuthTokenGetter(base::OnceClosure on_destroyed);
  ~FakeOAuthTokenGetter() override;

  void ResolveCallback(Status status,
                       const std::string& user_email,
                       const std::string& access_token);

  void ExpectInvalidateCache();

  // OAuthTokenGetter overrides.
  void CallWithToken(const TokenCallback& on_access_token) override;
  void InvalidateCache() override;

 private:
  TokenCallback on_access_token_;
  bool invalidate_cache_expected_ = false;
  base::OnceClosure on_destroyed_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(FakeOAuthTokenGetter);
};

FakeOAuthTokenGetter::FakeOAuthTokenGetter(base::OnceClosure on_destroyed)
    : on_destroyed_(std::move(on_destroyed)) {
  DETACH_FROM_THREAD(thread_checker_);
}

FakeOAuthTokenGetter::~FakeOAuthTokenGetter() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!invalidate_cache_expected_);
  std::move(on_destroyed_).Run();
}

void FakeOAuthTokenGetter::ResolveCallback(Status status,
                                           const std::string& user_email,
                                           const std::string& access_token) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!on_access_token_.is_null());
  on_access_token_.Run(status, user_email, access_token);
  on_access_token_.Reset();
}

void FakeOAuthTokenGetter::ExpectInvalidateCache() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ASSERT_FALSE(invalidate_cache_expected_);
  invalidate_cache_expected_ = true;
}

void FakeOAuthTokenGetter::CallWithToken(const TokenCallback& on_access_token) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  on_access_token_ = on_access_token;
}

void FakeOAuthTokenGetter::InvalidateCache() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  ASSERT_TRUE(invalidate_cache_expected_);
  invalidate_cache_expected_ = false;
}

}  // namespace

class OAuthTokenGetterProxyTest : public testing::Test {
 public:
  OAuthTokenGetterProxyTest() {}
  ~OAuthTokenGetterProxyTest() override {}

  // testing::Test overrides.
  void SetUp() override;
  void TearDown() override;

 protected:
  void TestCallWithTokenOnRunnerThread(OAuthTokenGetter::Status status,
                                       const std::string& user_email,
                                       const std::string& access_token);

  void TestCallWithTokenOnMainThread(OAuthTokenGetter::Status status,
                                     const std::string& user_email,
                                     const std::string& access_token);

  void ExpectInvalidateCache();

  base::Thread runner_thread_{"runner_thread"};
  FakeOAuthTokenGetter* token_getter_;
  std::unique_ptr<OAuthTokenGetterProxy> proxy_;

 private:
  void TestCallWithTokenImpl(OAuthTokenGetter::Status status,
                             const std::string& user_email,
                             const std::string& access_token);

  void OnTokenReceived(OAuthTokenGetter::Status status,
                       const std::string& user_email,
                       const std::string& access_token);

  struct TokenCallbackResult {
    OAuthTokenGetter::Status status;
    std::string user_email;
    std::string access_token;
  };

  std::unique_ptr<TokenCallbackResult> expected_callback_result_;

  void OnTokenGetterDestroyed();

  base::MessageLoop main_loop_;

  DISALLOW_COPY_AND_ASSIGN(OAuthTokenGetterProxyTest);
};

void OAuthTokenGetterProxyTest::SetUp() {
  std::unique_ptr<FakeOAuthTokenGetter> owned_token_getter =
      std::make_unique<FakeOAuthTokenGetter>(
          base::BindOnce(&OAuthTokenGetterProxyTest::OnTokenGetterDestroyed,
                         base::Unretained(this)));
  token_getter_ = owned_token_getter.get();
  runner_thread_.Start();
  proxy_ = std::make_unique<OAuthTokenGetterProxy>(
      std::move(owned_token_getter), runner_thread_.task_runner());
}

void OAuthTokenGetterProxyTest::TearDown() {
  proxy_.reset();
  runner_thread_.FlushForTesting();
  ASSERT_FALSE(token_getter_);
  ASSERT_FALSE(expected_callback_result_);
}

void OAuthTokenGetterProxyTest::TestCallWithTokenOnRunnerThread(
    OAuthTokenGetter::Status status,
    const std::string& user_email,
    const std::string& access_token) {
  runner_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&OAuthTokenGetterProxyTest::TestCallWithTokenImpl,
                     base::Unretained(this),
                     OAuthTokenGetter::Status::AUTH_ERROR, "email3", "token3"));
  runner_thread_.FlushForTesting();
}

void OAuthTokenGetterProxyTest::TestCallWithTokenOnMainThread(
    OAuthTokenGetter::Status status,
    const std::string& user_email,
    const std::string& access_token) {
  TestCallWithTokenImpl(status, user_email, access_token);
  runner_thread_.FlushForTesting();
  base::RunLoop().RunUntilIdle();
}

void OAuthTokenGetterProxyTest::ExpectInvalidateCache() {
  runner_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&FakeOAuthTokenGetter::ExpectInvalidateCache,
                                base::Unretained(token_getter_)));
}

void OAuthTokenGetterProxyTest::TestCallWithTokenImpl(
    OAuthTokenGetter::Status status,
    const std::string& user_email,
    const std::string& access_token) {
  ASSERT_FALSE(expected_callback_result_);
  expected_callback_result_ = std::make_unique<TokenCallbackResult>();
  expected_callback_result_->status = status;
  expected_callback_result_->user_email = user_email;
  expected_callback_result_->access_token = access_token;
  proxy_->CallWithToken(base::Bind(&OAuthTokenGetterProxyTest::OnTokenReceived,
                                   base::Unretained(this)));
  runner_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&FakeOAuthTokenGetter::ResolveCallback,
                                base::Unretained(token_getter_), status,
                                user_email, access_token));
}

void OAuthTokenGetterProxyTest::OnTokenReceived(
    OAuthTokenGetter::Status status,
    const std::string& user_email,
    const std::string& access_token) {
  ASSERT_TRUE(expected_callback_result_);
  EXPECT_EQ(expected_callback_result_->status, status);
  EXPECT_EQ(expected_callback_result_->user_email, user_email);
  EXPECT_EQ(expected_callback_result_->access_token, access_token);
  expected_callback_result_.reset();
}

void OAuthTokenGetterProxyTest::OnTokenGetterDestroyed() {
  token_getter_ = nullptr;
}

TEST_F(OAuthTokenGetterProxyTest, ProxyDeletedOnMainThread) {
  // Default behavior verified in TearDown().
}

TEST_F(OAuthTokenGetterProxyTest, ProxyDeletedOnRunnerThread) {
  runner_thread_.task_runner()->DeleteSoon(FROM_HERE, proxy_.release());
}

TEST_F(OAuthTokenGetterProxyTest, CallWithTokenOnMainThread) {
  TestCallWithTokenOnMainThread(OAuthTokenGetter::Status::SUCCESS, "email1",
                                "token1");
  TestCallWithTokenOnMainThread(OAuthTokenGetter::Status::NETWORK_ERROR,
                                "email2", "token2");
}

TEST_F(OAuthTokenGetterProxyTest, CallWithTokenOnRunnerThread) {
  TestCallWithTokenOnRunnerThread(OAuthTokenGetter::Status::AUTH_ERROR,
                                  "email3", "token3");
  TestCallWithTokenOnRunnerThread(OAuthTokenGetter::Status::SUCCESS, "email4",
                                  "token4");
}

TEST_F(OAuthTokenGetterProxyTest, InvalidateCacheOnMainThread) {
  ExpectInvalidateCache();
  proxy_->InvalidateCache();
  runner_thread_.FlushForTesting();
}

TEST_F(OAuthTokenGetterProxyTest, InvalidateCacheOnRunnerThread) {
  ExpectInvalidateCache();
  runner_thread_.task_runner()->PostTask(
      FROM_HERE, base::BindOnce(&OAuthTokenGetterProxy::InvalidateCache,
                                base::Unretained(proxy_.get())));
  runner_thread_.FlushForTesting();
}

}  // namespace remoting
