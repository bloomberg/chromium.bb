// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/attachments/attachment_service_impl.h"

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "sync/internal_api/public/attachments/fake_attachment_downloader.h"
#include "sync/internal_api/public/attachments/fake_attachment_uploader.h"
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
    scoped_ptr<MockAttachmentStore> attachment_store(new MockAttachmentStore());
    attachment_store_ = attachment_store->AsWeakPtr();

    if (uploader.get()) {
      attachment_uploader_ = uploader->AsWeakPtr();
    }
    if (downloader.get()) {
      attachment_downloader_ = downloader->AsWeakPtr();
    }
    attachment_service_.reset(
        new AttachmentServiceImpl(attachment_store.PassAs<AttachmentStore>(),
                                  uploader.PassAs<AttachmentUploader>(),
                                  downloader.PassAs<AttachmentDownloader>(),
                                  delegate));
  }

  AttachmentService* attachment_service() { return attachment_service_.get(); }

  AttachmentService::GetOrDownloadCallback download_callback() {
    return base::Bind(&AttachmentServiceImplTest::DownloadDone,
                      base::Unretained(this));
  }

  AttachmentService::StoreCallback store_callback() {
    return base::Bind(&AttachmentServiceImplTest::StoreDone,
                      base::Unretained(this));
  }

  void DownloadDone(const AttachmentService::GetOrDownloadResult& result,
                    scoped_ptr<AttachmentMap> attachments) {
    download_results_.push_back(result);
    last_download_attachments_ = attachments.Pass();
  }

  void StoreDone(const AttachmentService::StoreResult& result) {
    store_results_.push_back(result);
  }

  void RunLoop() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

  const std::vector<AttachmentService::GetOrDownloadResult>&
  download_results() const {
    return download_results_;
  }

  const AttachmentMap& last_download_attachments() const {
    return *last_download_attachments_.get();
  }

  const std::vector<AttachmentService::StoreResult>& store_results() const {
    return store_results_;
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
  base::WeakPtr<MockAttachmentStore> attachment_store_;
  base::WeakPtr<MockAttachmentDownloader> attachment_downloader_;
  base::WeakPtr<MockAttachmentUploader> attachment_uploader_;
  scoped_ptr<AttachmentService> attachment_service_;

  std::vector<AttachmentService::GetOrDownloadResult> download_results_;
  scoped_ptr<AttachmentMap> last_download_attachments_;
  std::vector<AttachmentId> on_attachment_uploaded_list_;

  std::vector<AttachmentService::StoreResult> store_results_;
};

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

TEST_F(AttachmentServiceImplTest, StoreAttachments_Success) {
  scoped_refptr<base::RefCountedString> data = new base::RefCountedString();
  Attachment attachment(Attachment::Create(data));
  AttachmentList attachments;
  attachments.push_back(attachment);
  attachment_service()->StoreAttachments(attachments, store_callback());
  EXPECT_EQ(1U, store()->write_attachments.size());
  EXPECT_EQ(1U, uploader()->upload_requests.size());

  store()->RespondToWrite(AttachmentStore::SUCCESS);
  uploader()->RespondToUpload(attachment.GetId(),
                              AttachmentUploader::UPLOAD_SUCCESS);
  RunLoop();
  ASSERT_EQ(1U, store_results().size());
  EXPECT_EQ(AttachmentService::STORE_SUCCESS, store_results()[0]);
  ASSERT_EQ(1U, on_attachment_uploaded_list().size());
  EXPECT_EQ(attachment.GetId(), on_attachment_uploaded_list()[0]);
}

TEST_F(AttachmentServiceImplTest,
       StoreAttachments_StoreFailsWithUnspecifiedError) {
  scoped_refptr<base::RefCountedString> data = new base::RefCountedString();
  Attachment attachment(Attachment::Create(data));
  AttachmentList attachments;
  attachments.push_back(attachment);
  attachment_service()->StoreAttachments(attachments, store_callback());
  EXPECT_EQ(1U, store()->write_attachments.size());
  EXPECT_EQ(1U, uploader()->upload_requests.size());

  store()->RespondToWrite(AttachmentStore::UNSPECIFIED_ERROR);
  uploader()->RespondToUpload(attachment.GetId(),
                              AttachmentUploader::UPLOAD_SUCCESS);
  RunLoop();
  ASSERT_EQ(1U, store_results().size());
  EXPECT_EQ(AttachmentService::STORE_UNSPECIFIED_ERROR, store_results()[0]);
  ASSERT_EQ(1U, on_attachment_uploaded_list().size());
  EXPECT_EQ(attachment.GetId(), on_attachment_uploaded_list()[0]);
}

TEST_F(AttachmentServiceImplTest,
       StoreAttachments_UploadFailsWithUnspecifiedError) {
  scoped_refptr<base::RefCountedString> data = new base::RefCountedString();
  Attachment attachment(Attachment::Create(data));
  AttachmentList attachments;
  attachments.push_back(attachment);
  attachment_service()->StoreAttachments(attachments, store_callback());
  EXPECT_EQ(1U, store()->write_attachments.size());
  EXPECT_EQ(1U, uploader()->upload_requests.size());

  store()->RespondToWrite(AttachmentStore::SUCCESS);
  uploader()->RespondToUpload(attachment.GetId(),
                              AttachmentUploader::UPLOAD_UNSPECIFIED_ERROR);
  RunLoop();
  ASSERT_EQ(1U, store_results().size());
  // Even though the upload failed, the Store operation is successful.
  EXPECT_EQ(AttachmentService::STORE_SUCCESS, store_results()[0]);
  EXPECT_TRUE(on_attachment_uploaded_list().empty());
}

TEST_F(AttachmentServiceImplTest, StoreAttachments_NoDelegate) {
  InitializeAttachmentService(make_scoped_ptr(new MockAttachmentUploader()),
                              make_scoped_ptr(new MockAttachmentDownloader()),
                              NULL);  // No delegate.

  scoped_refptr<base::RefCountedString> data = new base::RefCountedString();
  Attachment attachment(Attachment::Create(data));
  AttachmentList attachments;
  attachments.push_back(attachment);
  attachment_service()->StoreAttachments(attachments, store_callback());
  EXPECT_EQ(1U, store()->write_attachments.size());
  EXPECT_EQ(1U, uploader()->upload_requests.size());

  store()->RespondToWrite(AttachmentStore::SUCCESS);
  uploader()->RespondToUpload(attachment.GetId(),
                              AttachmentUploader::UPLOAD_SUCCESS);
  RunLoop();
  ASSERT_EQ(1U, store_results().size());
  EXPECT_EQ(AttachmentService::STORE_SUCCESS, store_results()[0]);
  EXPECT_TRUE(on_attachment_uploaded_list().empty());
}

TEST_F(AttachmentServiceImplTest, StoreAttachments_NoUploader) {
  // No uploader.
  InitializeAttachmentService(make_scoped_ptr<MockAttachmentUploader>(NULL),
                              make_scoped_ptr(new MockAttachmentDownloader()),
                              this);

  scoped_refptr<base::RefCountedString> data = new base::RefCountedString();
  Attachment attachment(Attachment::Create(data));
  AttachmentList attachments;
  attachments.push_back(attachment);
  attachment_service()->StoreAttachments(attachments, store_callback());
  EXPECT_EQ(1U, store()->write_attachments.size());

  store()->RespondToWrite(AttachmentStore::SUCCESS);
  RunLoop();
  ASSERT_EQ(1U, store_results().size());
  EXPECT_EQ(AttachmentService::STORE_SUCCESS, store_results()[0]);
  EXPECT_TRUE(on_attachment_uploaded_list().empty());
}

}  // namespace syncer
