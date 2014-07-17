// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/attachments/attachment_uploader_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/thread_task_runner_handle.h"
#include "base/threading/non_thread_safe.h"
#include "base/threading/thread.h"
#include "google_apis/gaia/fake_oauth2_token_service.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/oauth2_token_service_request.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_request_test_util.h"
#include "sync/api/attachments/attachment.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kAttachmentData[] = "some data";
const char kAccountId[] = "some-account-id";
const char kAccessToken[] = "some-access-token";
const char kAuthorization[] = "Authorization";
const char kAttachments[] = "/attachments/";

}  // namespace

namespace syncer {

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

class RequestHandler;

// A mock implementation of an OAuth2TokenService.
//
// Use |SetResponse| to vary the response to token requests.
//
// Use |num_invalidate_token| and |last_token_invalidated| to check the number
// of invalidate token operations performed and the last token invalidated.
class MockOAuth2TokenService : public FakeOAuth2TokenService {
 public:
  MockOAuth2TokenService();
  virtual ~MockOAuth2TokenService();

  void SetResponse(const GoogleServiceAuthError& error,
                   const std::string& access_token,
                   const base::Time& expiration);

  int num_invalidate_token() const { return num_invalidate_token_; }

  const std::string& last_token_invalidated() const {
    return last_token_invalidated_;
  }

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
  GoogleServiceAuthError response_error_;
  std::string response_access_token_;
  base::Time response_expiration_;
  int num_invalidate_token_;
  std::string last_token_invalidated_;
};

MockOAuth2TokenService::MockOAuth2TokenService()
    : response_error_(GoogleServiceAuthError::AuthErrorNone()),
      response_access_token_(kAccessToken),
      response_expiration_(base::Time::Max()),
      num_invalidate_token_(0) {
}

MockOAuth2TokenService::~MockOAuth2TokenService() {
}

void MockOAuth2TokenService::SetResponse(const GoogleServiceAuthError& error,
                                         const std::string& access_token,
                                         const base::Time& expiration) {
  response_error_ = error;
  response_access_token_ = access_token;
  response_expiration_ = expiration;
}

void MockOAuth2TokenService::FetchOAuth2Token(
    RequestImpl* request,
    const std::string& account_id,
    net::URLRequestContextGetter* getter,
    const std::string& client_id,
    const std::string& client_secret,
    const ScopeSet& scopes) {
  base::MessageLoop::current()->PostTask(
      FROM_HERE,
      base::Bind(&OAuth2TokenService::RequestImpl::InformConsumer,
                 request->AsWeakPtr(),
                 response_error_,
                 response_access_token_,
                 response_expiration_));
}

void MockOAuth2TokenService::InvalidateOAuth2Token(
    const std::string& account_id,
    const std::string& client_id,
    const ScopeSet& scopes,
    const std::string& access_token) {
  ++num_invalidate_token_;
  last_token_invalidated_ = access_token;
}

class TokenServiceProvider
    : public OAuth2TokenServiceRequest::TokenServiceProvider,
      base::NonThreadSafe {
 public:
  TokenServiceProvider(OAuth2TokenService* token_service);
  virtual ~TokenServiceProvider();

  // OAuth2TokenService::TokenServiceProvider implementation.
  virtual scoped_refptr<base::SingleThreadTaskRunner>
      GetTokenServiceTaskRunner() OVERRIDE;
  virtual OAuth2TokenService* GetTokenService() OVERRIDE;

 private:
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

// Text fixture for AttachmentUploaderImpl test.
//
// This fixture provides an embedded HTTP server and a mock OAuth2 token service
// for interacting with AttachmentUploaderImpl
class AttachmentUploaderImplTest : public testing::Test,
                                   public base::NonThreadSafe {
 public:
  void OnRequestReceived(const HttpRequest& request);

 protected:
  AttachmentUploaderImplTest();
  virtual void SetUp();
  virtual void TearDown();

  // Run the message loop until UploadDone has been invoked |num_uploads| times.
  void RunAndWaitFor(int num_uploads);

  scoped_ptr<AttachmentUploader>& uploader();
  const AttachmentUploader::UploadCallback& upload_callback() const;
  std::vector<HttpRequest>& http_requests_received();
  std::vector<AttachmentUploader::UploadResult>& upload_results();
  std::vector<AttachmentId>& attachment_ids();
  MockOAuth2TokenService& token_service();
  base::MessageLoopForIO& message_loop();
  RequestHandler& request_handler();

 private:
  // An UploadCallback invoked by AttachmentUploaderImpl.
  void UploadDone(const AttachmentUploader::UploadResult& result,
                  const AttachmentId& attachment_id);

  base::MessageLoopForIO message_loop_;
  scoped_refptr<net::URLRequestContextGetter> url_request_context_getter_;
  scoped_ptr<RequestHandler> request_handler_;
  scoped_ptr<AttachmentUploader> uploader_;
  AttachmentUploader::UploadCallback upload_callback_;
  net::test_server::EmbeddedTestServer server_;
  // A closure that signals an upload has finished.
  base::Closure signal_upload_done_;
  std::vector<HttpRequest> http_requests_received_;
  std::vector<AttachmentUploader::UploadResult> upload_results_;
  std::vector<AttachmentId> attachment_ids_;
  scoped_ptr<MockOAuth2TokenService> token_service_;

  // Must be last data member.
  base::WeakPtrFactory<AttachmentUploaderImplTest> weak_ptr_factory_;
};

// Handles HTTP requests received by the EmbeddedTestServer.
//
// Responds with HTTP_OK by default.  See |SetStatusCode|.
class RequestHandler : public base::NonThreadSafe {
 public:
  // Construct a RequestHandler that will PostTask to |test| using
  // |test_task_runner|.
  RequestHandler(
      const scoped_refptr<base::SingleThreadTaskRunner>& test_task_runner,
      const base::WeakPtr<AttachmentUploaderImplTest>& test);

  ~RequestHandler();

  scoped_ptr<HttpResponse> HandleRequest(const HttpRequest& request);

  // Set the HTTP status code to respond with.
  void SetStatusCode(const net::HttpStatusCode& status_code);

  // Returns the HTTP status code that will be used in responses.
  net::HttpStatusCode GetStatusCode() const;

 private:
  // Protects status_code_.
  mutable base::Lock mutex_;
  net::HttpStatusCode status_code_;

  scoped_refptr<base::SingleThreadTaskRunner> test_task_runner_;
  base::WeakPtr<AttachmentUploaderImplTest> test_;
};

AttachmentUploaderImplTest::AttachmentUploaderImplTest()
    : weak_ptr_factory_(this) {
}

void AttachmentUploaderImplTest::OnRequestReceived(const HttpRequest& request) {
  DCHECK(CalledOnValidThread());
  http_requests_received_.push_back(request);
}

void AttachmentUploaderImplTest::SetUp() {
  DCHECK(CalledOnValidThread());
  request_handler_.reset(new RequestHandler(message_loop_.message_loop_proxy(),
                                            weak_ptr_factory_.GetWeakPtr()));
  url_request_context_getter_ =
      new net::TestURLRequestContextGetter(message_loop_.message_loop_proxy());

  ASSERT_TRUE(server_.InitializeAndWaitUntilReady());
  server_.RegisterRequestHandler(
      base::Bind(&RequestHandler::HandleRequest,
                 base::Unretained(request_handler_.get())));

  GURL url(base::StringPrintf("http://localhost:%d/", server_.port()));

  token_service_.reset(new MockOAuth2TokenService);
  scoped_ptr<OAuth2TokenServiceRequest::TokenServiceProvider>
      token_service_provider(new TokenServiceProvider(token_service_.get()));

  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(GaiaConstants::kChromeSyncOAuth2Scope);
  uploader().reset(new AttachmentUploaderImpl(url,
                                              url_request_context_getter_,
                                              kAccountId,
                                              scopes,
                                              token_service_provider.Pass()));

  upload_callback_ = base::Bind(&AttachmentUploaderImplTest::UploadDone,
                                base::Unretained(this));
}

void AttachmentUploaderImplTest::TearDown() {
  base::RunLoop().RunUntilIdle();
}

void AttachmentUploaderImplTest::RunAndWaitFor(int num_uploads) {
  for (int i = 0; i < num_uploads; ++i) {
    // Run the loop until one upload completes.
    base::RunLoop run_loop;
    signal_upload_done_ = run_loop.QuitClosure();
    run_loop.Run();
  }
}

scoped_ptr<AttachmentUploader>& AttachmentUploaderImplTest::uploader() {
  return uploader_;
}

const AttachmentUploader::UploadCallback&
AttachmentUploaderImplTest::upload_callback() const {
  return upload_callback_;
}

std::vector<HttpRequest>& AttachmentUploaderImplTest::http_requests_received() {
  return http_requests_received_;
}

std::vector<AttachmentUploader::UploadResult>&
AttachmentUploaderImplTest::upload_results() {
  return upload_results_;
}

std::vector<AttachmentId>&
AttachmentUploaderImplTest::attachment_ids() {
  return attachment_ids_;
}

MockOAuth2TokenService& AttachmentUploaderImplTest::token_service() {
  return *token_service_;
}

base::MessageLoopForIO& AttachmentUploaderImplTest::message_loop() {
  return message_loop_;
}

RequestHandler& AttachmentUploaderImplTest::request_handler() {
  return *request_handler_;
}

void AttachmentUploaderImplTest::UploadDone(
    const AttachmentUploader::UploadResult& result,
    const AttachmentId& attachment_id) {
  DCHECK(CalledOnValidThread());
  upload_results_.push_back(result);
  attachment_ids_.push_back(attachment_id);
  DCHECK(!signal_upload_done_.is_null());
  signal_upload_done_.Run();
}

RequestHandler::RequestHandler(
    const scoped_refptr<base::SingleThreadTaskRunner>& test_task_runner,
    const base::WeakPtr<AttachmentUploaderImplTest>& test)
    : status_code_(net::HTTP_OK),
      test_task_runner_(test_task_runner),
      test_(test) {
  DetachFromThread();
}

RequestHandler::~RequestHandler() {
  DetachFromThread();
}

scoped_ptr<HttpResponse> RequestHandler::HandleRequest(
    const HttpRequest& request) {
  DCHECK(CalledOnValidThread());
  test_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &AttachmentUploaderImplTest::OnRequestReceived, test_, request));
  scoped_ptr<BasicHttpResponse> response(new BasicHttpResponse);
  response->set_code(GetStatusCode());
  response->set_content_type("text/plain");
  return response.PassAs<HttpResponse>();
}

void RequestHandler::SetStatusCode(const net::HttpStatusCode& status_code) {
  base::AutoLock lock(mutex_);
  status_code_ = status_code;
}

net::HttpStatusCode RequestHandler::GetStatusCode() const {
  base::AutoLock lock(mutex_);
  return status_code_;
}

TEST_F(AttachmentUploaderImplTest, GetURLForAttachmentId_NoPath) {
  AttachmentId id = AttachmentId::Create();
  std::string unique_id = id.GetProto().unique_id();
  GURL sync_service_url("https://example.com");
  EXPECT_EQ("https://example.com/attachments/" + unique_id,
            AttachmentUploaderImpl::GetURLForAttachmentId(sync_service_url, id)
                .spec());
}

TEST_F(AttachmentUploaderImplTest, GetURLForAttachmentId_JustSlash) {
  AttachmentId id = AttachmentId::Create();
  std::string unique_id = id.GetProto().unique_id();
  GURL sync_service_url("https://example.com/");
  EXPECT_EQ("https://example.com/attachments/" + unique_id,
            AttachmentUploaderImpl::GetURLForAttachmentId(sync_service_url, id)
                .spec());
}

TEST_F(AttachmentUploaderImplTest, GetURLForAttachmentId_Path) {
  AttachmentId id = AttachmentId::Create();
  std::string unique_id = id.GetProto().unique_id();
  GURL sync_service_url("https://example.com/service");
  EXPECT_EQ("https://example.com/service/attachments/" + unique_id,
            AttachmentUploaderImpl::GetURLForAttachmentId(sync_service_url, id)
                .spec());
}

TEST_F(AttachmentUploaderImplTest, GetURLForAttachmentId_PathAndSlash) {
  AttachmentId id = AttachmentId::Create();
  std::string unique_id = id.GetProto().unique_id();
  GURL sync_service_url("https://example.com/service/");
  EXPECT_EQ("https://example.com/service/attachments/" + unique_id,
            AttachmentUploaderImpl::GetURLForAttachmentId(sync_service_url, id)
                .spec());
}

// Verify the "happy case" of uploading an attachment.
//
// Token is requested, token is returned, HTTP request is made, attachment is
// received by server.
TEST_F(AttachmentUploaderImplTest, UploadAttachment_HappyCase) {
  token_service().AddAccount(kAccountId);
  request_handler().SetStatusCode(net::HTTP_OK);

  scoped_refptr<base::RefCountedString> some_data(new base::RefCountedString);
  some_data->data() = kAttachmentData;
  Attachment attachment = Attachment::Create(some_data);
  uploader()->UploadAttachment(attachment, upload_callback());

  // Run until the done callback is invoked.
  RunAndWaitFor(1);

  // See that the done callback was invoked with the right arguments.
  ASSERT_EQ(1U, upload_results().size());
  EXPECT_EQ(AttachmentUploader::UPLOAD_SUCCESS, upload_results()[0]);
  ASSERT_EQ(1U, attachment_ids().size());
  EXPECT_EQ(attachment.GetId(), attachment_ids()[0]);

  // See that the HTTP server received one request.
  ASSERT_EQ(1U, http_requests_received().size());
  const HttpRequest& http_request = http_requests_received().front();
  EXPECT_EQ(net::test_server::METHOD_POST, http_request.method);
  std::string expected_relative_url(kAttachments +
                                    attachment.GetId().GetProto().unique_id());
  EXPECT_EQ(expected_relative_url, http_request.relative_url);
  EXPECT_TRUE(http_request.has_content);
  EXPECT_EQ(kAttachmentData, http_request.content);
  const std::string header_name(kAuthorization);
  const std::string header_value(std::string("Bearer ") + kAccessToken);
  EXPECT_THAT(http_request.headers,
              testing::Contains(testing::Pair(header_name, header_value)));
}

// Verify two overlapping calls to upload the same attachment result in only one
// HTTP request.
TEST_F(AttachmentUploaderImplTest, UploadAttachment_Collapse) {
  token_service().AddAccount(kAccountId);
  request_handler().SetStatusCode(net::HTTP_OK);

  scoped_refptr<base::RefCountedString> some_data(new base::RefCountedString);
  some_data->data() = kAttachmentData;
  Attachment attachment1 = Attachment::Create(some_data);
  Attachment attachment2 = attachment1;
  uploader()->UploadAttachment(attachment1, upload_callback());
  uploader()->UploadAttachment(attachment2, upload_callback());

  // Wait for upload_callback() to be invoked twice.
  RunAndWaitFor(2);
  // See there was only one request.
  EXPECT_EQ(1U, http_requests_received().size());
}

// Verify that the internal state associated with an upload is removed when the
// uplaod finishes.  We do this by issuing two non-overlapping uploads for the
// same attachment and see that it results in two HTTP requests.
TEST_F(AttachmentUploaderImplTest, UploadAttachment_CleanUpAfterUpload) {
  token_service().AddAccount(kAccountId);
  request_handler().SetStatusCode(net::HTTP_OK);

  scoped_refptr<base::RefCountedString> some_data(new base::RefCountedString);
  some_data->data() = kAttachmentData;
  Attachment attachment1 = Attachment::Create(some_data);
  Attachment attachment2 = attachment1;
  uploader()->UploadAttachment(attachment1, upload_callback());

  // Wait for upload_callback() to be invoked before starting the second upload.
  RunAndWaitFor(1);
  uploader()->UploadAttachment(attachment2, upload_callback());

  // Wait for upload_callback() to be invoked a second time.
  RunAndWaitFor(1);
  // See there were two requests.
  ASSERT_EQ(2U, http_requests_received().size());
}

// Verify that we do not issue an HTTP request when we fail to receive an access
// token.
//
// Token is requested, no token is returned, no HTTP request is made
TEST_F(AttachmentUploaderImplTest, UploadAttachment_FailToGetToken) {
  // Note, we won't receive a token because we did not add kAccountId to the
  // token service.
  scoped_refptr<base::RefCountedString> some_data(new base::RefCountedString);
  some_data->data() = kAttachmentData;
  Attachment attachment = Attachment::Create(some_data);
  uploader()->UploadAttachment(attachment, upload_callback());

  RunAndWaitFor(1);

  // See that the done callback was invoked.
  ASSERT_EQ(1U, upload_results().size());
  EXPECT_EQ(AttachmentUploader::UPLOAD_UNSPECIFIED_ERROR, upload_results()[0]);
  ASSERT_EQ(1U, attachment_ids().size());
  EXPECT_EQ(attachment.GetId(), attachment_ids()[0]);

  // See that no HTTP request was received.
  ASSERT_EQ(0U, http_requests_received().size());
}

// Verify behavior when the server returns "503 Service Unavailable".
TEST_F(AttachmentUploaderImplTest, UploadAttachment_ServiceUnavilable) {
  token_service().AddAccount(kAccountId);
  request_handler().SetStatusCode(net::HTTP_SERVICE_UNAVAILABLE);

  scoped_refptr<base::RefCountedString> some_data(new base::RefCountedString);
  some_data->data() = kAttachmentData;
  Attachment attachment = Attachment::Create(some_data);
  uploader()->UploadAttachment(attachment, upload_callback());

  RunAndWaitFor(1);

  // See that the done callback was invoked.
  ASSERT_EQ(1U, upload_results().size());
  EXPECT_EQ(AttachmentUploader::UPLOAD_UNSPECIFIED_ERROR, upload_results()[0]);
  ASSERT_EQ(1U, attachment_ids().size());
  EXPECT_EQ(attachment.GetId(), attachment_ids()[0]);

  // See that the HTTP server received one request.
  ASSERT_EQ(1U, http_requests_received().size());
  const HttpRequest& http_request = http_requests_received().front();
  EXPECT_EQ(net::test_server::METHOD_POST, http_request.method);
  std::string expected_relative_url(kAttachments +
                                    attachment.GetId().GetProto().unique_id());
  EXPECT_EQ(expected_relative_url, http_request.relative_url);
  EXPECT_TRUE(http_request.has_content);
  EXPECT_EQ(kAttachmentData, http_request.content);
  std::string expected_header(kAuthorization);
  const std::string header_name(kAuthorization);
  const std::string header_value(std::string("Bearer ") + kAccessToken);
  EXPECT_THAT(http_request.headers,
              testing::Contains(testing::Pair(header_name, header_value)));

  // See that we did not invalidate the token.
  ASSERT_EQ(0, token_service().num_invalidate_token());
}

// Verify that when we receive an "401 Unauthorized" we invalidate the access
// token.
TEST_F(AttachmentUploaderImplTest, UploadAttachment_BadToken) {
  token_service().AddAccount(kAccountId);
  request_handler().SetStatusCode(net::HTTP_UNAUTHORIZED);

  scoped_refptr<base::RefCountedString> some_data(new base::RefCountedString);
  some_data->data() = kAttachmentData;
  Attachment attachment = Attachment::Create(some_data);
  uploader()->UploadAttachment(attachment, upload_callback());

  RunAndWaitFor(1);

  // See that the done callback was invoked.
  ASSERT_EQ(1U, upload_results().size());
  EXPECT_EQ(AttachmentUploader::UPLOAD_UNSPECIFIED_ERROR, upload_results()[0]);
  ASSERT_EQ(1U, attachment_ids().size());
  EXPECT_EQ(attachment.GetId(), attachment_ids()[0]);

  // See that the HTTP server received one request.
  ASSERT_EQ(1U, http_requests_received().size());
  const HttpRequest& http_request = http_requests_received().front();
  EXPECT_EQ(net::test_server::METHOD_POST, http_request.method);
  std::string expected_relative_url(kAttachments +
                                    attachment.GetId().GetProto().unique_id());
  EXPECT_EQ(expected_relative_url, http_request.relative_url);
  EXPECT_TRUE(http_request.has_content);
  EXPECT_EQ(kAttachmentData, http_request.content);
  std::string expected_header(kAuthorization);
  const std::string header_name(kAuthorization);
  const std::string header_value(std::string("Bearer ") + kAccessToken);
  EXPECT_THAT(http_request.headers,
              testing::Contains(testing::Pair(header_name, header_value)));

  // See that we invalidated the token.
  ASSERT_EQ(1, token_service().num_invalidate_token());
}

// TODO(maniscalco): Add test case for when we are uploading an attachment that
// already exists.  409 Conflict? (bug 379825)

}  // namespace syncer
