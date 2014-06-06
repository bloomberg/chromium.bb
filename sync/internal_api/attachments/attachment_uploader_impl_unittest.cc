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
#include "base/threading/non_thread_safe.h"
#include "base/threading/thread.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/url_request/url_request_test_util.h"
#include "sync/api/attachments/attachment.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kAttachmentData[] = "some data";

}  // namespace

namespace syncer {

using net::test_server::BasicHttpResponse;
using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

class RequestHandler;

// Text fixture for AttachmentUploaderImpl test.
//
// This fixture provides an embedded HTTP server for interacting with
// AttachmentUploaderImpl.
class AttachmentUploaderImplTest : public testing::Test,
                                   public base::NonThreadSafe {
 public:
  void OnRequestReceived(const HttpRequest& request);

 protected:
  AttachmentUploaderImplTest();
  virtual void SetUp();

  // Run the message loop until UploadDone has been invoked |num_uploads| times.
  void RunAndWaitFor(int num_uploads);

  scoped_ptr<AttachmentUploader>& uploader();
  const AttachmentUploader::UploadCallback& upload_callback() const;
  std::vector<HttpRequest>& http_requests_received();
  std::vector<AttachmentUploader::UploadResult>& upload_results();
  std::vector<AttachmentId>& updated_attachment_ids();

 private:
  // An UploadCallback invoked by AttachmentUploaderImpl.
  void UploadDone(const AttachmentUploader::UploadResult& result,
                  const AttachmentId& updated_attachment_id);

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
  std::vector<AttachmentId> updated_attachment_ids_;

  // Must be last data member.
  base::WeakPtrFactory<AttachmentUploaderImplTest> weak_ptr_factory_;
};

// Handles HTTP requests received by the EmbeddedTestServer.
class RequestHandler : public base::NonThreadSafe {
 public:
  // Construct a RequestHandler that will PostTask to |test| using
  // |test_task_runner|.
  RequestHandler(
      const scoped_refptr<base::SingleThreadTaskRunner>& test_task_runner,
      const base::WeakPtr<AttachmentUploaderImplTest>& test);

  ~RequestHandler();

  scoped_ptr<HttpResponse> HandleRequest(const HttpRequest& request);

 private:
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

  std::string url_prefix(
      base::StringPrintf("http://localhost:%d/uploads/", server_.port()));

  uploader().reset(
      new AttachmentUploaderImpl(url_prefix, url_request_context_getter_));
  upload_callback_ = base::Bind(&AttachmentUploaderImplTest::UploadDone,
                                base::Unretained(this));
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
AttachmentUploaderImplTest::updated_attachment_ids() {
  return updated_attachment_ids_;
}

void AttachmentUploaderImplTest::UploadDone(
    const AttachmentUploader::UploadResult& result,
    const AttachmentId& updated_attachment_id) {
  DCHECK(CalledOnValidThread());
  upload_results_.push_back(result);
  updated_attachment_ids_.push_back(updated_attachment_id);
  signal_upload_done_.Run();
}

RequestHandler::RequestHandler(
    const scoped_refptr<base::SingleThreadTaskRunner>& test_task_runner,
    const base::WeakPtr<AttachmentUploaderImplTest>& test)
    : test_task_runner_(test_task_runner), test_(test) {
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
  scoped_ptr<BasicHttpResponse> http_response(new BasicHttpResponse);
  http_response->set_code(net::HTTP_OK);
  http_response->set_content("hello");
  http_response->set_content_type("text/plain");
  return http_response.PassAs<HttpResponse>();
}

// Verify the "happy case" of uploading an attachment.
TEST_F(AttachmentUploaderImplTest, UploadAttachment_HappyCase) {
  scoped_refptr<base::RefCountedString> some_data(new base::RefCountedString);
  some_data->data() = kAttachmentData;
  Attachment attachment = Attachment::Create(some_data);
  uploader()->UploadAttachment(attachment, upload_callback());
  RunAndWaitFor(1);

  // See that the HTTP server received one request.
  EXPECT_EQ(1U, http_requests_received().size());
  const HttpRequest& http_request = http_requests_received().front();
  EXPECT_EQ(net::test_server::METHOD_POST, http_request.method);
  std::string expected_relative_url("/uploads/" +
                                    attachment.GetId().GetProto().unique_id());
  EXPECT_EQ(expected_relative_url, http_request.relative_url);
  EXPECT_TRUE(http_request.has_content);
  EXPECT_EQ(kAttachmentData, http_request.content);

  // See that the UploadCallback received a result and updated AttachmentId.
  EXPECT_EQ(1U, upload_results().size());
  EXPECT_EQ(1U, updated_attachment_ids().size());
  EXPECT_EQ(AttachmentUploader::UPLOAD_SUCCESS, upload_results().front());
  EXPECT_EQ(attachment.GetId(), updated_attachment_ids().front());
  // TODO(maniscalco): Once AttachmentUploaderImpl is capable of updating the
  // AttachmentId with server address information about the attachment, add some
  // checks here to verify it works properly (bug 371522).
}

// Verify two overlapping calls to upload the same attachment result in only one
// HTTP request.
TEST_F(AttachmentUploaderImplTest, UploadAttachment_Collapse) {
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
  EXPECT_EQ(2U, http_requests_received().size());
}

}  // namespace syncer
