// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/attachments/attachment_downloader.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/thread_task_runner_handle.h"
#include "google_apis/gaia/fake_oauth2_token_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "sync/api/attachments/attachment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

const char kAccountId[] = "attachments@gmail.com";
const char kAccessToken[] = "access.token";
const char kAttachmentServerUrl[] = "http://attachments.com/";
const char kAttachmentContent[] = "attachment.content";

// MockOAuth2TokenService remembers last request for access token and verifies
// that only one request is active at a time.
// Call RespondToAccessTokenRequest to respond to it.
class MockOAuth2TokenService : public FakeOAuth2TokenService {
 public:
  MockOAuth2TokenService() : num_invalidate_token_(0) {}

  virtual ~MockOAuth2TokenService() {}

  void RespondToAccessTokenRequest(GoogleServiceAuthError error);

  int num_invalidate_token() const { return num_invalidate_token_; }

 protected:
  virtual void FetchOAuth2Token(RequestImpl* request,
                                const std::string& account_id,
                                net::URLRequestContextGetter* getter,
                                const std::string& client_id,
                                const std::string& client_secret,
                                const ScopeSet& scopes) OVERRIDE;

  virtual void InvalidateOAuth2Token(const std::string& account_id,
                                     const std::string& client_id,
                                     const ScopeSet& scopes,
                                     const std::string& access_token) OVERRIDE;

 private:
  base::WeakPtr<RequestImpl> last_request_;
  int num_invalidate_token_;
};

void MockOAuth2TokenService::RespondToAccessTokenRequest(
    GoogleServiceAuthError error) {
  EXPECT_TRUE(last_request_ != NULL);
  std::string access_token;
  base::Time expiration_time;
  if (error == GoogleServiceAuthError::AuthErrorNone()) {
    access_token = kAccessToken;
    expiration_time = base::Time::Max();
  }
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&OAuth2TokenService::RequestImpl::InformConsumer,
                 last_request_,
                 error,
                 access_token,
                 expiration_time));
}

void MockOAuth2TokenService::FetchOAuth2Token(
    RequestImpl* request,
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    const std::string& client_id,
    const std::string& client_secret,
    const ScopeSet& scopes) {
  // Only one request at a time is allowed.
  EXPECT_TRUE(last_request_ == NULL);
  last_request_ = request->AsWeakPtr();
}

void MockOAuth2TokenService::InvalidateOAuth2Token(
    const std::string& account_id,
    const std::string& client_id,
    const ScopeSet& scopes,
    const std::string& access_token) {
  ++num_invalidate_token_;
}

class TokenServiceProvider
    : public OAuth2TokenServiceRequest::TokenServiceProvider,
      base::NonThreadSafe {
 public:
  TokenServiceProvider(OAuth2TokenService* token_service);

  // OAuth2TokenService::TokenServiceProvider implementation.
  virtual scoped_refptr<base::SingleThreadTaskRunner>
      GetTokenServiceTaskRunner() OVERRIDE;
  virtual OAuth2TokenService* GetTokenService() OVERRIDE;

 private:
  virtual ~TokenServiceProvider();

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  OAuth2TokenService* token_service_;
};

TokenServiceProvider::TokenServiceProvider(OAuth2TokenService* token_service)
    : task_runner_(base::ThreadTaskRunnerHandle::Get()),
      token_service_(token_service) {
  DCHECK(token_service_);
}

TokenServiceProvider::~TokenServiceProvider() {
}

scoped_refptr<base::SingleThreadTaskRunner>
TokenServiceProvider::GetTokenServiceTaskRunner() {
  return task_runner_;
}

OAuth2TokenService* TokenServiceProvider::GetTokenService() {
  DCHECK(task_runner_->BelongsToCurrentThread());
  return token_service_;
}

}  // namespace

class AttachmentDownloaderImplTest : public testing::Test {
 protected:
  typedef std::map<AttachmentId, AttachmentDownloader::DownloadResult>
      ResultsMap;

  AttachmentDownloaderImplTest() : num_completed_downloads_(0) {}

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  AttachmentDownloader* downloader() { return attachment_downloader_.get(); }

  MockOAuth2TokenService* token_service() { return token_service_.get(); }

  int num_completed_downloads() { return num_completed_downloads_; }

  AttachmentDownloader::DownloadCallback download_callback(
      const AttachmentId& id) {
    return base::Bind(&AttachmentDownloaderImplTest::DownloadDone,
                      base::Unretained(this),
                      id);
  }

  void CompleteDownload(int response_code);

  void DownloadDone(const AttachmentId& attachment_id,
                    const AttachmentDownloader::DownloadResult& result,
                    scoped_ptr<Attachment> attachment);

  void VerifyDownloadResult(const AttachmentId& attachment_id,
                            const AttachmentDownloader::DownloadResult& result);

  void RunMessageLoop();

 private:
  base::MessageLoopForIO message_loop_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  net::TestURLFetcherFactory url_fetcher_factory_;
  scoped_ptr<MockOAuth2TokenService> token_service_;
  scoped_ptr<AttachmentDownloader> attachment_downloader_;
  ResultsMap download_results_;
  int num_completed_downloads_;
};

void AttachmentDownloaderImplTest::SetUp() {
  url_request_context_getter_ =
      new net::TestURLRequestContextGetter(message_loop_.message_loop_proxy());
  url_fetcher_factory_.set_remove_fetcher_on_delete(true);
  token_service_.reset(new MockOAuth2TokenService());
  token_service_->AddAccount(kAccountId);
  scoped_refptr<OAuth2TokenServiceRequest::TokenServiceProvider>
      token_service_provider(new TokenServiceProvider(token_service_.get()));

  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(GaiaConstants::kChromeSyncOAuth2Scope);
  attachment_downloader_ =
      AttachmentDownloader::Create(GURL(kAttachmentServerUrl),
                                   url_request_context_getter_,
                                   kAccountId,
                                   scopes,
                                   token_service_provider);
}

void AttachmentDownloaderImplTest::TearDown() {
  RunMessageLoop();
}

void AttachmentDownloaderImplTest::CompleteDownload(int response_code) {
  // TestURLFetcherFactory remembers last active URLFetcher.
  net::TestURLFetcher* fetcher = url_fetcher_factory_.GetFetcherByID(0);
  // There should be outstanding url fetch request.
  EXPECT_TRUE(fetcher != NULL);
  fetcher->set_status(net::URLRequestStatus());
  fetcher->set_response_code(response_code);
  if (response_code == net::HTTP_OK) {
    fetcher->SetResponseString(kAttachmentContent);
  }
  // Call URLFetcherDelegate.
  net::URLFetcherDelegate* delegate = fetcher->delegate();
  delegate->OnURLFetchComplete(fetcher);
  RunMessageLoop();
  // Once result is processed URLFetcher should be deleted.
  fetcher = url_fetcher_factory_.GetFetcherByID(0);
  EXPECT_TRUE(fetcher == NULL);
}

void AttachmentDownloaderImplTest::DownloadDone(
    const AttachmentId& attachment_id,
    const AttachmentDownloader::DownloadResult& result,
    scoped_ptr<Attachment> attachment) {
  download_results_.insert(std::make_pair(attachment_id, result));
  if (result == AttachmentDownloader::DOWNLOAD_SUCCESS) {
    // Successful download should be accompanied by valid attachment with
    // matching id and valid data.
    EXPECT_TRUE(attachment != NULL);
    EXPECT_EQ(attachment_id, attachment->GetId());

    scoped_refptr<base::RefCountedMemory> data = attachment->GetData();
    std::string data_as_string(data->front_as<char>(), data->size());
    EXPECT_EQ(data_as_string, kAttachmentContent);
  } else {
    EXPECT_TRUE(attachment == NULL);
  }
  ++num_completed_downloads_;
}

void AttachmentDownloaderImplTest::VerifyDownloadResult(
    const AttachmentId& attachment_id,
    const AttachmentDownloader::DownloadResult& result) {
  ResultsMap::const_iterator iter = download_results_.find(attachment_id);
  EXPECT_TRUE(iter != download_results_.end());
  EXPECT_EQ(iter->second, result);
}

void AttachmentDownloaderImplTest::RunMessageLoop() {
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
}

TEST_F(AttachmentDownloaderImplTest, HappyCase) {
  AttachmentId id1 = AttachmentId::Create();
  // DownloadAttachment should trigger RequestAccessToken.
  downloader()->DownloadAttachment(id1, download_callback(id1));
  RunMessageLoop();
  // Return valid access token.
  token_service()->RespondToAccessTokenRequest(
      GoogleServiceAuthError::AuthErrorNone());
  RunMessageLoop();
  // Check that there is outstanding URLFetcher request and complete it.
  CompleteDownload(net::HTTP_OK);
  // Verify that callback was called for the right id with the right result.
  VerifyDownloadResult(id1, AttachmentDownloader::DOWNLOAD_SUCCESS);
}

TEST_F(AttachmentDownloaderImplTest, SameIdMultipleDownloads) {
  AttachmentId id1 = AttachmentId::Create();
  // Call DownloadAttachment two times for the same id.
  downloader()->DownloadAttachment(id1, download_callback(id1));
  downloader()->DownloadAttachment(id1, download_callback(id1));
  RunMessageLoop();
  // Return valid access token.
  token_service()->RespondToAccessTokenRequest(
      GoogleServiceAuthError::AuthErrorNone());
  RunMessageLoop();
  // Start one more download after access token is received.
  downloader()->DownloadAttachment(id1, download_callback(id1));
  // Complete URLFetcher request.
  CompleteDownload(net::HTTP_OK);
  // Verify that all download requests completed.
  VerifyDownloadResult(id1, AttachmentDownloader::DOWNLOAD_SUCCESS);
  EXPECT_EQ(3, num_completed_downloads());

  // Let's download the same attachment again.
  downloader()->DownloadAttachment(id1, download_callback(id1));
  RunMessageLoop();
  // Verify that it didn't finish prematurely.
  EXPECT_EQ(3, num_completed_downloads());
  // Return valid access token.
  token_service()->RespondToAccessTokenRequest(
      GoogleServiceAuthError::AuthErrorNone());
  RunMessageLoop();
  // Complete URLFetcher request.
  CompleteDownload(net::HTTP_OK);
  // Verify that all download requests completed.
  VerifyDownloadResult(id1, AttachmentDownloader::DOWNLOAD_SUCCESS);
  EXPECT_EQ(4, num_completed_downloads());
}

TEST_F(AttachmentDownloaderImplTest, RequestAccessTokenFails) {
  AttachmentId id1 = AttachmentId::Create();
  AttachmentId id2 = AttachmentId::Create();
  // Trigger first RequestAccessToken.
  downloader()->DownloadAttachment(id1, download_callback(id1));
  RunMessageLoop();
  // Return valid access token.
  token_service()->RespondToAccessTokenRequest(
      GoogleServiceAuthError::AuthErrorNone());
  RunMessageLoop();
  // Trigger second RequestAccessToken.
  downloader()->DownloadAttachment(id2, download_callback(id2));
  RunMessageLoop();
  // Fail RequestAccessToken.
  token_service()->RespondToAccessTokenRequest(
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  RunMessageLoop();
  // Only id2 should fail.
  VerifyDownloadResult(id2, AttachmentDownloader::DOWNLOAD_TRANSIENT_ERROR);
  // Complete request for id1.
  CompleteDownload(net::HTTP_OK);
  VerifyDownloadResult(id1, AttachmentDownloader::DOWNLOAD_SUCCESS);
}

TEST_F(AttachmentDownloaderImplTest, URLFetcher_BadToken) {
  AttachmentId id1 = AttachmentId::Create();
  downloader()->DownloadAttachment(id1, download_callback(id1));
  RunMessageLoop();
  // Return valid access token.
  token_service()->RespondToAccessTokenRequest(
      GoogleServiceAuthError::AuthErrorNone());
  RunMessageLoop();
  // Fail URLFetcher. This should trigger download failure and access token
  // invalidation.
  CompleteDownload(net::HTTP_UNAUTHORIZED);
  EXPECT_EQ(1, token_service()->num_invalidate_token());
  VerifyDownloadResult(id1, AttachmentDownloader::DOWNLOAD_TRANSIENT_ERROR);
}

TEST_F(AttachmentDownloaderImplTest, URLFetcher_ServiceUnavailable) {
  AttachmentId id1 = AttachmentId::Create();
  downloader()->DownloadAttachment(id1, download_callback(id1));
  RunMessageLoop();
  // Return valid access token.
  token_service()->RespondToAccessTokenRequest(
      GoogleServiceAuthError::AuthErrorNone());
  RunMessageLoop();
  // Fail URLFetcher. This should trigger download failure. Access token
  // shouldn't be invalidated.
  CompleteDownload(net::HTTP_SERVICE_UNAVAILABLE);
  EXPECT_EQ(0, token_service()->num_invalidate_token());
  VerifyDownloadResult(id1, AttachmentDownloader::DOWNLOAD_TRANSIENT_ERROR);
}

}  // namespace syncer
