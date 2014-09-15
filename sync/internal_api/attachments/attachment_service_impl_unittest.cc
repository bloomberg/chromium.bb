// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/internal_api/public/attachments/attachment_service_impl.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/timer/mock_timer.h"
#include "sync/internal_api/public/attachments/fake_attachment_downloader.h"
#include "sync/internal_api/public/attachments/fake_attachment_uploader.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class MockAttachmentStore : public AttachmentStore,
                            public base::SupportsWeakPtr<MockAttachmentStore> {
 public:
  MockAttachmentStore() {}

  virtual void Read(const AttachmentIdList& ids,
                    const ReadCallback& callback) OVERRIDE {
    read_ids.push_back(ids);
    read_callbacks.push_back(callback);
  }

  virtual void Write(const AttachmentList& attachments,
                     const WriteCallback& callback) OVERRIDE {
    write_attachments.push_back(attachments);
    write_callbacks.push_back(callback);
  }

  virtual void Drop(const AttachmentIdList& ids,
                    const DropCallback& callback) OVERRIDE {
    NOTREACHED();
  }

  // Respond to Read request. Attachments found in local_attachments should be
  // returned, everything else should be reported unavailable.
  void RespondToRead(const AttachmentIdSet& local_attachments) {
    scoped_refptr<base::RefCountedString> data = new base::RefCountedString();
    ReadCallback callback = read_callbacks.back();
    AttachmentIdList ids = read_ids.back();
    read_callbacks.pop_back();
    read_ids.pop_back();

    scoped_ptr<AttachmentMap> attachments(new AttachmentMap());
    scoped_ptr<AttachmentIdList> unavailable_attachments(
        new AttachmentIdList());
    for (AttachmentIdList::const_iterator iter = ids.begin(); iter != ids.end();
         ++iter) {
      if (local_attachments.find(*iter) != local_attachments.end()) {
        Attachment attachment = Attachment::CreateWithId(*iter, data);
        attachments->insert(std::make_pair(*iter, attachment));
      } else {
        unavailable_attachments->push_back(*iter);
      }
    }
    Result result =
        unavailable_attachments->empty() ? SUCCESS : UNSPECIFIED_ERROR;

    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   result,
                   base::Passed(&attachments),
                   base::Passed(&unavailable_attachments)));
  }

  // Respond to Write request with |result|.
  void RespondToWrite(const Result& result) {
    WriteCallback callback = write_callbacks.back();
    AttachmentList attachments = write_attachments.back();
    write_callbacks.pop_back();
    write_attachments.pop_back();
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::Bind(callback, result));
  }

  std::vector<AttachmentIdList> read_ids;
  std::vector<ReadCallback> read_callbacks;
  std::vector<AttachmentList> write_attachments;
  std::vector<WriteCallback> write_callbacks;

 private:
  virtual ~MockAttachmentStore() {}

  DISALLOW_COPY_AND_ASSIGN(MockAttachmentStore);
};

class MockAttachmentDownloader
    : public AttachmentDownloader,
      public base::SupportsWeakPtr<MockAttachmentDownloader> {
 public:
  MockAttachmentDownloader() {}

  virtual void DownloadAttachment(const AttachmentId& id,
                                  const DownloadCallback& callback) OVERRIDE {
    ASSERT_TRUE(download_requests.find(id) == download_requests.end());
    download_requests.insert(std::make_pair(id, callback));
  }

  // Multiple requests to download will be active at the same time.
  // RespondToDownload should respond to only one of them.
  void RespondToDownload(const AttachmentId& id, const DownloadResult& result) {
    ASSERT_TRUE(download_requests.find(id) != download_requests.end());
    scoped_ptr<Attachment> attachment;
    if (result == DOWNLOAD_SUCCESS) {
      scoped_refptr<base::RefCountedString> data = new base::RefCountedString();
      attachment.reset(new Attachment(Attachment::CreateWithId(id, data)));
    }
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(download_requests[id], result, base::Passed(&attachment)));

    download_requests.erase(id);
  }

  std::map<AttachmentId, DownloadCallback> download_requests;

  DISALLOW_COPY_AND_ASSIGN(MockAttachmentDownloader);
};

class MockAttachmentUploader
    : public AttachmentUploader,
      public base::SupportsWeakPtr<MockAttachmentUploader> {
 public:
  MockAttachmentUploader() {}

  // AttachmentUploader implementation.
  virtual void UploadAttachment(const Attachment& attachment,
                                const UploadCallback& callback) OVERRIDE {
    const AttachmentId id = attachment.GetId();
    ASSERT_TRUE(upload_requests.find(id) == upload_requests.end());
    upload_requests.insert(std::make_pair(id, callback));
  }

  void RespondToUpload(const AttachmentId& id, const UploadResult& result) {
    ASSERT_TRUE(upload_requests.find(id) != upload_requests.end());
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(upload_requests[id], result, id));
    upload_requests.erase(id);
  }

  std::map<AttachmentId, UploadCallback> upload_requests;

  DISALLOW_COPY_AND_ASSIGN(MockAttachmentUploader);
};

class AttachmentServiceImplTest : public testing::Test,
                                  public AttachmentService::Delegate {
 protected:
  AttachmentServiceImplTest() {}

  virtual void SetUp() OVERRIDE {
    network_change_notifier_.reset(net::NetworkChangeNotifier::CreateMock());
    InitializeAttachmentService(make_scoped_ptr(new MockAttachmentUploader()),
                                make_scoped_ptr(new MockAttachmentDownloader()),
                                this);
  }

  virtual void TearDown() OVERRIDE {
    attachment_service_.reset();
    ASSERT_FALSE(attachment_store_);
    ASSERT_FALSE(attachment_uploader_);
    ASSERT_FALSE(attachment_downloader_);
  }

  // AttachmentService::Delegate implementation.
  virtual void OnAttachmentUploaded(
      const AttachmentId& attachment_id) OVERRIDE {
    on_attachment_uploaded_list_.push_back(attachment_id);
  }

  void InitializeAttachmentService(
      scoped_ptr<MockAttachmentUploader> uploader,
      scoped_ptr<MockAttachmentDownloader> downloader,
      AttachmentService::Delegate* delegate) {
    scoped_refptr<MockAttachmentStore> attachment_store(
        new MockAttachmentStore());
    attachment_store_ = attachment_store->AsWeakPtr();

    if (uploader.get()) {
      attachment_uploader_ = uploader->AsWeakPtr();
    }
    if (downloader.get()) {
      attachment_downloader_ = downloader->AsWeakPtr();
    }
    attachment_service_.reset(
        new AttachmentServiceImpl(attachment_store,
                                  uploader.PassAs<AttachmentUploader>(),
                                  downloader.PassAs<AttachmentDownloader>(),
                                  delegate,
                                  base::TimeDelta::FromMinutes(1),
                                  base::TimeDelta::FromMinutes(8)));

    scoped_ptr<base::MockTimer> timer_to_pass(
        new base::MockTimer(false, false));
    mock_timer_ = timer_to_pass.get();
    attachment_service_->SetTimerForTest(timer_to_pass.PassAs<base::Timer>());
  }

  AttachmentService* attachment_service() { return attachment_service_.get(); }

  base::MockTimer* mock_timer() { return mock_timer_; }

  AttachmentService::GetOrDownloadCallback download_callback() {
    return base::Bind(&AttachmentServiceImplTest::DownloadDone,
                      base::Unretained(this));
  }

  void DownloadDone(const AttachmentService::GetOrDownloadResult& result,
                    scoped_ptr<AttachmentMap> attachments) {
    download_results_.push_back(result);
    last_download_attachments_ = attachments.Pass();
  }

  void RunLoop() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  void RunLoopAndFireTimer() {
    RunLoop();
    if (mock_timer()->IsRunning()) {
      mock_timer()->Fire();
    }
    RunLoop();
  }

  const std::vector<AttachmentService::GetOrDownloadResult>&
  download_results() const {
    return download_results_;
  }

  const AttachmentMap& last_download_attachments() const {
    return *last_download_attachments_.get();
  }

  net::NetworkChangeNotifier* network_change_notifier() {
    return network_change_notifier_.get();
  }

  MockAttachmentStore* store() { return attachment_store_.get(); }

  MockAttachmentDownloader* downloader() {
    return attachment_downloader_.get();
  }

  MockAttachmentUploader* uploader() {
    return attachment_uploader_.get();
  }

  const std::vector<AttachmentId>& on_attachment_uploaded_list() const {
    return on_attachment_uploaded_list_;
  }

 private:
  base::MessageLoop message_loop_;
  scoped_ptr<net::NetworkChangeNotifier> network_change_notifier_;
  base::WeakPtr<MockAttachmentStore> attachment_store_;
  base::WeakPtr<MockAttachmentDownloader> attachment_downloader_;
  base::WeakPtr<MockAttachmentUploader> attachment_uploader_;
  scoped_ptr<AttachmentServiceImpl> attachment_service_;
  base::MockTimer* mock_timer_;  // not owned

  std::vector<AttachmentService::GetOrDownloadResult> download_results_;
  scoped_ptr<AttachmentMap> last_download_attachments_;
  std::vector<AttachmentId> on_attachment_uploaded_list_;
};

TEST_F(AttachmentServiceImplTest, GetStore) {
  EXPECT_EQ(store(), attachment_service()->GetStore());
}

TEST_F(AttachmentServiceImplTest, GetOrDownload_EmptyAttachmentList) {
  AttachmentIdList attachment_ids;
  attachment_service()->GetOrDownloadAttachments(attachment_ids,
                                                 download_callback());
  store()->RespondToRead(AttachmentIdSet());

  RunLoop();
  EXPECT_EQ(1U, download_results().size());
  EXPECT_EQ(0U, last_download_attachments().size());
}

TEST_F(AttachmentServiceImplTest, GetOrDownload_Local) {
  AttachmentIdList attachment_ids;
  attachment_ids.push_back(AttachmentId::Create());
  attachment_service()->GetOrDownloadAttachments(attachment_ids,
                                                 download_callback());
  AttachmentIdSet local_attachments;
  local_attachments.insert(attachment_ids[0]);
  store()->RespondToRead(local_attachments);

  RunLoop();
  EXPECT_EQ(1U, download_results().size());
  EXPECT_EQ(1U, last_download_attachments().size());
  EXPECT_TRUE(last_download_attachments().find(attachment_ids[0]) !=
              last_download_attachments().end());
}

TEST_F(AttachmentServiceImplTest, GetOrDownload_LocalRemoteUnavailable) {
  // Create attachment list with 3 ids.
  AttachmentIdList attachment_ids;
  attachment_ids.push_back(AttachmentId::Create());
  attachment_ids.push_back(AttachmentId::Create());
  attachment_ids.push_back(AttachmentId::Create());
  // Call attachment service.
  attachment_service()->GetOrDownloadAttachments(attachment_ids,
                                                 download_callback());
  // Ensure AttachmentStore is called.
  EXPECT_FALSE(store()->read_ids.empty());

  // make AttachmentStore return only attachment 0.
  AttachmentIdSet local_attachments;
  local_attachments.insert(attachment_ids[0]);
  store()->RespondToRead(local_attachments);
  RunLoop();
  // Ensure Downloader called with right attachment ids
  EXPECT_EQ(2U, downloader()->download_requests.size());

  // Make downloader return attachment 1.
  downloader()->RespondToDownload(attachment_ids[1],
                                  AttachmentDownloader::DOWNLOAD_SUCCESS);
  RunLoop();
  // Ensure consumer callback is not called.
  EXPECT_TRUE(download_results().empty());

  // Make downloader fail attachment 2.
  downloader()->RespondToDownload(
      attachment_ids[2], AttachmentDownloader::DOWNLOAD_UNSPECIFIED_ERROR);
  RunLoop();
  // Ensure callback is called
  EXPECT_FALSE(download_results().empty());
  // There should be only two attachments returned, 0 and 1.
  EXPECT_EQ(2U, last_download_attachments().size());
  EXPECT_TRUE(last_download_attachments().find(attachment_ids[0]) !=
              last_download_attachments().end());
  EXPECT_TRUE(last_download_attachments().find(attachment_ids[1]) !=
              last_download_attachments().end());
  EXPECT_TRUE(last_download_attachments().find(attachment_ids[2]) ==
              last_download_attachments().end());
}

TEST_F(AttachmentServiceImplTest, GetOrDownload_NoDownloader) {
  // No downloader.
  InitializeAttachmentService(
      make_scoped_ptr<MockAttachmentUploader>(new MockAttachmentUploader()),
      make_scoped_ptr<MockAttachmentDownloader>(NULL),
      this);

  AttachmentIdList attachment_ids;
  attachment_ids.push_back(AttachmentId::Create());
  attachment_service()->GetOrDownloadAttachments(attachment_ids,
                                                 download_callback());
  EXPECT_FALSE(store()->read_ids.empty());

  AttachmentIdSet local_attachments;
  store()->RespondToRead(local_attachments);
  RunLoop();
  ASSERT_EQ(1U, download_results().size());
  EXPECT_EQ(AttachmentService::GET_UNSPECIFIED_ERROR, download_results()[0]);
  EXPECT_TRUE(last_download_attachments().empty());
}

TEST_F(AttachmentServiceImplTest, UploadAttachments_Success) {
  AttachmentIdSet attachment_ids;
  const unsigned num_attachments = 3;
  for (unsigned i = 0; i < num_attachments; ++i) {
    attachment_ids.insert(AttachmentId::Create());
  }
  attachment_service()->UploadAttachments(attachment_ids);

  for (unsigned i = 0; i < num_attachments; ++i) {
    RunLoopAndFireTimer();
    // See that the service has issued a read for at least one of the
    // attachments.
    ASSERT_GE(store()->read_ids.size(), 1U);
    store()->RespondToRead(attachment_ids);
    RunLoop();
    ASSERT_GE(uploader()->upload_requests.size(), 1U);
    uploader()->RespondToUpload(uploader()->upload_requests.begin()->first,
                                AttachmentUploader::UPLOAD_SUCCESS);
  }
  RunLoop();
  ASSERT_EQ(0U, store()->read_ids.size());
  ASSERT_EQ(0U, uploader()->upload_requests.size());

  // See that all the attachments were uploaded.
  ASSERT_EQ(attachment_ids.size(), on_attachment_uploaded_list().size());
  AttachmentIdSet::const_iterator iter = attachment_ids.begin();
  const AttachmentIdSet::const_iterator end = attachment_ids.end();
  for (iter = attachment_ids.begin(); iter != end; ++iter) {
    EXPECT_THAT(on_attachment_uploaded_list(), testing::Contains(*iter));
  }
}

TEST_F(AttachmentServiceImplTest, UploadAttachments_Success_NoDelegate) {
  InitializeAttachmentService(make_scoped_ptr(new MockAttachmentUploader()),
                              make_scoped_ptr(new MockAttachmentDownloader()),
                              NULL);  // No delegate.

  AttachmentIdSet attachment_ids;
  attachment_ids.insert(AttachmentId::Create());
  attachment_service()->UploadAttachments(attachment_ids);
  RunLoopAndFireTimer();
  ASSERT_EQ(1U, store()->read_ids.size());
  ASSERT_EQ(0U, uploader()->upload_requests.size());
  store()->RespondToRead(attachment_ids);
  RunLoop();
  ASSERT_EQ(0U, store()->read_ids.size());
  ASSERT_EQ(1U, uploader()->upload_requests.size());
  uploader()->RespondToUpload(*attachment_ids.begin(),
                              AttachmentUploader::UPLOAD_SUCCESS);
  RunLoop();
  ASSERT_TRUE(on_attachment_uploaded_list().empty());
}

TEST_F(AttachmentServiceImplTest, UploadAttachments_SomeMissingFromStore) {
  AttachmentIdSet attachment_ids;
  attachment_ids.insert(AttachmentId::Create());
  attachment_ids.insert(AttachmentId::Create());
  attachment_service()->UploadAttachments(attachment_ids);
  RunLoopAndFireTimer();
  ASSERT_GE(store()->read_ids.size(), 1U);

  ASSERT_EQ(0U, uploader()->upload_requests.size());
  store()->RespondToRead(attachment_ids);
  RunLoop();
  ASSERT_EQ(1U, uploader()->upload_requests.size());

  uploader()->RespondToUpload(uploader()->upload_requests.begin()->first,
                              AttachmentUploader::UPLOAD_SUCCESS);
  RunLoopAndFireTimer();
  ASSERT_EQ(1U, on_attachment_uploaded_list().size());
  ASSERT_GE(store()->read_ids.size(), 1U);
  // Not found!
  store()->RespondToRead(AttachmentIdSet());
  RunLoop();
  // No upload requests since the read failed.
  ASSERT_EQ(0U, uploader()->upload_requests.size());
}

TEST_F(AttachmentServiceImplTest, UploadAttachments_AllMissingFromStore) {
  AttachmentIdSet attachment_ids;
  const unsigned num_attachments = 2;
  for (unsigned i = 0; i < num_attachments; ++i) {
    attachment_ids.insert(AttachmentId::Create());
  }
  attachment_service()->UploadAttachments(attachment_ids);

  for (unsigned i = 0; i < num_attachments; ++i) {
    RunLoopAndFireTimer();
    ASSERT_GE(store()->read_ids.size(), 1U);
    // None found!
    store()->RespondToRead(AttachmentIdSet());
  }
  RunLoop();

  // Nothing uploaded.
  EXPECT_EQ(0U, uploader()->upload_requests.size());
  // See that the delegate was never called.
  ASSERT_EQ(0U, on_attachment_uploaded_list().size());
}

TEST_F(AttachmentServiceImplTest, UploadAttachments_NoUploader) {
  InitializeAttachmentService(make_scoped_ptr<MockAttachmentUploader>(NULL),
                              make_scoped_ptr(new MockAttachmentDownloader()),
                              this);

  AttachmentIdSet attachment_ids;
  attachment_ids.insert(AttachmentId::Create());
  attachment_service()->UploadAttachments(attachment_ids);
  RunLoop();
  EXPECT_EQ(0U, store()->read_ids.size());
  ASSERT_EQ(0U, on_attachment_uploaded_list().size());
}

// Upload three attachments.  For one of them, server responds with error.
TEST_F(AttachmentServiceImplTest, UploadAttachments_OneUploadFails) {
  AttachmentIdSet attachment_ids;
  const unsigned num_attachments = 3;
  for (unsigned i = 0; i < num_attachments; ++i) {
    attachment_ids.insert(AttachmentId::Create());
  }
  attachment_service()->UploadAttachments(attachment_ids);

  for (unsigned i = 0; i < 3; ++i) {
    RunLoopAndFireTimer();
    ASSERT_GE(store()->read_ids.size(), 1U);
    store()->RespondToRead(attachment_ids);
    RunLoop();
    ASSERT_EQ(1U, uploader()->upload_requests.size());
    AttachmentUploader::UploadResult result =
        AttachmentUploader::UPLOAD_SUCCESS;
    // Fail the 2nd one.
    if (i == 2U) {
      result = AttachmentUploader::UPLOAD_UNSPECIFIED_ERROR;
    } else {
      result = AttachmentUploader::UPLOAD_SUCCESS;
    }
    uploader()->RespondToUpload(uploader()->upload_requests.begin()->first,
                                result);
    RunLoop();
  }
  ASSERT_EQ(2U, on_attachment_uploaded_list().size());
}

// Attempt an upload, respond with transient error to trigger backoff, issue
// network disconnect/connect events and see that backoff is cleared.
TEST_F(AttachmentServiceImplTest,
       UploadAttachments_ResetBackoffAfterNetworkChange) {
  AttachmentIdSet attachment_ids;
  attachment_ids.insert(AttachmentId::Create());
  attachment_service()->UploadAttachments(attachment_ids);

  RunLoopAndFireTimer();
  ASSERT_EQ(1U, store()->read_ids.size());
  store()->RespondToRead(attachment_ids);
  RunLoop();
  ASSERT_EQ(1U, uploader()->upload_requests.size());

  uploader()->RespondToUpload(uploader()->upload_requests.begin()->first,
                              AttachmentUploader::UPLOAD_TRANSIENT_ERROR);
  RunLoop();

  // See that we are in backoff.
  ASSERT_TRUE(mock_timer()->IsRunning());
  ASSERT_GT(mock_timer()->GetCurrentDelay(), base::TimeDelta());

  // Issue a network disconnect event.
  network_change_notifier()->NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_NONE);
  RunLoop();

  // Still in backoff.
  ASSERT_TRUE(mock_timer()->IsRunning());
  ASSERT_GT(mock_timer()->GetCurrentDelay(), base::TimeDelta());

  // Issue a network connect event.
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_WIFI);
  RunLoop();

  // No longer in backoff.
  ASSERT_TRUE(mock_timer()->IsRunning());
  ASSERT_EQ(base::TimeDelta(), mock_timer()->GetCurrentDelay());
}

}  // namespace syncer
